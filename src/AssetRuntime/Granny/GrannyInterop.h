#pragma once
#include "AssetRuntime/AssetRuntime.h"
#include "Native.h"
#include <optional>

namespace AssetRuntime::GrannyAnimationAdapter { struct RuntimeImportCache; }

namespace AssetRuntime::GrannyInterop
{
using SourceFingerprint = std::array<std::uint8_t, 32>;
std::optional<SourceFingerprint> GetSourceFingerprint(const std::shared_ptr<AssetDocument>& document);
// Explicit temporary interop for EterGrnLib's retained upload, mixer, deformer and binding code.
// Every returned pointer is borrowed from the supplied owning handle.
granny_model* GetModel(const ModelHandle& handle);
GrannyAnimationAdapter::RuntimeImportCache* GetRuntimeImportCache(const ModelHandle& handle);
granny_animation* GetAnimation(const AnimationHandle& handle);
granny_mesh* GetMesh(const ModelHandle& handle, std::size_t mesh);
PoseView GetPoseView(const granny_world_pose* pose);
bool CopyWorldPose(const granny_world_pose* pose, std::span<Matrix4> destination);
granny_model_instance* GetAnimationInstance(AnimationInstance& instance);
granny_world_pose* EnsureWorldPose(AnimationInstance& instance);
std::unique_ptr<AnimationInstance> CreateLegacyAnimationInstance(granny_model* model);
AnimationHandle CreateLegacyAnimationHandle(granny_animation* animation);
std::unique_ptr<MeshBinding> CreateLegacyMeshBinding(granny_model* source, std::size_t mesh, AnimationInstance& destination);

class GrannyPoseEvaluator final : public PoseEvaluator
{
public:
    GrannyPoseEvaluator(granny_model_instance* model, granny_local_pose* scratch,
        granny_world_pose* world, std::int32_t boneCount)
        : model_(model), scratch_(scratch), world_(world), boneCount_(boneCount) {}
    PoseResult Evaluate(const PoseRequest& request) override;
private:
    granny_model_instance* model_;
    granny_local_pose* scratch_;
    granny_world_pose* world_;
    std::int32_t boneCount_;
};
}
