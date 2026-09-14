#pragma once
#include "Renderer/SkinningData.h"
#include "AssetRuntime/AssetRuntime.h"

namespace SkinningDataAdapter
{
std::shared_ptr<const Renderer::SkinningModelData> Extract(const AssetRuntime::ModelHandle& model);
Renderer::SkinDataStatus CapturePose(Renderer::BonePalette& palette,
    std::shared_ptr<const Renderer::SkeletonLayout> skeleton, AssetRuntime::PoseView pose);
std::shared_ptr<const Renderer::BoneRemap> ExtractRemap(const Renderer::StaticSkinnedMeshData& mesh,
    std::span<const std::int32_t> mapping, std::shared_ptr<const Renderer::SkeletonLayout> destination,
    Renderer::SkinDataStatus& status);
}
