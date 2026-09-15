#pragma once
#include "AssetRuntime/MaterialData.h"
#include "TerrainTextureData.h"
#include <atomic>

namespace Renderer
{
inline std::atomic_size_t liveMaterialRuntimeObjects{}, livePBRBindings{}, livePBRPipelines{};
inline std::atomic_uint64_t materialRuntimeCreations{}, pbrDraws{}, pbrTextureSamples{};
// Resolved once at material creation. The draw path only receives stable handles.
struct MaterialRuntime
{
    AssetRuntime::PBRMaterialData parameters;
    std::array<TerrainTexturePtr,AssetRuntime::MaterialMapCount> maps;
    TerrainTexturePtr classicDiffuse, classicSphere;
    bool authored{}, explicitRenderState{};
    MaterialRuntime() { ++liveMaterialRuntimeObjects; ++materialRuntimeCreations; }
    ~MaterialRuntime() { --liveMaterialRuntimeObjects; }
    MaterialRuntime(const MaterialRuntime&)=delete;
    MaterialRuntime& operator=(const MaterialRuntime&)=delete;
};
using MaterialRuntimePtr=std::shared_ptr<const MaterialRuntime>;
enum class MaterialDebugView : unsigned { Lit, BaseColor, Normal, Roughness, Metallic, Occlusion, Emissive };
// Development-only diagnostic, deliberately absent from the player settings.
inline MaterialDebugView materialDebugView=MaterialDebugView::Lit;
}
