#pragma once
#include "Renderer/ResourceData.h"
#include "Renderer/SkinningData.h"
#include <ostream>

namespace Renderer
{
// ZiiNAN: Removed final D3D9 compile-time dependency. Report actual CPU owners only.
inline void WriteSourceResourceAudit(std::ostream& output)
{
    output << "SourceTextures=" << liveSourceTextures
           << " SourceBuffers=" << liveSourceBuffers << std::endl;
    // ZiiNAN: GPU skinning static mesh data
    output << "SkinMeshes=" << liveSkinMeshes << " BoneRemaps=" << liveBoneRemaps
           << " BonePalettes=" << liveBonePalettes << " PaletteUpdates=" << skinPaletteUpdates
           << " SkinPreparationFailures=" << skinSidecarFailures << std::endl;
}
}
