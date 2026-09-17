#pragma once
#include "AssetRuntime/AssetRuntime.h"
#include "TerrainTextureData.h"

namespace Renderer
{
// Neutral material binding: shared image handles, no device or importer types.
struct MaterialRuntimeData
{
    std::array<TerrainTexturePtr,AssetRuntime::MaterialTextureCount> textures;
    std::array<float,4> baseColor{1,1,1,1};
    std::array<float,3> emissive{};
    float roughness{.85f},metallic{},normalScale{1.f},occlusionStrength{1.f};
    std::uint8_t roughnessChannel{},metallicChannel{},occlusionChannel{};
    AssetRuntime::MaterialModel model{AssetRuntime::MaterialModel::Legacy};
};
}
