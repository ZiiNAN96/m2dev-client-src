#pragma once
#include "Renderer/ResourceData.h"
#include "Renderer/SkinningData.h"
#include "Graphics/GraphicsSettings.h"
#include "Renderer/MaterialRuntime.h"
#include <ostream>

namespace Renderer
{
// ZiiNAN: Removed final D3D9 compile-time dependency. Report actual CPU owners only.
inline void WriteSourceResourceAudit(std::ostream& output)
{
    output << "GraphicsSettingsObjects=" << Graphics::liveSettingsStores << std::endl;
    output << "MaterialRuntimeObjects=" << liveMaterialRuntimeObjects << " PBRBindings=" << livePBRBindings
           << " PBRPipelines=" << livePBRPipelines << " MaterialCreations=" << materialRuntimeCreations
           << " PBRDraws=" << pbrDraws << " PBREnabledSamplesPerDrawSum=" << pbrTextureSamples
           << " MaterialOverrides=" << AssetRuntime::materialOverrideApplications << std::endl;
    output << "SourceTextures=" << liveSourceTextures
           << " SourceBuffers=" << liveSourceBuffers << std::endl;
    // ZiiNAN: GPU skinning static mesh data
    output << "SkinMeshes=" << liveSkinMeshes << " BoneRemaps=" << liveBoneRemaps
           << " BonePalettes=" << liveBonePalettes << " PaletteUpdates=" << skinPaletteUpdates
           << " SkinPreparationFailures=" << skinSidecarFailures << std::endl;
}
}
