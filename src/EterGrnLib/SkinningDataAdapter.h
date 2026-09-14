#pragma once
#include "Renderer/SkinningData.h"
#include "AssetRuntime/Granny/NativeTypes.h"
#include "AssetRuntime/AssetRuntime.h"

namespace SkinningDataAdapter
{
std::shared_ptr<const Renderer::SkinningModelData> Extract(const AssetRuntime::ModelHandle& model);
std::shared_ptr<const Renderer::SkinningModelData> Extract(const granny_model& model,
    const AssetRuntime::ModelAsset* asset = nullptr);
std::shared_ptr<const Renderer::BoneRemap> ExtractRemap(const Renderer::StaticSkinnedMeshData& mesh,
    const granny_mesh_binding* binding, std::shared_ptr<const Renderer::SkeletonLayout> destination,
    Renderer::SkinDataStatus& status);
Renderer::SkinDataStatus CapturePose(Renderer::BonePalette& palette,
    std::shared_ptr<const Renderer::SkeletonLayout> skeleton, const granny_world_pose* pose);
Renderer::SkinDataStatus CapturePose(Renderer::BonePalette& palette,
    std::shared_ptr<const Renderer::SkeletonLayout> skeleton, AssetRuntime::PoseView pose);
std::shared_ptr<const Renderer::BoneRemap> ExtractRemap(const Renderer::StaticSkinnedMeshData& mesh,
    std::span<const std::int32_t> mapping, std::shared_ptr<const Renderer::SkeletonLayout> destination,
    Renderer::SkinDataStatus& status);
}
