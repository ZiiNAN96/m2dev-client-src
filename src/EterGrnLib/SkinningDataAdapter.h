#pragma once
#include "Renderer/SkinningData.h"
#include <granny.h>

namespace SkinningDataAdapter
{
std::shared_ptr<const Renderer::SkinningModelData> Extract(const granny_model& model);
std::shared_ptr<const Renderer::BoneRemap> ExtractRemap(const Renderer::StaticSkinnedMeshData& mesh,
    const granny_mesh_binding* binding, std::shared_ptr<const Renderer::SkeletonLayout> destination,
    Renderer::SkinDataStatus& status);
Renderer::SkinDataStatus CapturePose(Renderer::BonePalette& palette,
    std::shared_ptr<const Renderer::SkeletonLayout> skeleton, const granny_world_pose* pose);
}
