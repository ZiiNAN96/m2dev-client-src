#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace AnimationRuntime
{
using Vector3 = std::array<float, 3>;
using Quaternion = std::array<float, 4>; // xyzw
using ScaleShear = std::array<float, 9>;
using Matrix = std::array<float, 16>;

constexpr Matrix IdentityMatrix() noexcept
{
    return {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
}

struct LocalTransform
{
    Vector3 translation{0, 0, 0};
    Quaternion rotation{0, 0, 0, 1};
    ScaleShear scaleShear{1, 0, 0, 0, 1, 0, 0, 0, 1};
};

struct SkeletonBone
{
    std::string name;
    std::int32_t parent = -1;
    LocalTransform localBind;
    Matrix inverseBind = IdentityMatrix();
};

class RuntimeSkeleton
{
public:
    static constexpr std::size_t MaximumDepth = 256;
    RuntimeSkeleton();
    RuntimeSkeleton(const RuntimeSkeleton&);
    RuntimeSkeleton(RuntimeSkeleton&&) noexcept;
    RuntimeSkeleton& operator=(const RuntimeSkeleton&) = default;
    RuntimeSkeleton& operator=(RuntimeSkeleton&&) noexcept = default;
    ~RuntimeSkeleton();

    // Indexed source formats can contain several roots and repeated display
    // names. Indices, parent validation and evaluation order remain unchanged.
    enum class SourceSemantics { NamedTree, IndexedForest };
    bool Initialize(std::vector<SkeletonBone> bones, std::string& error,
        SourceSemantics semantics = SourceSemantics::NamedTree);
    const std::vector<SkeletonBone>& Bones() const noexcept { return bones_; }
    const std::vector<std::uint32_t>& EvaluationOrder() const noexcept { return evaluationOrder_; }
    std::int32_t FindBone(std::string_view name) const noexcept;
    std::uint64_t BindingId() const noexcept { return bindingId_; }

private:
    std::vector<SkeletonBone> bones_;
    std::vector<std::uint32_t> evaluationOrder_;
    std::uint64_t bindingId_ = 0;
};

enum class Interpolation { Step, Linear };

template <class Value> struct Keyframe
{
    double time = 0; // seconds
    Value value{};
};

template <class Value> struct Track
{
    Interpolation interpolation = Interpolation::Linear;
    std::vector<Keyframe<Value>> keys;
};

struct AnimationTrack
{
    std::uint32_t targetBone = 0;
    Track<Vector3> translation;
    Track<Quaternion> rotation;
    Track<ScaleShear> scaleShear;
};

class RuntimeAnimationClip
{
public:
    RuntimeAnimationClip();
    RuntimeAnimationClip(const RuntimeAnimationClip&);
    RuntimeAnimationClip(RuntimeAnimationClip&&) noexcept;
    RuntimeAnimationClip& operator=(const RuntimeAnimationClip&) = default;
    RuntimeAnimationClip& operator=(RuntimeAnimationClip&&) noexcept = default;
    ~RuntimeAnimationClip();

    bool Initialize(std::string name, double duration, bool looping,
        std::vector<AnimationTrack> tracks, const RuntimeSkeleton& skeleton, std::string& error);
    const std::string& Name() const noexcept { return name_; }
    double Duration() const noexcept { return duration_; }
    bool Looping() const noexcept { return looping_; }
    const std::vector<AnimationTrack>& Tracks() const noexcept { return tracks_; }
    std::uint64_t BindingId() const noexcept { return bindingId_; }

private:
    std::string name_;
    double duration_ = 0;
    bool looping_ = false;
    std::vector<AnimationTrack> tracks_;
    std::uint64_t bindingId_ = 0;
};

struct AnimationPose
{
    std::vector<LocalTransform> localTransforms;
    void Prepare(std::size_t boneCount) { localTransforms.resize(boneCount); }
};

enum class TimeMode { Clip, Clamp, Loop };

// ZiiNAN: Animation Runtime boundary. Matrices use row vectors; local * parent,
// inverseBind * model, with translation at indices 12..14. Outputs are prepared
// by the caller; successful sampling, evaluation and blending allocate nothing.
Matrix LocalMatrix(const LocalTransform& transform) noexcept;
Matrix Multiply(const Matrix& left, const Matrix& right) noexcept;
double ResolveTime(double time, double duration, bool looping) noexcept;
bool Sample(const RuntimeSkeleton& skeleton, const RuntimeAnimationClip& clip,
    double time, TimeMode mode, AnimationPose& output) noexcept;
bool Evaluate(const RuntimeSkeleton& skeleton, const AnimationPose& pose,
    std::span<Matrix> modelMatrices, const Matrix* attachmentParent = nullptr) noexcept;
bool BuildPalette(const RuntimeSkeleton& skeleton, std::span<const Matrix> modelMatrices,
    std::span<Matrix> palette) noexcept;
bool Blend(const AnimationPose& first, const AnimationPose& second,
    float weight, AnimationPose& output) noexcept;

struct PlaybackClock
{
    double time = 0;
    double rate = 1;
    bool paused = false;
    bool Advance(double deltaSeconds) noexcept;
};

struct LifetimeCounts
{
    std::uint64_t skeletons = 0;
    std::uint64_t clips = 0;
};
LifetimeCounts GetLifetimeCounts() noexcept;
}
