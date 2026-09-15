#include "AssetRuntime/AssetRuntime.h"
#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include "Graphics/GraphicsSettings.h"
#include "../AssetRuntime/GlTFFixtures.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace AssetRuntime;
static void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
static void Near(float a,float b){Check(std::abs(a-b)<1e-5f,"Numeric material contract");}
static LoadResult Load(GlTFFixtures::Builder& b){return GetGlTFAssetProvider().Load("test/material.glb",b.Bytes());}
static GlTFFixtures::Builder Fixture()
{
    GlTFFixtures::Builder b;
    // Each slot points to the same encoded image: semantic/color-space identity is separate from bytes.
    const std::array<unsigned char,70> png{137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,255,255,255,127,0,9,251,3,253,42,134,227,138,0,0,0,0,73,69,78,68,174,66,96,130};
    auto view=b.View(png);
    b.extra=R"(,"images":[{"bufferView":)"+std::to_string(view)+R"(,"mimeType":"image/png"}],"textures":[{"source":0}],"materials":[{
      "name":"channels","pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.6,0.8],"metallicFactor":0.7,"roughnessFactor":0.3,
      "baseColorTexture":{"index":0},"metallicRoughnessTexture":{"index":0}},"normalTexture":{"index":0,"scale":0.6},
      "occlusionTexture":{"index":0,"strength":0.4},"emissiveFactor":[0.1,0.2,0.3],"emissiveTexture":{"index":0},
      "alphaMode":"MASK","alphaCutoff":0.25,"doubleSided":true}])";
    return b;
}
static void Mapping()
{
    auto fixture=Fixture();auto result=Load(fixture);Check(bool(result),result.diagnostic.c_str());
    const auto& material=result.asset.Model(0).Get()->materials[0];const auto& p=material.pbr;
    Check(material.model==MaterialModel::PBRMetallicRoughness,"glTF selects neutral PBR model");
    Near(p.baseColor[0],.2f);Near(p.baseColor[3],.8f);Near(p.metallic,.7f);Near(p.roughness,.3f);
    Near(p.normalScale,.6f);Near(p.occlusionStrength,.4f);Near(p.emissive[2],.3f);Near(p.alphaCutoff,.25f);
    Check(p.alpha==AlphaMode::Mask&&p.doubleSided,"alpha/culling mapping");
    for(const auto& map:p.maps)Check(map.image==p.maps[0].image&&map.Present(),"shared encoded image for all channels");
    Check(MapColorSpace(MaterialMap::BaseColor)==ColorSpace::SRGB&&MapColorSpace(MaterialMap::Emissive)==ColorSpace::SRGB,"color textures sRGB");
    for(auto map:{MaterialMap::Normal,MaterialMap::MetallicRoughness,MaterialMap::Occlusion})Check(MapColorSpace(map)==ColorSpace::Linear,"data textures linear");
    MaterialAsset legacy;legacy.textures[0]="old.dds";const auto fallback=ResolveMaterial(legacy);
    Near(fallback.metallic,0);Near(fallback.roughness,.8f);Near(fallback.occlusionStrength,1);
    Check(fallback.maps[0].path=="old.dds"&&!fallback.maps[1].Present(),"safe diffuse-only legacy defaults");
    GlTFFixtures::Builder defaults;defaults.extra=R"(,"materials":[{"name":"implicit-pbr","emissiveFactor":[0.2,0.3,0.4]}])";
    auto implicit=Load(defaults);Check(bool(implicit),"implicit glTF material");
    const auto& implicitMaterial=implicit.asset.Model(0).Get()->materials[0];
    Check(implicitMaterial.model==MaterialModel::PBRMetallicRoughness,"implicit glTF PBR model");
    Near(implicitMaterial.pbr.metallic,1);Near(implicitMaterial.pbr.roughness,1);Near(implicitMaterial.pbr.emissive[1],.3f);
    GlTFFixtures::Builder absent;auto defaultAsset=Load(absent);Check(bool(defaultAsset),"absent glTF material");
    const auto& defaultMaterial=defaultAsset.asset.Model(0).Get()->materials.back();
    Check(defaultMaterial.model==MaterialModel::PBRMetallicRoughness,"default glTF PBR model");Near(defaultMaterial.pbr.metallic,1);
}
static void Validation()
{
    PBRMaterialData p;p.metallic=9;p.roughness=0;p.normalScale=std::numeric_limits<float>::quiet_NaN();
    p.occlusionStrength=-9;p.baseColor[0]=std::numeric_limits<float>::infinity();p.emissive[0]=-1;p.alpha=static_cast<AlphaMode>(99);
    p.maps[1].path="../escape.png";p=ValidateMaterial(p);
    Near(p.metallic,1);Near(p.roughness,MinimumRoughness);Near(p.normalScale,1);Near(p.occlusionStrength,0);Near(p.baseColor[0],1);Near(p.emissive[0],0);
    Check(!p.maps[1].Present()&&p.alpha==AlphaMode::Opaque,"invalid map/mode safe defaults");
    p.normalScale=-2;Near(ValidateMaterial(p).normalScale,-2);
    p.normalScale=16;Near(ValidateMaterial(p).normalScale,16);
    auto fixture=Fixture();const auto pos=fixture.extra.find("\"metallicFactor\":0.7");fixture.extra.replace(pos,20,"\"metallicFactor\":9.0");
    auto result=Load(fixture);Check(bool(result),result.diagnostic.c_str());Near(result.asset.Model(0).Get()->materials[0].pbr.metallic,1);
    MaterialAsset unknown;unknown.model=static_cast<MaterialModel>(99);unknown.pbr.metallic=1;
    Near(ResolveMaterial(unknown).metallic,0);
    GlTFFixtures::Builder unlit;unlit.extra=R"(,"materials":[{"extensions":{"KHR_materials_unlit":{}}}])";
    auto unsupported=Load(unlit);Check(bool(unsupported),"optional unsupported material has safe approximation");
    Check(unsupported.asset.Model(0).Get()->materials[0].model==MaterialModel::Legacy,"unsupported optional model uses neutral legacy fallback");
}
static void Overrides()
{
    MaterialAsset m;m.name="brick";m.textures[0]="old.dds";m.alphaTest=true;
    MaterialOverride o;o.model=2;o.material=3;o.name="brick";o.data.roughness=.27f;o.data.metallic=.8f;o.data.maps[1].path="d:/ymir work/proof/normal.png";
    const auto text=SerializeMaterialOverrides({o});const auto parsed=ParseMaterialOverrides(text);
    Check(bool(parsed)&&parsed.entries.size()==1,"versioned override roundtrip");
    Check(text==SerializeMaterialOverrides(parsed.entries),"deterministic serialization");
    Check(!ApplyMaterialOverride(m,2,4,parsed.entries),"material index mismatch does not apply");
    Check(ApplyMaterialOverride(m,2,3,parsed.entries),"stable index and material name select override");
    Check(m.textures[0]=="old.dds"&&m.alphaTest&&m.model==MaterialModel::PBRMetallicRoughness,"legacy data and geometry preserved");
    Near(m.pbr.roughness,.27f);Check(m.pbr.maps[0].path=="old.dds","optional base map keeps legacy texture");
    for(const auto* invalid:{"VERSION 2\n","VERSION 1\nMAP 0 \"x\"\n","VERSION 1\nMATERIAL 0 0 \"x\"\nMAP 8 \"x\"\n","VERSION 1\nMATERIAL 0 0 \"x\"\nMATERIAL 0 0 \"x\"\n"})Check(!ParseMaterialOverrides(invalid),"invalid override rejected");
    Check(!ParseMaterialOverrides(std::string(65537,'x')),"bounded override input");
    auto fixture=Fixture();auto asset=Load(fixture);Check(bool(asset),"fixture loaded");
    Check(!asset.asset.Get()->ApplyMaterialOverrides(text),"document rejects mismatched identity without partial edits");
}
static void Tangents()
{
    GlTFFixtures::Builder b;
    const auto view=b.View(std::array<float,12>{1,0,0,-1,1,0,0,-1,1,0,0,-1});
    const auto attr=b.Accessor(view,5126,3,"VEC4");b.attributes+=",\"TANGENT\":"+std::to_string(attr);
    b.nodes=R"({"mesh":0,"scale":[-2,3,4]})";
    auto result=Load(b);Check(bool(result),result.diagnostic.c_str());
    const auto& mesh=result.asset.Model(0).Get()->meshes[0];
    Check(mesh.materialVertices.size()==mesh.vertexCount,"parallel material vertex stream");
    Near(mesh.materialVertices[0].tangent[3],1);Near(mesh.materialVertices[0].tangent[0],-1);
    Check(mesh.materialVertices[0].tangent==mesh.tangents[0],"converted handedness reaches renderer stream");
}
static void Settings()
{
    Graphics::Store store;
    for(auto style:{Graphics::GraphicsStyle::Modern,Graphics::GraphicsStyle::Classic}){
        auto s=store.GetGraphicsSettings();s.style=style;Check(store.ApplyGraphicsSettings(s),"live style accepted");
        const auto event=store.ConsumeChanges();Check(event.runtime.usePBR==(style==Graphics::GraphicsStyle::Modern),"central PBR capability follows style");
        const auto loaded=Graphics::LoadGraphicsSettings(Graphics::SaveGraphicsSettings(s));
        Check(loaded.settings.style==style&&Graphics::Resolve(loaded.settings).usePBR==event.runtime.usePBR,"style persistence");
        Check(!event.runtime.hdr&&!event.runtime.bloom&&event.runtime.ambientOcclusion==Graphics::AmbientOcclusionQuality::Off,"G2+ features stay disabled");
    }
}
int main(int argc,char** argv)
{
    try{Check(argc==2,"mode required");const std::string mode=argv[1];
        if(mode=="Mapping")Mapping();else if(mode=="Validation")Validation();else if(mode=="Overrides")Overrides();else if(mode=="Tangents")Tangents();else if(mode=="Settings")Settings();else throw std::runtime_error("unknown mode");
        Check(liveDocuments==0&&Graphics::liveSettingsStores==0,"core objects released");std::cout<<"PASS Materials."<<mode<<'\n';return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
