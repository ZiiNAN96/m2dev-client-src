#pragma once
#include "AssetRuntime.h"
#include <cmath>
#include <set>

namespace AssetRuntime
{
// Versioned import-time contract. The caller resolves a sidecar or tool manifest
// before publishing immutable assets; rendering never reads files or names.
struct MaterialOverride
{
    std::string materialName;
    std::array<MaterialTextureAsset,MaterialTextureCount> maps;
    std::array<float,4> baseColor{1,1,1,1};
    std::array<float,3> emissive{};
    float roughness{.85f},metallic{},normalScale{1},occlusionStrength{1};
};
struct MaterialOverrideSet
{
    std::uint32_t version{1};
    AssetId assetId;
    std::vector<MaterialOverride> materials;
};

// Exact provider identities, no basename matching, order-dependent wildcards,
// partial updates, alpha-state changes or format-specific shader choices.
inline bool ApplyMaterialOverrides(std::string_view assetId,
                                   std::vector<MaterialAsset>& materials,
                                   const MaterialOverrideSet& overrides,
                                   std::string& error)
{
    error.clear();
    auto fail=[&](const char* text){error=text;return false;};
    if(overrides.version!=1)return fail("Unsupported material override version");
    if(assetId!=overrides.assetId)return fail("Material override asset identity mismatch");
    std::set<std::string> names;
    auto unit=[](float v){return std::isfinite(v)&&v>=0&&v<=1;};
    for(const auto& entry:overrides.materials) {
        if(entry.materialName.empty()||!names.insert(entry.materialName).second)return fail("Duplicate or empty material override name");
        std::size_t matches=0;for(const auto& material:materials)if(material.name==entry.materialName)++matches;
        if(matches!=1)return fail("Material override target must resolve exactly once");
        if(!unit(entry.roughness)||!unit(entry.metallic)||!unit(entry.occlusionStrength)||
            !std::isfinite(entry.normalScale)||entry.normalScale<0)return fail("Invalid material override factors");
        for(float value:entry.baseColor)if(!unit(value))return fail("Invalid material override base color");
        for(float value:entry.emissive)if(!std::isfinite(value)||value<0)return fail("Invalid material override emission");
        for(const auto& map:entry.maps)if(map.channel>3||map.texcoord!=0)return fail("Unsupported material override texture channel");
    }
    auto updated=materials;
    for(const auto& entry:overrides.materials)for(auto& material:updated)if(material.name==entry.materialName) {
        material.model=MaterialModel::PBRMetallicRoughness;
        material.baseColorFactor=entry.baseColor;material.emissiveColor=entry.emissive;
        material.roughness=entry.roughness;material.metallic=entry.metallic;
        material.normalScale=entry.normalScale;material.occlusionStrength=entry.occlusionStrength;
        for(std::size_t i=0;i<entry.maps.size();++i)if(!entry.maps[i].id.empty()||entry.maps[i].image)material.materialTextures[i]=entry.maps[i];
        if(!entry.maps[0].id.empty()||entry.maps[0].image){material.textures[0]=entry.maps[0].id;material.embeddedImages[0]=entry.maps[0].image;}
    }
    materials.swap(updated);return true;
}
}
