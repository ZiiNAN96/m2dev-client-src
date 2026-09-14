#include "AnimationRuntime/AnimationRuntime.h"

#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace
{
std::atomic<std::size_t> allocationCount{0};
bool countAllocations = false;
}
void* operator new(std::size_t size)
{
    if (countAllocations) ++allocationCount;
    if (void* result = std::malloc(size == 0 ? 1 : size)) return result;
    throw std::bad_alloc();
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

using namespace AnimationRuntime;
namespace
{
void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void Near(double actual, double expected, const char* message, double tolerance = 1e-5)
{
    Check(std::isfinite(actual) && std::abs(actual - expected) <= tolerance, message);
}
void Identity(const Matrix& value, const char* message)
{
    const Matrix identity = IdentityMatrix();
    for (std::size_t i = 0; i < 16; ++i) Near(value[i], identity[i], message);
}
SkeletonBone Bone(std::string name, std::int32_t parent, Vector3 translation = {})
{
    SkeletonBone bone;
    bone.name = std::move(name);
    bone.parent = parent;
    bone.localBind.translation = translation;
    return bone;
}
RuntimeSkeleton Skeleton(std::vector<SkeletonBone> bones)
{
    RuntimeSkeleton skeleton;
    std::string error;
    Check(skeleton.Initialize(std::move(bones), error), error.c_str());
    return skeleton;
}
RuntimeAnimationClip Clip(const RuntimeSkeleton& skeleton, std::vector<AnimationTrack> tracks = {},
    double duration = 2, bool loop = true)
{
    RuntimeAnimationClip clip;
    std::string error;
    Check(clip.Initialize("fixture", duration, loop, std::move(tracks), skeleton, error), error.c_str());
    return clip;
}
AnimationPose Pose(std::size_t size)
{
    AnimationPose pose;
    pose.Prepare(size);
    return pose;
}

void Hierarchy()
{
    auto skeleton = Skeleton({Bone("hand", 2, {0, 3, 0}), Bone("root", -1, {10, 0, 0}),
        Bone("arm", 1, {2, 0, 0}), Bone("other", 1, {0, 0, 4})});
    Check(skeleton.FindBone("hand") == 0 && skeleton.FindBone("root") == 1 &&
        skeleton.FindBone("missing") == -1, "original bone order/name binding");
    Check(skeleton.EvaluationOrder() == std::vector<std::uint32_t>({1, 2, 0, 3}), "parent-first evaluation order");
    auto clip = Clip(skeleton);
    auto pose = Pose(4);
    std::vector<Matrix> model(4), palette(4);
    Check(Sample(skeleton, clip, 0, TimeMode::Clip, pose) && Evaluate(skeleton, pose, model), "bind evaluation");
    Near(model[0][12], 12, "chain bind x");
    Near(model[0][13], 3, "chain bind y");
    Near(model[3][12], 10, "branch bind x");
    Near(model[3][14], 4, "branch bind z");
    Check(BuildPalette(skeleton, model, palette), "palette evaluation");
    Matrix attachment = IdentityMatrix();
    attachment[13] = 20;
    Check(Evaluate(skeleton, pose, model, &attachment), "attachment parent evaluation");
    Near(model[0][13], 23, "attachment translated child");

    auto invalid = [&](std::vector<SkeletonBone> bones, const char* message) {
        RuntimeSkeleton candidate;
        std::string error;
        Check(!candidate.Initialize(std::move(bones), error) && !error.empty() && candidate.Bones().empty(), message);
    };
    invalid({}, "empty skeleton rejection");
    invalid({Bone("root", -1), Bone("out", 2)}, "out of range parent rejection");
    invalid({Bone("root", -2)}, "invalid negative parent rejection");
    invalid({Bone("root", -1), Bone("self", 1)}, "self parent rejection");
    invalid({Bone("root", -1), Bone("a", 2), Bone("b", 1)}, "disconnected cycle rejection");
    invalid({Bone("a", 1), Bone("b", 0)}, "missing root rejection");
    invalid({Bone("a", -1), Bone("b", -1)}, "disconnected roots rejection");
    invalid({Bone("root", -1), Bone("root", 0)}, "duplicate name rejection");
    invalid({Bone("", -1)}, "empty name rejection");
    std::vector<SkeletonBone> chain;
    for (std::size_t i = 0; i <= RuntimeSkeleton::MaximumDepth; ++i)
        chain.push_back(Bone(std::to_string(i), static_cast<std::int32_t>(i) - 1));
    invalid(chain, "excessive depth rejection");
    chain.pop_back();
    auto maximum = Skeleton(std::move(chain));
    Check(maximum.Bones().size() == RuntimeSkeleton::MaximumDepth, "maximum depth accepted");
    SkeletonBone bad = Bone("bad", -1);
    bad.localBind.rotation = {0, 0, 0, 0};
    invalid({bad}, "zero quaternion rejection");
    bad.localBind.rotation = {0, 0, 0, 4};
    auto normalized = Skeleton({bad});
    Near(normalized.Bones()[0].localBind.rotation[3], 1, "bind quaternion normalization");
    bad.inverseBind[0] = std::numeric_limits<float>::infinity();
    invalid({bad}, "nonfinite inverse bind rejection");
    bad = Bone("bad", -1);
    bad.localBind.scaleShear[4] = std::numeric_limits<float>::quiet_NaN();
    invalid({bad}, "nonfinite scale rejection");
    bad = Bone("bad", -1);
    bad.localBind.translation[1] = std::numeric_limits<float>::infinity();
    invalid({bad}, "nonfinite translation rejection");
}

void Matrices()
{
    const float sine = std::sqrt(0.5f);
    SkeletonBone root = Bone("root", -1, {10, 0, 0});
    root.localBind.rotation = {0, 0, sine, sine};
    auto skeleton = Skeleton({root, Bone("child", 0, {2, 0, 0})});
    auto pose = Pose(2);
    auto clip = Clip(skeleton);
    std::vector<Matrix> model(2);
    Check(Sample(skeleton, clip, 0, TimeMode::Clamp, pose) && Evaluate(skeleton, pose, model), "rotated hierarchy");
    Near(model[1][12], 10, "row-vector local times parent x");
    Near(model[1][13], 2, "row-vector local times parent y");
    LocalTransform transform;
    transform.rotation = root.localBind.rotation;
    transform.scaleShear = {2, 1, 0, 0, 3, 0, 0, 0, 4};
    auto matrix = LocalMatrix(transform);
    Near(matrix[0], -1, "scale shear before rotation xx");
    Near(matrix[1], 2, "scale shear before rotation xy");
    Near(matrix[4], -3, "scale shear before rotation yx");
    Near(matrix[10], 4, "nonuniform scale z");

    root = Bone("root", -1, {10, 0, 0});
    root.inverseBind[12] = -10;
    SkeletonBone child = Bone("child", 0, {0, 3, 0});
    child.inverseBind[12] = -10;
    child.inverseBind[13] = -3;
    skeleton = Skeleton({root, child});
    clip = Clip(skeleton);
    std::vector<Matrix> palette(2);
    Check(Sample(skeleton, clip, 0, TimeMode::Clamp, pose) && Evaluate(skeleton, pose, model) &&
        BuildPalette(skeleton, model, palette), "bind palette");
    Identity(palette[0], "root inverse bind identity");
    Identity(palette[1], "chain inverse bind identity");
    pose.localTransforms[0].rotation = {0, 0, sine, sine};
    Check(Evaluate(skeleton, pose, model) && BuildPalette(skeleton, model, palette), "animated palette");
    Near(palette[1][12], 10, "inverse bind multiplied before model x");
    Near(palette[1][13], -10, "inverse bind multiplied before model y");
    model[0][0] = std::numeric_limits<float>::infinity();
    Check(!BuildPalette(skeleton, model, palette), "invalid model matrix rejection");
    pose.localTransforms[0].rotation = {0, 0, 0, 0};
    Check(!Evaluate(skeleton, pose, model), "invalid pose transform rejection");
    Check(!Evaluate(skeleton, Pose(1), model), "pose size mismatch rejection");
}

AnimationTrack AnimatedTrack()
{
    AnimationTrack track;
    track.translation.keys = {{0, {0, 0, 0}}, {2, {10, 4, -2}}};
    track.rotation.keys = {{0, {0, 0, 0, 2}}, {2, {0, 0, 2, 0}}};
    track.scaleShear.keys = {{0, {1, 0, 0, 0, 1, 0, 0, 0, 1}}, {2, {3, 1, 0, 0, 5, 0, 0, 0, 7}}};
    return track;
}

void SamplingAndTime()
{
    auto skeleton = Skeleton({Bone("root", -1), Bone("child", 0, {7, 8, 9})});
    auto clip = Clip(skeleton, {AnimatedTrack()});
    auto pose = Pose(2);
    Check(Sample(skeleton, clip, 1, TimeMode::Clip, pose), "sample midpoint");
    Near(pose.localTransforms[0].translation[0], 5, "translation lerp");
    Near(pose.localTransforms[0].rotation[2], std::sqrt(0.5), "quaternion nlerp z");
    Near(pose.localTransforms[0].rotation[3], std::sqrt(0.5), "quaternion nlerp w");
    Near(pose.localTransforms[0].scaleShear[0], 2, "scale lerp x");
    Near(pose.localTransforms[0].scaleShear[4], 3, "nonuniform scale lerp y");
    Near(pose.localTransforms[0].scaleShear[8], 4, "nonuniform scale lerp z");
    Near(pose.localTransforms[0].scaleShear[1], 0.5, "shear lerp");
    Near(pose.localTransforms[1].translation[0], 7, "missing track bind default");
    for (const auto& [time, expected] : std::vector<std::pair<double, double>>{{-0.5, 7.5}, {0, 0},
        {0.2, 1}, {0.5, 2.5}, {1, 5}, {1.5, 7.5}, {1.8, 9}, {2, 0}, {2.01, 0.05}, {6.5, 2.5}})
    {
        Check(Sample(skeleton, clip, time, TimeMode::Loop, pose), "loop sample");
        Near(pose.localTransforms[0].translation[0], expected, "seconds loop semantics");
    }
    Check(Sample(skeleton, clip, -1, TimeMode::Clamp, pose), "negative clamp");
    Near(pose.localTransforms[0].translation[0], 0, "clamp to beginning");
    Check(Sample(skeleton, clip, 20, TimeMode::Clamp, pose), "past-end clamp");
    Near(pose.localTransforms[0].translation[0], 10, "clamp to final key");
    Check(Sample(skeleton, clip, 1.99999, TimeMode::Loop, pose), "before loop boundary");
    Near(pose.localTransforms[0].translation[0], 9.99995, "last sample before boundary");
    auto constant = Clip(skeleton, {}, 0);
    Check(Sample(skeleton, constant, -500, TimeMode::Loop, pose), "zero-duration clip");
    Near(pose.localTransforms[1].translation[2], 9, "zero-duration bind default");
    auto track = AnimatedTrack();
    track.translation.interpolation = Interpolation::Step;
    track.rotation.keys = {{0, {0, 0, 0, 1}}, {2, {0, 0, 0, -1}}};
    track.scaleShear.keys.clear();
    auto step = Clip(skeleton, {track});
    Check(Sample(skeleton, step, 1, TimeMode::Clip, pose), "step/antipodal sample");
    Near(pose.localTransforms[0].translation[0], 0, "step holds previous key");
    Near(std::abs(pose.localTransforms[0].rotation[3]), 1, "antipodal quaternion shortest path");
    Near(pose.localTransforms[0].scaleShear[4], 1, "missing channel bind default");
    Check(Sample(skeleton, step, 2, TimeMode::Clamp, pose), "step endpoint");
    Near(pose.localTransforms[0].translation[0], 10, "step exact final key");
    Check(!Sample(skeleton, clip, std::numeric_limits<double>::infinity(), TimeMode::Clip, pose), "invalid sample time");
    auto other = Skeleton({Bone("different", -1), Bone("child", 0)});
    Check(!Sample(other, clip, 0, TimeMode::Clip, pose), "different bone layout rejection");
    Check(!Sample(skeleton, RuntimeAnimationClip{}, 0, TimeMode::Clip, pose), "uninitialized clip rejection");

    PlaybackClock clock;
    for (double rate : {0.5, 1.0, 2.0})
    {
        clock = {0, rate, false};
        Check(clock.Advance(0.25), "playback advance");
        Near(clock.time, rate * 0.25, "playback rate");
        clock.paused = true;
        Check(clock.Advance(1), "pause advance");
        Near(clock.time, rate * 0.25, "paused time retained");
        clock.paused = false;
        Check(clock.Advance(0.25), "resume advance");
        Near(clock.time, rate * 0.5, "resumed time");
    }
    Check(!clock.Advance(std::numeric_limits<double>::quiet_NaN()), "invalid clock delta");
}

void InvalidClips()
{
    auto skeleton = Skeleton({Bone("root", -1)});
    auto invalid = [&](AnimationTrack track, double duration = 2) {
        RuntimeAnimationClip clip;
        std::string error;
        Check(!clip.Initialize("bad", duration, true, {std::move(track)}, skeleton, error) && !error.empty(), "invalid clip rejected");
    };
    auto track = AnimatedTrack();
    track.targetBone = 1;
    invalid(track);
    track = AnimatedTrack();
    track.translation.keys[1].time = 0;
    invalid(track);
    track.translation.keys[1].time = -1;
    invalid(track);
    track.translation.keys[1].time = 3;
    invalid(track);
    track.translation.keys[1].time = std::numeric_limits<double>::quiet_NaN();
    invalid(track);
    track = AnimatedTrack();
    track.rotation.keys[0].value = {0, 0, 0, 0};
    invalid(track);
    track = AnimatedTrack();
    track.scaleShear.keys[0].value[0] = std::numeric_limits<float>::infinity();
    invalid(track);
    track = AnimatedTrack();
    track.translation.interpolation = static_cast<Interpolation>(99);
    invalid(track);
    invalid(AnimatedTrack(), -1);
    invalid(AnimatedTrack(), std::numeric_limits<double>::infinity());
    RuntimeAnimationClip clip;
    std::string error;
    Check(!clip.Initialize("duplicate", 2, false, {AnimatedTrack(), AnimatedTrack()}, skeleton, error), "duplicate target binding rejection");
    Check(!clip.Initialize("", 0, false, {}, skeleton, error), "empty clip name rejection");
    auto single = AnimationTrack{};
    single.translation.keys = {{0, {3, 4, 5}}};
    auto zero = Clip(skeleton, {single}, 0);
    auto pose = Pose(1);
    Check(Sample(skeleton, zero, 100, TimeMode::Loop, pose), "zero duration single key");
    Near(pose.localTransforms[0].translation[0], 3, "zero duration exact pose");
}

void BlendingAndAllocation()
{
    auto skeleton = Skeleton({Bone("root", -1), Bone("child", 0)});
    auto first = Pose(2), second = Pose(2), blended = Pose(2);
    auto idle = Clip(skeleton);
    auto moving = Clip(skeleton, {AnimatedTrack()});
    Check(Sample(skeleton, idle, 0, TimeMode::Clip, first) &&
        Sample(skeleton, moving, 2, TimeMode::Clamp, second), "two clips share skeleton");
    Check(Blend(first, second, 0.5f, blended), "pose crossfade");
    Near(blended.localTransforms[0].translation[0], 5, "blended translation");
    Near(blended.localTransforms[0].rotation[2], std::sqrt(0.5), "blended rotation");
    Near(blended.localTransforms[0].scaleShear[8], 4, "blended scale");
    Check(Blend(first, second, -1, blended), "blend weight clamps low");
    Near(blended.localTransforms[0].translation[0], 0, "blend zero endpoint");
    Check(Blend(first, second, 2, blended), "blend weight clamps high");
    Near(blended.localTransforms[0].translation[0], 10, "blend one endpoint");
    Check(Blend(first, second, 0.5f, second), "blend permits alias output");
    Near(second.localTransforms[0].translation[0], 5, "aliased blend translation");
    Check(!Blend(first, second, std::numeric_limits<float>::quiet_NaN(), blended), "invalid blend weight");
    Check(!Blend(Pose(1), second, 1, blended), "blend incompatible sizes");
    std::vector<Matrix> model(2), palette(2);
    const auto allocationsBefore = allocationCount.load();
    countAllocations = true;
    bool success = true;
    for (int frame = 0; frame < 100; ++frame)
    {
        success = Sample(skeleton, moving, frame * 0.01, TimeMode::Loop, first) && success;
        success = Blend(first, second, 0.5f, blended) && success;
        success = Evaluate(skeleton, blended, model) && success;
        success = BuildPalette(skeleton, model, palette) && success;
    }
    countAllocations = false;
    Check(success, "prepared frame pipeline");
    Check(allocationCount.load() == allocationsBefore, "frame pipeline performs no heap allocations");
}
}

int main()
{
    try
    {
        Check(GetLifetimeCounts().skeletons == 0 && GetLifetimeCounts().clips == 0, "initial runtime lifetime counts");
        Hierarchy();
        Matrices();
        SamplingAndTime();
        InvalidClips();
        BlendingAndAllocation();
        Check(GetLifetimeCounts().skeletons == 0 && GetLifetimeCounts().clips == 0, "runtime objects released");
        std::cout << "PASS: hierarchy, bind/order, sampling, time, blend, matrices, palette, invalid data, no frame allocations; live objects=0\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
