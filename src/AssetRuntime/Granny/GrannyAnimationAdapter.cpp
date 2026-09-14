#include "GrannyAnimationAdapter.h"
#include "GrannyInterop.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <chrono>
#include <limits>
#include <unordered_set>
#include <utility>

namespace AssetRuntime::GrannyAnimationAdapter
{
namespace
{
using RuntimeSkeleton = AnimationRuntime::RuntimeSkeleton;
using RuntimeClip = AnimationRuntime::RuntimeAnimationClip;
// Refining each bone independently avoids re-sampling every constant/slow bone
// whenever one curve needs more keys. Fixed scratch removes per-sample heaps.
struct Pose
{
    std::array<AnimationRuntime::LocalTransform, 1> localTransforms;
};
using Fingerprint = std::array<std::uint8_t, 32>;
struct ContentKey
{
    Fingerprint model, animation;
    std::size_t modelIndex{}, animationIndex{};
    std::uint64_t bindingId{};
    bool operator==(const ContentKey&) const = default;
};
struct RetainedClip { ContentKey key; std::uint64_t lastUsed{}; std::shared_ptr<const ImportedAnimation> runtime; };
struct LiveClip { ContentKey key; std::weak_ptr<const ImportedAnimation> runtime; };
// Only immutable neutral imports and copied content keys survive source unload.
// No source document, skeleton owner or SDK pointer is retained here.
std::vector<RetainedClip> retainedClips;
std::vector<LiveClip> liveClips;
std::uint64_t cacheClock{};
constexpr std::size_t maximumRetainedEntries = 4096;
constexpr std::size_t maximumModelEntries = 1024;
constexpr std::size_t maximumLiveEntries = 8192;

bool SameModel(const ContentKey& key, const Fingerprint& model, std::size_t modelIndex)
{
    return key.model == model && key.modelIndex == modelIndex;
}

bool Retains(const ImportedAnimation* runtime, const ContentKey* model = nullptr)
{
    return std::any_of(retainedClips.begin(), retainedClips.end(), [&](const auto& entry) {
        return entry.runtime.get() == runtime && (!model || SameModel(entry.key, model->model, model->modelIndex));
    });
}

std::size_t ModelKeyBytes(const Fingerprint& model, std::size_t modelIndex)
{
    std::unordered_set<const ImportedAnimation*> counted;
    std::size_t result = 0;
    for (const auto& entry : retainedClips)
        if (SameModel(entry.key, model, modelIndex) && counted.insert(entry.runtime.get()).second) result += entry.runtime->keyBytes;
    return result;
}

std::size_t ModelEntryCount(const ContentKey& key)
{
    return static_cast<std::size_t>(std::count_if(retainedClips.begin(), retainedClips.end(),
        [&](const auto& entry) { return SameModel(entry.key, key.model, key.modelIndex); }));
}

void RemoveCachedClip(std::size_t index, bool eviction)
{
    const auto* runtime = retainedClips[index].runtime.get();
    const auto bytes = runtime->keyBytes;
    retainedClips.erase(retainedClips.begin() + index);
    // Aliased entries count their underlying key allocation only once.
    if (!Retains(runtime)) retainedImportKeyBytes -= bytes;
    if (eviction) ++importCacheEvictions;
}

std::size_t OldestClip(const ContentKey* model = nullptr)
{
    std::size_t result = retainedClips.size();
    for (std::size_t index = 0; index < retainedClips.size(); ++index)
        if ((!model || SameModel(retainedClips[index].key, model->model, model->modelIndex)) &&
            (result == retainedClips.size() || retainedClips[index].lastUsed < retainedClips[result].lastUsed)) result = index;
    return result;
}

std::optional<ContentKey> MakeContentKey(const RuntimeImportCache& cache,
    const AnimationHandle& animation, std::size_t modelIndex, std::uint64_t bindingId)
{
    const auto animationFingerprint = GrannyInterop::GetSourceFingerprint(animation.GetDocument());
    if (!cache.modelFingerprint || !animationFingerprint) return {};
    return ContentKey{*cache.modelFingerprint, *animationFingerprint, modelIndex, animation.Index(), bindingId};
}

std::size_t KeyBytes(const ImportedAnimation& animation)
{
    std::size_t bytes = 0;
    for (const auto& clip : animation.boundaryClips) if (clip)
        for (const auto& track : clip->Tracks()) {
            bytes += track.translation.keys.capacity() * sizeof(AnimationRuntime::Keyframe<AnimationRuntime::Vector3>);
            bytes += track.rotation.keys.capacity() * sizeof(AnimationRuntime::Keyframe<AnimationRuntime::Quaternion>);
            bytes += track.scaleShear.keys.capacity() * sizeof(AnimationRuntime::Keyframe<AnimationRuntime::ScaleShear>);
        }
    return bytes;
}

AnimationRuntime::LocalTransform CopyTransform(const granny_transform& source)
{
    AnimationRuntime::LocalTransform result;
    if (source.Flags & 1) std::copy_n(source.Position, 3, result.translation.begin());
    if (source.Flags & 2) std::copy_n(source.Orientation, 4, result.rotation.begin());
    if (source.Flags & 4) std::copy_n(&source.ScaleShear[0][0], 9, result.scaleShear.begin());
    return result;
}

bool InventoryCurve(const granny_curve2& curve, const char* bone, int expectedDimension,
    ImportedAnimation& output, std::string& error)
{
    if (!curve.CurveData.Object || !GrannyCurveFormatIsInitializedCorrectly(&curve, true)) {
        error = "invalid source curve storage";
        return false;
    }
    const auto* header = static_cast<const granny_curve_data_header*>(curve.CurveData.Object);
    CurveInfo info{bone ? bone : "", GrannyCurveGetDimension(&curve), GrannyCurveGetDegree(&curve),
        header->Format, GrannyCurveGetKnotCount(&curve), GrannyCurveIsKeyframed(&curve), GrannyCurveIsIdentity(&curve)};
    output.curves.push_back(info);
    // F1-X original assets use DaK32fC32f, including identity/constant curves,
    // linear scales and quadratic P/Q splines. Other formats are explicit.
    if (info.format != 1 || info.keyframed || info.degree < 0 || info.degree > 2 ||
        (!info.identity && info.dimension != expectedDimension) || info.knotCount < 0 || info.knotCount > 65536) {
        error = "unsupported source curve: bone=" + info.bone + " format=" + std::to_string(info.format) +
            " degree=" + std::to_string(info.degree) + " dimension=" + std::to_string(info.dimension);
        return false;
    }
    return true;
}

struct ImportSampler
{
    granny_model_instance* model{};
    granny_local_pose* local{};
    granny_control* control{};
    float duration{};
    unsigned boundary{};
    std::size_t boneCount{}, samples{}, firstBone{}, sourceBoneCount{}, sampledBones{};
    std::string failure;
    AnimationRuntime::AnimationPose blendFirst, blendSecond, blendResult;
    ~ImportSampler()
    {
        if (model) GrannyFreeModelInstance(model);
        if (local) GrannyFreeLocalPose(local);
    }
    bool Initialize(granny_model* source, granny_animation* animation, unsigned state)
    {
        duration = animation->Duration;
        boundary = state;
        boneCount = sourceBoneCount = static_cast<std::size_t>(source->Skeleton->BoneCount);
        model = GrannyInstantiateModel(source);
        local = GrannyNewLocalPose(static_cast<int>(boneCount));
        if (!model || !local) return false;
        blendFirst.Prepare(1); blendSecond.Prepare(1); blendResult.Prepare(1);
        control = GrannyPlayControlledAnimation(0, animation, model);
        if (!control) return false;
        // 0: single clamped cycle, 1: final cycle of two, 2: first infinite
        // cycle, 3: interior infinite cycle. No source curve read per frame.
        GrannySetControlLoopCount(control, state == 0 ? 1 : state == 1 ? 2 : 0);
        GrannySetControlEaseIn(control, false);
        GrannySetControlEaseOut(control, false);
        GrannySetControlSpeed(control, 1);
        GrannyFreeControlOnceUnused(control);
        return true;
    }
    bool Blend(const Pose& first, const Pose& second, float weight, Pose& output)
    {
        blendFirst.localTransforms[0] = first.localTransforms[0];
        blendSecond.localTransforms[0] = second.localTransforms[0];
        if (!AnimationRuntime::Blend(blendFirst, blendSecond, weight, blendResult)) return false;
        output.localTransforms[0] = blendResult.localTransforms[0];
        return true;
    }
    bool Sample(double time, Pose& result)
    {
        if (boneCount != 1) return false;
        ++samples;
        sampledBones += boneCount;
        // Preserve the old whole-pose work ceiling, and independently bound
        // sliced call overhead (four variants at most 8,388,608 calls total).
        if (sampledBones > 131072 * sourceBoneCount || samples > 2097152) {
            failure = "sampled bone/call budget exceeded"; return false;
        }
        // Keep the sampled clock local. Adding/subtracting a whole duration
        // first loses float precision and can look like a curve discontinuity
        // during adaptive refinement; loop neighborhood is a separate state.
        GrannySetControlRawLocalClock(control, static_cast<float>(time));
        GrannySetControlLoopIndex(control, (boundary & 1) ? 1 : 0);
        GrannySampleModelAnimations(model, static_cast<int>(firstBone), static_cast<int>(boneCount), local);
        ++importPoseSamples;
        for (std::size_t bone = 0; bone < boneCount; ++bone) {
            const auto* transform = GrannyGetLocalPoseTransform(local, static_cast<int>(bone + firstBone));
            if (!transform) return false;
            result.localTransforms[bone] = CopyTransform(*transform);
            const auto& value = result.localTransforms[bone];
            for (float component : value.translation) if (!std::isfinite(component)) return false;
            for (float component : value.rotation) if (!std::isfinite(component)) return false;
            for (float component : value.scaleShear) if (!std::isfinite(component)) return false;
        }
        return true;
    }
};

float TranslationTolerance(float first, float second, float absolute)
{
    const float magnitude = std::max(std::abs(first), std::abs(second));
    const float next = std::nextafter(magnitude, std::numeric_limits<float>::infinity());
    const float ulp = std::isfinite(next) ? next - magnitude : magnitude - std::nextafter(magnitude, 0.0f);
    return std::max(absolute, 2.0f * ulp);
}

bool ClosePose(const Pose& actual, const Pose& interpolated, std::string* mismatch = nullptr)
{
    const auto fail = [&](const char* channel, std::size_t bone, int component, double actualValue, double predictedValue) {
        if (mismatch) {
            char detail[240];
            std::snprintf(detail, sizeof(detail), "%s bone=%zu component=%d actual=%.12g predicted=%.12g deviation=%.12g",
                channel, bone, component, actualValue, predictedValue, std::abs(actualValue - predictedValue));
            *mismatch = detail;
        }
        return false;
    };
    for (std::size_t bone = 0; bone < actual.localTransforms.size(); ++bone) {
        const auto& a = actual.localTransforms[bone];
        const auto& b = interpolated.localTransforms[bone];
        for (int i = 0; i < 3; ++i)
            if (std::abs(a.translation[i] - b.translation[i]) > TranslationTolerance(a.translation[i], b.translation[i], 0.00002f))
                return fail("translation", bone, i, a.translation[i], b.translation[i]);
        for (int i = 0; i < 9; ++i) if (std::abs(a.scaleShear[i] - b.scaleShear[i]) > 0.000002f) return fail("scale", bone, i, a.scaleShear[i], b.scaleShear[i]);
        double aa = 0, bb = 0, dot = 0;
        for (int i = 0; i < 4; ++i) { aa += double(a.rotation[i]) * a.rotation[i]; bb += double(b.rotation[i]) * b.rotation[i]; dot += double(a.rotation[i]) * b.rotation[i]; }
        if (!(aa > 1e-20 && bb > 1e-20)) return false;
        const double scaleA = 1.0 / std::sqrt(aa), scaleB = (dot < 0 ? -1.0 : 1.0) / std::sqrt(bb);
        for (int i = 0; i < 4; ++i)
            if (std::abs(a.rotation[i] * scaleA - b.rotation[i] * scaleB) > 0.000002) return fail("quaternion", bone, i, a.rotation[i] * scaleA, b.rotation[i] * scaleB);
    }
    return true;
}

struct BakedSample { double time{}; Pose pose; };
bool Refine(ImportSampler& sampler, const BakedSample& first, const BakedSample& last,
    unsigned depth, std::vector<BakedSample>& result)
{
    if (result.size() >= 32768) { sampler.failure = "pose key budget exceeded"; return false; }
    constexpr std::size_t maximumSavedPoseBytes = 256 * 1024 * 1024;
    const auto snapshotBytes = sampler.boneCount * sizeof(AnimationRuntime::LocalTransform) + sizeof(BakedSample);
    if (result.size() + 1 > maximumSavedPoseBytes / snapshotBytes) {
        sampler.failure = "temporary pose storage budget exceeded"; return false;
    }
    Pose interpolated;
    BakedSample middle{static_cast<float>((first.time + last.time) * 0.5), {}};
    bool close = true;
    std::string mismatch;
    for (float fraction : {0.25f, 0.5f, 0.75f}) {
        Pose actual;
        const double time = static_cast<float>(first.time + (last.time - first.time) * fraction);
        const float actualFraction = static_cast<float>((time - first.time) / (last.time - first.time));
        if (!sampler.Sample(time, actual) || !sampler.Blend(first.pose, last.pose, actualFraction, interpolated)) return false;
        if (!ClosePose(actual, interpolated, depth >= 11 ? &mismatch : nullptr)) close = false;
        if (fraction == 0.5f) middle.pose = std::move(actual);
    }
    if (close) { result.push_back(last); return true; }
    if (depth >= 12 || last.time - first.time < 0.000001) {
        sampler.failure = "refinement floor boundary=" + std::to_string(sampler.boundary) +
            " begin=" + std::to_string(first.time) + " end=" + std::to_string(last.time) + " depth=" + std::to_string(depth) + " " + mismatch;
        return false;
    }
    return Refine(sampler, first, middle, depth + 1, result) &&
        Refine(sampler, middle, last, depth + 1, result);
}

template <std::size_t N> std::array<float, N> InterpolateValue(const std::array<float, N>& first,
    const std::array<float, N>& last, float fraction)
{
    std::array<float, N> result;
    float sign = 1;
    if constexpr (N == 4) {
        double dot = 0;
        for (std::size_t i = 0; i < N; ++i) dot += double(first[i]) * last[i];
        if (dot < 0) sign = -1;
    }
    for (std::size_t i = 0; i < N; ++i)
        result[i] = static_cast<float>(double(first[i]) * (1.0 - fraction) + double(last[i]) * sign * fraction);
    if constexpr (N == 4) {
        double norm = 0;
        for (float value : result) norm += double(value) * value;
        if (!(norm > 1e-30) || !std::isfinite(norm)) {
            result.fill(std::numeric_limits<float>::quiet_NaN()); return result;
        }
        const float factor = static_cast<float>(1.0 / std::sqrt(norm));
        for (auto& value : result) value *= factor;
    }
    return result;
}

template <std::size_t N> double ReductionError(std::array<float, N> actual, std::array<float, N> predicted)
{
    double result = 0;
    for (std::size_t i = 0; i < N; ++i)
        if (!std::isfinite(actual[i]) || !std::isfinite(predicted[i])) return std::numeric_limits<double>::infinity();
    if constexpr (N == 4) {
        double aa = 0, bb = 0, dot = 0;
        for (std::size_t i = 0; i < N; ++i) { aa += double(actual[i]) * actual[i]; bb += double(predicted[i]) * predicted[i]; dot += double(actual[i]) * predicted[i]; }
        if (!(aa > 1e-30 && bb > 1e-30) || !std::isfinite(aa) || !std::isfinite(bb)) return std::numeric_limits<double>::infinity();
        const double sa = 1.0 / std::sqrt(aa), sb = (dot < 0 ? -1.0 : 1.0) / std::sqrt(bb);
        for (std::size_t i = 0; i < N; ++i) result = std::max(result, std::abs(actual[i] * sa - predicted[i] * sb) / 0.0000005);
    } else {
        for (std::size_t i = 0; i < N; ++i) {
            const float tolerance = N == 3 ? TranslationTolerance(actual[i], predicted[i], 0.000005f) : 0.0000005f;
            const double error = std::abs(double(actual[i]) - predicted[i]) / tolerance;
            if (!std::isfinite(error)) return std::numeric_limits<double>::infinity();
            result = std::max(result, error);
        }
    }
    return result;
}

template <std::size_t N> void ReduceKeys(AnimationRuntime::Track<std::array<float, N>>& track)
{
    auto& keys = track.keys;
    if (keys.size() < 2) return;
    if (std::all_of(keys.begin() + 1, keys.end(), [&](const auto& key) { return ReductionError(key.value, keys.front().value) <= 1; })) {
        keys.resize(1); keys.shrink_to_fit(); return;
    }
    std::vector<bool> keep(keys.size(), false);
    keep.front() = keep.back() = true;
    std::vector<std::pair<std::size_t, std::size_t>> pending{{0, keys.size() - 1}};
    while (!pending.empty()) {
        const auto [first, last] = pending.back();
        pending.pop_back();
        double maximum = 1;
        std::size_t selected = first;
        for (std::size_t key = first + 1; key < last; ++key) {
            const float fraction = static_cast<float>((keys[key].time - keys[first].time) / (keys[last].time - keys[first].time));
            const double error = ReductionError(keys[key].value, InterpolateValue(keys[first].value, keys[last].value, fraction));
            if (error > maximum) { maximum = error; selected = key; }
        }
        if (selected != first) {
            keep[selected] = true;
            pending.push_back({selected, last});
            pending.push_back({first, selected});
        }
    }
    std::size_t destination = 0;
    for (std::size_t key = 0; key < keys.size(); ++key) if (keep[key]) keys[destination++] = std::move(keys[key]);
    keys.resize(destination);
    keys.shrink_to_fit();
}

std::shared_ptr<const RuntimeClip> Bake(granny_model* source, granny_animation* animation,
    const RuntimeSkeleton& skeleton, unsigned boundary, ImportedAnimation& diagnostics, std::string& error)
{
    ImportSampler sampler;
    if (!sampler.Initialize(source, animation, boundary)) { error = "source animation binding failed"; return {}; }
    const double duration = animation->Duration;
    const double intervalCount = std::ceil(duration * 60.0);
    if (!std::isfinite(intervalCount) || intervalCount > 36000) { error = "animation duration exceeds import budget"; return {}; }
    const auto intervals = static_cast<std::size_t>(intervalCount);
    std::vector<AnimationRuntime::AnimationTrack> tracks(skeleton.Bones().size());
    for (std::size_t bone = 0; bone < tracks.size(); ++bone) {
        sampler.firstBone = bone;
        sampler.boneCount = 1;
        std::vector<BakedSample> samples;
        BakedSample first{0, {}};
        if (!sampler.Sample(0, first.pose)) { error = "non-finite source sample"; return {}; }
        samples.push_back(first);
        for (std::size_t interval = 0; interval < intervals; ++interval) {
            BakedSample last{static_cast<float>(duration * double(interval + 1) / double(intervals)), {}};
            if (!sampler.Sample(last.time, last.pose) || !Refine(sampler, first, last, 0, samples)) {
                error = "adaptive import tolerance or sample budget exceeded: bone=" + std::to_string(bone) + " " + sampler.failure;
                return {};
            }
            first = std::move(last);
        }
        auto& track = tracks[bone];
        track.targetBone = static_cast<std::uint32_t>(bone);
        for (const auto& sample : samples) {
            const auto& transform = sample.pose.localTransforms.front();
            track.translation.keys.push_back({sample.time, transform.translation});
            track.rotation.keys.push_back({sample.time, transform.rotation});
            track.scaleShear.keys.push_back({sample.time, transform.scaleShear});
        }
        ReduceKeys(track.translation);
        ReduceKeys(track.rotation);
        ReduceKeys(track.scaleShear);
        diagnostics.storedKeys += track.translation.keys.size() + track.rotation.keys.size() + track.scaleShear.keys.size();
        if (diagnostics.storedKeys > 2000000) { error = "animation key storage budget exceeded"; return {}; }
    }
    diagnostics.importSamples += sampler.samples;
    diagnostics.sampledBoneTransforms += sampler.sampledBones;
    auto result = std::make_shared<RuntimeClip>();
    const bool loops = !(animation->Flags & GrannyAnimationDefaultLoopCountValid) || animation->DefaultLoopCount != 1;
    if (!result->Initialize(animation->Name ? animation->Name : "", duration, loops, std::move(tracks), skeleton, error)) return {};
    return result;
}
}

void ClearImportCache() noexcept
{
    std::vector<RetainedClip>().swap(retainedClips);
    std::vector<LiveClip>().swap(liveClips);
    retainedImportKeyBytes = 0;
    cacheClock = 0;
}

std::size_t RuntimeImportCache::RetainedKeyBytes(std::size_t modelIndex) const
{
    return modelFingerprint ? ModelKeyBytes(*modelFingerprint, modelIndex) : 0;
}

std::shared_ptr<const ImportedAnimation> RuntimeImportCache::Find(const AnimationHandle& animation,
    std::size_t modelIndex, std::uint64_t bindingId)
{
    const auto key = MakeContentKey(*this, animation, modelIndex, bindingId);
    const auto recordLookup = [&](bool hit) {
        AnimationStallAudit::CacheLookup(key ? &key->model : nullptr, key ? &key->animation : nullptr,
            modelIndex, animation.Index(), bindingId, hit);
    };
    if (!key) { recordLookup(false); return {}; }
    animationRuntimeCacheClearSink = &ClearImportCache;
    for (auto& entry : retainedClips)
        if (entry.key == *key) {
            entry.lastUsed = ++cacheClock;
            ++importCacheHits;
            recordLookup(true);
            return entry.runtime;
        }
    std::erase_if(liveClips, [](const auto& entry) { return entry.runtime.expired(); });
    for (const auto& entry : liveClips)
        if (entry.key == *key)
            if (auto runtime = entry.runtime.lock()) {
                ++importCacheHits;
                recordLookup(true);
                return runtime;
            }
    recordLookup(false);
    return {};
}

void RuntimeImportCache::Retain(const ModelHandle& model, const AnimationHandle& animation,
    std::uint64_t bindingId, std::shared_ptr<const ImportedAnimation> runtime)
{
    if (!runtime) return;
    if (!modelFingerprint) modelFingerprint = GrannyInterop::GetSourceFingerprint(model.GetDocument());
    const auto key = MakeContentKey(*this, animation, model.Index(), bindingId);
    if (!key) return;
    animationRuntimeCacheClearSink = &ClearImportCache;
    std::erase_if(liveClips, [&](const auto& entry) {
        return entry.runtime.expired() || entry.key == *key;
    });
    if (liveClips.size() >= maximumLiveEntries) liveClips.erase(liveClips.begin());
    liveClips.push_back({*key, runtime});
    if (runtime->keyBytes > ModelKeyByteLimit) return;
    for (std::size_t index = retainedClips.size(); index > 0; --index)
        if (retainedClips[index - 1].key == *key) RemoveCachedClip(index - 1, false);
    while (ModelKeyBytes(key->model, key->modelIndex) + (Retains(runtime.get(), &*key) ? 0 : runtime->keyBytes) > ModelKeyByteLimit ||
        ModelEntryCount(*key) >= maximumModelEntries) {
        const auto oldest = OldestClip(&*key);
        if (oldest == retainedClips.size()) return;
        RemoveCachedClip(oldest, true);
    }
    while (retainedImportKeyBytes.load() + (Retains(runtime.get()) ? 0 : runtime->keyBytes) > ProcessKeyByteLimit ||
        retainedClips.size() >= maximumRetainedEntries) {
        const auto oldest = OldestClip();
        if (oldest == retainedClips.size()) return;
        RemoveCachedClip(oldest, true);
    }
    const bool alreadyRetained = Retains(runtime.get());
    retainedClips.push_back({*key, ++cacheClock, runtime});
    const auto addedBytes = alreadyRetained ? 0 : runtime->keyBytes;
    const auto retained = retainedImportKeyBytes.fetch_add(addedBytes) + addedBytes;
    peakRetainedImportKeyBytes = std::max(peakRetainedImportKeyBytes.load(), retained);
}

std::shared_ptr<const RuntimeSkeleton> ImportSkeleton(const ModelHandle& model, std::string& error)
{
    error.clear();
    if (!model || !model.Get()->skeleton) { error = "missing source skeleton"; return {}; }
    auto* cache = GrannyInterop::GetRuntimeImportCache(model);
    if (!cache) { error = "missing model import cache"; return {}; }
    cache->modelFingerprint = GrannyInterop::GetSourceFingerprint(model.GetDocument());
    if (model.Index() < cache->skeletons.size() && cache->skeletons[model.Index()]) return cache->skeletons[model.Index()];
    std::vector<AnimationRuntime::SkeletonBone> bones;
    for (const auto& source : model.Get()->skeleton->bones) {
        AnimationRuntime::SkeletonBone bone;
        bone.name = source.name;
        bone.parent = source.parentIndex;
        bone.localBind.translation = source.localBind.position;
        bone.localBind.rotation = source.localBind.orientation;
        bone.localBind.scaleShear = source.localBind.scaleShear;
        bone.inverseBind = source.inverseBind;
        bones.push_back(std::move(bone));
    }
    auto result = std::make_shared<RuntimeSkeleton>();
    if (!result->Initialize(std::move(bones), error)) return {};
    if (cache->skeletons.size() <= model.Index()) cache->skeletons.resize(model.Index() + 1);
    cache->skeletons[model.Index()] = result;
    return result;
}

std::shared_ptr<const ImportedAnimation> ImportAnimation(const ModelHandle& model,
    const AnimationHandle& animation, std::shared_ptr<const RuntimeSkeleton> skeleton, std::string& error)
{
    error.clear();
    auto* nativeModel = GrannyInterop::GetModel(model);
    auto* nativeAnimation = GrannyInterop::GetAnimation(animation);
    if (!nativeModel || !nativeAnimation || !skeleton || !std::isfinite(nativeAnimation->Duration) || nativeAnimation->Duration < 0) {
        error = "invalid source model/animation"; return {};
    }
    if (nativeAnimation->Duration > 600) { error = "animation duration exceeds import budget"; return {}; }
    if (!nativeModel->Skeleton || nativeModel->Skeleton->BoneCount <= 0 || !nativeModel->Skeleton->Bones ||
        skeleton->Bones().size() != static_cast<std::size_t>(nativeModel->Skeleton->BoneCount)) {
        error = "runtime skeleton does not match source bone count"; return {};
    }
    if (nativeModel->Skeleton->BoneCount > 1024) { error = "source skeleton exceeds import budget"; return {}; }
    for (std::size_t bone = 0; bone < skeleton->Bones().size(); ++bone) {
        const auto& source = nativeModel->Skeleton->Bones[bone];
        if (!source.Name || skeleton->Bones()[bone].name != source.Name || skeleton->Bones()[bone].parent != source.ParentIndex) {
            error = "runtime skeleton does not match source bone order"; return {};
        }
    }
    auto* cache = GrannyInterop::GetRuntimeImportCache(model);
    if (!cache) { error = "missing model import cache"; return {}; }
    cache->modelFingerprint = GrannyInterop::GetSourceFingerprint(model.GetDocument());
    if (auto cached = cache->Find(animation, model.Index(), skeleton->BindingId())) return cached;
    AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Import);
    AnimationStallAudit::ImportStarted();
    if (AnimationStallAudit::enabled) {
        // Read the stored digests only; this does not hash source bytes again.
        const auto key = MakeContentKey(*cache, animation, model.Index(), skeleton->BindingId());
        AnimationStallAudit::ImportKey(key ? &key->model : nullptr, key ? &key->animation : nullptr,
            model.Index(), animation.Index(), skeleton->BindingId());
    }
    const auto importStarted = std::chrono::steady_clock::now();
    granny_int32x groupIndex = -1;
    if (!GrannyFindTrackGroupForModel(nativeAnimation, nativeModel->Name, &groupIndex) ||
        groupIndex < 0 || groupIndex >= nativeAnimation->TrackGroupCount) {
        error = "no matching track group"; return {};
    }
    const auto* group = nativeAnimation->TrackGroups[groupIndex];
    if (!group || group->TransformTrackCount < 0 || (group->TransformTrackCount && !group->TransformTracks) ||
        group->VectorTrackCount != 0 || (group->Flags & GrannyTrackGroupIsMorphs)) {
        error = "unsupported or invalid source track group"; return {};
    }
    auto result = std::make_shared<ImportedAnimation>();
    result->groupFlags = group->Flags;
    result->animationFlags = nativeAnimation->Flags;
    result->defaultLoopCount = nativeAnimation->DefaultLoopCount;
    result->sourceTimeStep = nativeAnimation->TimeStep;
    std::copy_n(group->LoopTranslation, 3, result->loopTranslation.begin());
    for (int index = 0; index < group->TransformTrackCount; ++index) {
        const auto& track = group->TransformTracks[index];
        if (!track.Name) { error = "source track has no bone name"; return {}; }
        if (skeleton->FindBone(track.Name) < 0) result->ignoredSourceTracks.emplace_back(track.Name);
        if (!InventoryCurve(track.PositionCurve, track.Name, 3, *result, error) ||
            !InventoryCurve(track.OrientationCurve, track.Name, 4, *result, error) ||
            !InventoryCurve(track.ScaleShearCurve, track.Name, 9, *result, error)) return {};
    }
    for (unsigned boundary = 0; boundary < result->boundaryClips.size(); ++boundary) {
        result->boundaryClips[boundary] = Bake(nativeModel, nativeAnimation, *skeleton, boundary, *result, error);
        if (!result->boundaryClips[boundary]) return {};
    }
    result->clip = result->boundaryClips[3];
    result->keyBytes = KeyBytes(*result);
    cache->Retain(model, animation, skeleton->BindingId(), result);
    result->importMicroseconds = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - importStarted).count();
    importTotalMicroseconds += result->importMicroseconds;
    importPeakMicroseconds = std::max(importPeakMicroseconds.load(), result->importMicroseconds);
    ++importedAnimationClips;
    if (result->importMicroseconds >= 50000) {
        const std::string message = "ZiiNAN Animation Runtime import: model=" + model.GetDocument()->Id() +
            " clip=" + animation.GetDocument()->Id() + " microseconds=" + std::to_string(result->importMicroseconds) +
            " nativeCalls=" + std::to_string(result->importSamples) + " sampledBones=" + std::to_string(result->sampledBoneTransforms) +
            " keys=" + std::to_string(result->storedKeys) + " keyBytes=" + std::to_string(result->keyBytes);
        if (animationRuntimeErrorSink) animationRuntimeErrorSink(message.c_str());
        else std::fprintf(stderr, "%s\n", message.c_str());
    }
    if (!result->ignoredSourceTracks.empty()) {
        ++importedBindingWarnings;
        const std::string message = "ZiiNAN Animation Runtime binding warning: model=" + model.GetDocument()->Id() +
            " clip=" + animation.GetDocument()->Id() + " source tracks absent from destination skeleton=" +
            std::to_string(result->ignoredSourceTracks.size()) + "; existing SDK LOD binding preserved";
        if (animationRuntimeErrorSink) animationRuntimeErrorSink(message.c_str());
        else std::fprintf(stderr, "%s\n", message.c_str());
    }
    return result;
}
}
