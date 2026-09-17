#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include "Graphics/SceneLighting.h"
#include "AssetRuntime/MaterialOverrides.h"
#include "../AssetRuntime/GlTFFixtures.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace AssetRuntime;
void Check(bool value,const char* message) {if(!value)throw std::runtime_error(message);}
LoadResult Load(std::string materials)
{
    GlTFFixtures::Builder fixture;
    fixture.indices=R"(,"material":0)";
    fixture.extra=R"(,"images":[{"uri":"d:/ymir work/material.dds"}],"textures":[{"source":0}],"materials":[)"+materials+"]";
    return GetGlTFAssetProvider().Load("material-fixture.glb",fixture.Bytes());
}
int main()
{
    try {
        auto loaded=Load("{}");Check(bool(loaded),loaded.diagnostic.c_str());
        auto material=loaded.asset.Model(0).Get()->materials[0];
        Check(material.model==MaterialModel::Legacy && material.metallic==0,"GLB without authored PBR stays diffuse");
        Check(material.normalScale==1&&material.occlusionStrength==1&&material.alphaMode==AlphaMode::Opaque,"absent texture defaults");
        loaded=Load(R"({"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.3,0.4,0.5],"baseColorTexture":{"index":0},"metallicFactor":0.7,"roughnessFactor":0.3,"metallicRoughnessTexture":{"index":0}},"normalTexture":{"index":0,"scale":0.4},"occlusionTexture":{"index":0,"strength":0.6},"emissiveFactor":[0.1,0.2,0.3],"emissiveTexture":{"index":0},"alphaMode":"MASK","alphaCutoff":0.25,"doubleSided":true})");
        Check(bool(loaded),loaded.diagnostic.c_str());material=loaded.asset.Model(0).Get()->materials[0];
        Check(material.model==MaterialModel::PBRMetallicRoughness,"authored GLB opts into PBR");
        Check(material.materialTextures[2].channel==1&&material.materialTextures[3].channel==2,"MR texture G/B channels");
        for(const auto& texture:material.materialTextures)Check(texture.id=="d:/ymir work/material.dds","all material maps retained");
        Check(material.metallic==.7f&&material.roughness==.3f&&material.normalScale==.4f&&material.occlusionStrength==.6f,"material scalar factors");
        Check(material.emissiveColor[2]==.3f&&material.baseColorFactor[3]==.5f,"linear color factors retained");
        Check(material.alphaMode==AlphaMode::Mask&&material.alphaCutoff==.25f&&material.doubleSided,"alpha and sidedness");
        Check(!Load(R"({"pbrMetallicRoughness":{"roughnessFactor":-1}})"),"negative roughness rejected");
        Check(!Load(R"({"pbrMetallicRoughness":{"metallicFactor":2}})"),"out of range metallic rejected");
        Check(!Load(R"({"occlusionTexture":{"index":0,"strength":2}})"),"out of range AO rejected");
        Check(!Load(R"({"normalTexture":{"index":0,"texCoord":1}})"),"unsupported UV domain explicitly rejected");
        MaterialAsset legacy;
        Check(legacy.model==MaterialModel::Legacy&&legacy.metallic==0&&legacy.roughness==.85f,"GR2 defaults to legacy diffuse");
        Check(legacy.emissiveColor==std::array<float,3>{}&&legacy.occlusionStrength==1,"neutral legacy AO/emissive");
        legacy.name="body";legacy.textures[0]="bear.dds";
        std::vector<MaterialAsset> overridden{legacy};MaterialOverrideSet overrides;overrides.assetId="bear.gr2";
        overrides.materials.push_back({});overrides.materials[0].materialName="body";
        overrides.materials[0].maps[1].id="bear_normal.dds";overrides.materials[0].roughness=.6f;
        std::string error;Check(ApplyMaterialOverrides("bear.gr2",overridden,overrides,error),"versioned neutral override");
        Check(overridden[0].model==MaterialModel::PBRMetallicRoughness,"explicit legacy override opts into PBR");
        Check(overridden[0].textures[0]=="bear.dds"&&overridden[0].materialTextures[1].id=="bear_normal.dds"&&overridden[0].roughness==.6f,"override keeps original diffuse and alpha state");
        overrides.materials.push_back(overrides.materials[0]);
        Check(!ApplyMaterialOverrides("bear.gr2",overridden,overrides,error)&&overridden[0].roughness==.6f,"duplicate override rejected atomically");
        overrides.materials.pop_back();overrides.version=2;
        Check(!ApplyMaterialOverrides("bear.gr2",overridden,overrides,error),"future override schema rejected");
        overrides.version=1;Check(!ApplyMaterialOverrides("other/bear.gr2",overridden,overrides,error),"exact override asset identity");
        Graphics::SceneLighting light;light.sunDirection={2,0,-2};light.sunIntensity=2;
        Graphics::LegacyEnvironmentLight environment{{.35f,.56f,-.75f},{1,1,1},{0,0,0},{1,1,1},{.8f,.8f,.8f},{.32f,.32f,.41f},true};
        const auto mapped=Graphics::ResolveLegacyEnvironmentLight(environment);
        Check(mapped.ambient==environment.environmentFill&&mapped.sunIntensity>3.14f,"map ambient fill survives zero background ambient");
        environment.enabled=false;Check(Graphics::ResolveLegacyEnvironmentLight(environment).sunIntensity==0,"disabled map sun stays disabled");
        light=Graphics::ValidateSceneLighting(light);
        Check(std::abs(light.sunDirection[0]-std::sqrt(.5f))<1e-6f&&light.sunIntensity==2,"single normalized sun");
        light.sunDirection[0]=std::numeric_limits<float>::quiet_NaN();
        light=Graphics::ValidateSceneLighting(light);
        Check(light.sunIntensity==0&&light.sunDirection[2]==-1,"invalid sun has deterministic safe fallback");
        std::cout<<"G-DX material mapping and scene-lighting contracts PASS\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
