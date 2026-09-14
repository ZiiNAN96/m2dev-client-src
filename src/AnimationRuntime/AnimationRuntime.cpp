#include "AnimationRuntime.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <utility>

namespace AnimationRuntime
{
namespace
{
std::atomic<std::uint64_t> liveSkeletons{0};
std::atomic<std::uint64_t> liveClips{0};

template <std::size_t N> bool Finite(const std::array<float, N>& values) noexcept
{
    return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
}

bool Normalize(Quaternion& value) noexcept
{
    if (!Finite(value)) return false;
    double norm = 0;
    for (float component : value) norm += static_cast<double>(component) * component;
    if (norm < 1e-16) return false;
    const double inverse = 1.0 / std::sqrt(norm);
    for (float& component : value) component = static_cast<float>(component * inverse);
    return true;
}

bool ValidTransform(const LocalTransform& value) noexcept
{
    Quaternion rotation = value.rotation;
    return Finite(value.translation) && Finite(value.scaleShear) && Normalize(rotation);
}

template <std::size_t N> std::array<float, N> Lerp(const std::array<float, N>& first,
    const std::array<float, N>& second, float weight) noexcept
{
    std::array<float, N> result{};
    for (std::size_t i = 0; i < N; ++i)
        result[i] = static_cast<float>((1.0 - weight) * first[i] + static_cast<double>(weight) * second[i]);
    return result;
}

Quaternion Nlerp(const Quaternion& first, const Quaternion& second, float weight) noexcept
{
    float dot = 0;
    for (std::size_t i = 0; i < 4; ++i) dot += first[i] * second[i];
    Quaternion adjusted = second;
    if (dot < 0) for (float& component : adjusted) component = -component;
    Quaternion result = Lerp(first, adjusted, weight);
    Normalize(result);
    return result;
}

template <std::size_t N> std::array<float, N> SampleTrack(
    const Track<std::array<float, N>>& track, double time, const std::array<float, N>& fallback) noexcept
{
    const auto& keys = track.keys;
    if (keys.empty()) return fallback;
    if (time <= keys.front().time) return keys.front().value;
    if (time >= keys.back().time) return keys.back().value;
    const auto next = std::upper_bound(keys.begin(), keys.end(), time,
        [](double value, const auto& key) { return value < key.time; });
    const auto& previous = *(next - 1);
    if (track.interpolation == Interpolation::Step) return previous.value;
    const float weight = static_cast<float>((time - previous.time) / (next->time - previous.time));
    if constexpr (N == 4) return Nlerp(previous.value, next->value, weight);
    else return Lerp(previous.value, next->value, weight);
}

template <std::size_t N> bool ValidateTrack(Track<std::array<float, N>>& track,
    double duration, std::string& error)
{
    if (track.interpolation != Interpolation::Linear && track.interpolation != Interpolation::Step)
    {
        error = "unsupported interpolation";
        return false;
    }
    double previous = -1;
    for (auto& key : track.keys)
    {
        if (!std::isfinite(key.time) || key.time < 0 || key.time > duration || key.time <= previous)
        {
            error = "key times must be finite, strictly increasing, and within clip duration";
            return false;
        }
        if (!Finite(key.value))
        {
            error = "non-finite key value";
            return false;
        }
        if constexpr (N == 4)
        {
            if (!Normalize(key.value))
            {
                error = "invalid rotation key quaternion";
                return false;
            }
        }
        previous = key.time;
    }
    return true;
}
}

RuntimeSkeleton::RuntimeSkeleton() { ++liveSkeletons; }
RuntimeSkeleton::RuntimeSkeleton(const RuntimeSkeleton& other)
    : bones_(other.bones_), evaluationOrder_(other.evaluationOrder_), bindingId_(other.bindingId_)
{ ++liveSkeletons; }
RuntimeSkeleton::RuntimeSkeleton(RuntimeSkeleton&& other) noexcept
    : bones_(std::move(other.bones_)), evaluationOrder_(std::move(other.evaluationOrder_)),
      bindingId_(std::exchange(other.bindingId_, 0))
{ ++liveSkeletons; }
RuntimeSkeleton::~RuntimeSkeleton() { --liveSkeletons; }

bool RuntimeSkeleton::Initialize(std::vector<SkeletonBone> bones, std::string& error, SourceSemantics semantics)
{
    if (bones.empty() || bones.size() > 65536)
    {
        error = "skeleton requires 1..65536 bones";
        return false;
    }
    std::unordered_set<std::string> names;
    std::size_t roots = 0;
    std::uint64_t bindingId = 14695981039346656037ULL;
    const auto hashByte = [&bindingId](unsigned char value) { bindingId = (bindingId ^ value) * 1099511628211ULL; };
    for (std::size_t i = 0; i < bones.size(); ++i)
    {
        auto& bone = bones[i];
        if (bone.name.empty() || (!names.insert(bone.name).second && semantics == SourceSemantics::NamedTree))
        {
            error = "bone names must be nonempty and unique";
            return false;
        }
        if (bone.parent < -1 || bone.parent >= static_cast<std::int32_t>(bones.size()) ||
            bone.parent == static_cast<std::int32_t>(i))
        {
            error = "invalid or self parent for bone " + bone.name;
            return false;
        }
        if (bone.parent == -1) ++roots;
        if (!Finite(bone.localBind.translation) || !Finite(bone.localBind.scaleShear) ||
            !Normalize(bone.localBind.rotation) || !Finite(bone.inverseBind))
        {
            error = "invalid bind transform for bone " + bone.name;
            return false;
        }
        for (unsigned char value : bone.name) hashByte(value);
        hashByte(0);
        const auto parent = static_cast<std::uint32_t>(bone.parent);
        for (unsigned shift = 0; shift < 32; shift += 8) hashByte(static_cast<unsigned char>(parent >> shift));
    }
    if (roots == 0 || (roots != 1 && semantics == SourceSemantics::NamedTree))
    {
        error = "skeleton requires exactly one connected root";
        return false;
    }
    std::vector<unsigned char> state(bones.size(), 0);
    std::vector<std::size_t> depth(bones.size(), 0);
    std::vector<std::uint32_t> order;
    std::vector<std::uint32_t> path;
    order.reserve(bones.size());
    path.reserve(MaximumDepth);
    for (std::size_t i = 0; i < bones.size(); ++i)
    {
        if (state[i] == 2) continue;
        path.clear();
        auto cursor = static_cast<std::int32_t>(i);
        while (cursor >= 0 && state[static_cast<std::size_t>(cursor)] == 0)
        {
            const auto index = static_cast<std::size_t>(cursor);
            state[index] = 1;
            path.push_back(static_cast<std::uint32_t>(index));
            if (path.size() > MaximumDepth)
            {
                error = "skeleton hierarchy exceeds maximum depth";
                return false;
            }
            cursor = bones[index].parent;
        }
        if (cursor >= 0 && state[static_cast<std::size_t>(cursor)] == 1)
        {
            error = "skeleton hierarchy contains a cycle";
            return false;
        }
        std::size_t currentDepth = cursor < 0 ? 0 : depth[static_cast<std::size_t>(cursor)];
        for (auto position = path.rbegin(); position != path.rend(); ++position)
        {
            if (++currentDepth > MaximumDepth)
            {
                error = "skeleton hierarchy exceeds maximum depth";
                return false;
            }
            depth[*position] = currentDepth;
            state[*position] = 2;
            order.push_back(*position);
        }
    }
    bones_ = std::move(bones);
    evaluationOrder_ = std::move(order);
    bindingId_ = bindingId == 0 ? 1 : bindingId;
    error.clear();
    return true;
}

std::int32_t RuntimeSkeleton::FindBone(std::string_view name) const noexcept
{
    for (std::size_t i = 0; i < bones_.size(); ++i)
        if (bones_[i].name == name) return static_cast<std::int32_t>(i);
    return -1;
}

RuntimeAnimationClip::RuntimeAnimationClip() { ++liveClips; }
RuntimeAnimationClip::RuntimeAnimationClip(const RuntimeAnimationClip& other)
    : name_(other.name_), duration_(other.duration_), looping_(other.looping_),
      tracks_(other.tracks_), bindingId_(other.bindingId_)
{ ++liveClips; }
RuntimeAnimationClip::RuntimeAnimationClip(RuntimeAnimationClip&& other) noexcept
    : name_(std::move(other.name_)), duration_(other.duration_), looping_(other.looping_),
      tracks_(std::move(other.tracks_)), bindingId_(std::exchange(other.bindingId_, 0))
{ ++liveClips; }
RuntimeAnimationClip::~RuntimeAnimationClip() { --liveClips; }

bool RuntimeAnimationClip::Initialize(std::string name, double duration, bool looping,
    std::vector<AnimationTrack> tracks, const RuntimeSkeleton& skeleton, std::string& error)
{
    if (skeleton.Bones().empty() || !std::isfinite(duration) || duration < 0 || name.empty())
    {
        error = "clip requires a validated skeleton, name and finite nonnegative duration";
        return false;
    }
    std::vector<bool> bound(skeleton.Bones().size(), false);
    for (auto& track : tracks)
    {
        if (track.targetBone >= bound.size() || bound[track.targetBone])
        {
            error = "track target is unknown or bound more than once";
            return false;
        }
        bound[track.targetBone] = true;
        if (!ValidateTrack(track.translation, duration, error) ||
            !ValidateTrack(track.rotation, duration, error) || !ValidateTrack(track.scaleShear, duration, error))
            return false;
    }
    name_ = std::move(name);
    duration_ = duration;
    looping_ = looping;
    tracks_ = std::move(tracks);
    bindingId_ = skeleton.BindingId();
    error.clear();
    return true;
}

Matrix Multiply(const Matrix& left, const Matrix& right) noexcept
{
    Matrix result{};
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            for (std::size_t inner = 0; inner < 4; ++inner)
                result[row * 4 + column] += left[row * 4 + inner] * right[inner * 4 + column];
    return result;
}

Matrix LocalMatrix(const LocalTransform& transform) noexcept
{
    Quaternion q = transform.rotation;
    if (!Normalize(q)) q = {0, 0, 0, 1};
    const float x = q[0], y = q[1], z = q[2], w = q[3];
    const ScaleShear rotation{
        1 - 2 * (y * y + z * z), 2 * (x * y + z * w), 2 * (x * z - y * w),
        2 * (x * y - z * w), 1 - 2 * (x * x + z * z), 2 * (y * z + x * w),
        2 * (x * z + y * w), 2 * (y * z - x * w), 1 - 2 * (x * x + y * y)};
    Matrix matrix = IdentityMatrix();
    for (std::size_t row = 0; row < 3; ++row)
        for (std::size_t column = 0; column < 3; ++column)
        {
            matrix[row * 4 + column] = 0;
            for (std::size_t inner = 0; inner < 3; ++inner)
                matrix[row * 4 + column] += transform.scaleShear[row * 3 + inner] * rotation[inner * 3 + column];
        }
    for (std::size_t i = 0; i < 3; ++i) matrix[12 + i] = transform.translation[i];
    return matrix;
}

double ResolveTime(double time, double duration, bool looping) noexcept
{
    if (!std::isfinite(time) || !std::isfinite(duration) || duration < 0)
        return std::numeric_limits<double>::quiet_NaN();
    if (duration == 0) return 0;
    if (!looping) return std::clamp(time, 0.0, duration);
    const double wrapped = std::fmod(time, duration);
    return wrapped < 0 ? wrapped + duration : wrapped;
}

bool Sample(const RuntimeSkeleton& skeleton, const RuntimeAnimationClip& clip,
    double time, TimeMode mode, AnimationPose& output) noexcept
{
    if (skeleton.Bones().empty() || clip.BindingId() == 0 || skeleton.BindingId() != clip.BindingId() ||
        output.localTransforms.size() != skeleton.Bones().size() ||
        (mode != TimeMode::Clip && mode != TimeMode::Clamp && mode != TimeMode::Loop)) return false;
    time = ResolveTime(time, clip.Duration(), mode == TimeMode::Loop || (mode == TimeMode::Clip && clip.Looping()));
    if (!std::isfinite(time)) return false;
    for (std::size_t i = 0; i < skeleton.Bones().size(); ++i)
        output.localTransforms[i] = skeleton.Bones()[i].localBind;
    for (const auto& track : clip.Tracks())
    {
        auto& transform = output.localTransforms[track.targetBone];
        transform.translation = SampleTrack(track.translation, time, transform.translation);
        transform.rotation = SampleTrack(track.rotation, time, transform.rotation);
        transform.scaleShear = SampleTrack(track.scaleShear, time, transform.scaleShear);
    }
    return true;
}

bool Evaluate(const RuntimeSkeleton& skeleton, const AnimationPose& pose,
    std::span<Matrix> modelMatrices, const Matrix* attachmentParent) noexcept
{
    const auto& bones = skeleton.Bones();
    if (bones.empty() || pose.localTransforms.size() != bones.size() || modelMatrices.size() != bones.size()) return false;
    const Matrix rootParent = attachmentParent ? *attachmentParent : IdentityMatrix();
    if (!Finite(rootParent)) return false;
    for (const auto& transform : pose.localTransforms) if (!ValidTransform(transform)) return false;
    for (std::uint32_t i : skeleton.EvaluationOrder())
    {
        const auto parent = bones[i].parent;
        modelMatrices[i] = Multiply(LocalMatrix(pose.localTransforms[i]),
            parent < 0 ? rootParent : modelMatrices[static_cast<std::size_t>(parent)]);
        if (!Finite(modelMatrices[i])) return false;
    }
    return true;
}

bool BuildPalette(const RuntimeSkeleton& skeleton, std::span<const Matrix> modelMatrices,
    std::span<Matrix> palette) noexcept
{
    if (skeleton.Bones().empty() || modelMatrices.size() != skeleton.Bones().size() || palette.size() != modelMatrices.size())
        return false;
    for (const Matrix& matrix : modelMatrices) if (!Finite(matrix)) return false;
    for (std::size_t i = 0; i < palette.size(); ++i)
    {
        palette[i] = Multiply(skeleton.Bones()[i].inverseBind, modelMatrices[i]);
        if (!Finite(palette[i])) return false;
    }
    return true;
}

bool Blend(const AnimationPose& first, const AnimationPose& second, float weight, AnimationPose& output) noexcept
{
    if (!std::isfinite(weight) || first.localTransforms.empty() ||
        first.localTransforms.size() != second.localTransforms.size() ||
        first.localTransforms.size() != output.localTransforms.size()) return false;
    for (std::size_t i = 0; i < first.localTransforms.size(); ++i)
        if (!ValidTransform(first.localTransforms[i]) || !ValidTransform(second.localTransforms[i])) return false;
    weight = std::clamp(weight, 0.0f, 1.0f);
    for (std::size_t i = 0; i < first.localTransforms.size(); ++i)
    {
        const auto& a = first.localTransforms[i];
        const auto& b = second.localTransforms[i];
        Quaternion qa = a.rotation, qb = b.rotation;
        Normalize(qa);
        Normalize(qb);
        LocalTransform result;
        result.translation = Lerp(a.translation, b.translation, weight);
        result.rotation = Nlerp(qa, qb, weight);
        result.scaleShear = Lerp(a.scaleShear, b.scaleShear, weight);
        output.localTransforms[i] = result;
    }
    return true;
}

bool PlaybackClock::Advance(double deltaSeconds) noexcept
{
    if (!std::isfinite(deltaSeconds) || !std::isfinite(time) || !std::isfinite(rate)) return false;
    if (paused) return true;
    const double next = time + deltaSeconds * rate;
    if (!std::isfinite(next)) return false;
    time = next;
    return true;
}

LifetimeCounts GetLifetimeCounts() noexcept { return {liveSkeletons.load(), liveClips.load()}; }
}
