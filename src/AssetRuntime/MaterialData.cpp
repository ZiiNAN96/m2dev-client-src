#include "AssetRuntime.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <set>
#include <sstream>

namespace AssetRuntime
{
namespace {
float Clamp(float v, float fallback, float low, float high)
{ return std::isfinite(v) ? std::clamp(v,low,high) : fallback; }
bool SafePath(const std::string& path)
{ return path.size() <= 1024 && path.find("..") == std::string::npos && path.find_first_of("\r\n\t") == std::string::npos; }
}
PBRMaterialData ValidateMaterial(PBRMaterialData p)
{
    for(auto& c:p.baseColor) c=Clamp(c,1,0,1);
    for(auto& c:p.emissive) c=Clamp(c,0,0,1);
    p.metallic=Clamp(p.metallic,0,0,1);
    p.roughness=Clamp(p.roughness,.8f,MinimumRoughness,1);
    // glTF permits any finite signed normal scale; it is not a 0..1 factor.
    if(!std::isfinite(p.normalScale))p.normalScale=1;
    p.occlusionStrength=Clamp(p.occlusionStrength,1,0,1);
    p.alphaCutoff=Clamp(p.alphaCutoff,.5f,0,1);
    if(unsigned(p.alpha)>2) p.alpha=AlphaMode::Opaque;
    for(auto& map:p.maps) {
        if(!SafePath(map.path)) map={};
        for(float v:map.uvTransform) if(!std::isfinite(v)) {map.uvTransform={1,0,0,0,1,0};break;}
    }
    return p;
}
PBRMaterialData ResolveMaterial(const MaterialAsset& material)
{
    if(material.model==MaterialModel::PBRMetallicRoughness) return ValidateMaterial(material.pbr);
    PBRMaterialData p;
    p.baseColor=material.baseColorFactor;
    p.maps[0].path=material.textures[0];p.maps[0].image=material.embeddedImages[0];
    p.alpha=material.blending?AlphaMode::Blend:material.alphaTest?AlphaMode::Mask:AlphaMode::Opaque;
    p.alphaCutoff=material.alphaCutoff;p.doubleSided=material.culling==Culling::None;
    return ValidateMaterial(p);
}
MaterialOverrideResult ParseMaterialOverrides(std::string_view text)
{
    MaterialOverrideResult result;
    auto fail=[&](const char* error){result.entries.clear();result.error=error;return result;};
    if(text.size()>65536) return fail("Material override exceeds 64 KiB");
    std::istringstream input{std::string(text)};input.imbue(std::locale::classic());
    std::string line,key;bool version=false;MaterialOverride* current=nullptr;
    std::set<std::pair<std::uint32_t,std::uint32_t>> identities;
    while(std::getline(input,line)) {
        std::istringstream row(line);row.imbue(std::locale::classic());
        if(!(row>>key)||key.starts_with('#'))continue;
        if(key=="VERSION") {int v=-1;if(version||!(row>>v)||v!=1)return fail("Unsupported material override version");version=true;}
        else if(key=="MATERIAL") {
            if(!version||result.entries.size()>=256)return fail("Missing version or too many material overrides");
            MaterialOverride entry;
            if(!(row>>entry.model>>entry.material>>std::quoted(entry.name))||entry.name.size()>1024||
                !identities.emplace(entry.model,entry.material).second)return fail("Invalid or duplicate material identity");
            result.entries.push_back(std::move(entry));current=&result.entries.back();
        } else {
            if(!current)return fail("Material property without identity");
            auto& p=current->data;
            if(key=="BASE_COLOR") {for(auto& f:p.baseColor)if(!(row>>f))return fail("Invalid base color");}
            else if(key=="EMISSIVE") {for(auto& f:p.emissive)if(!(row>>f))return fail("Invalid emissive");}
            else if(key=="METALLIC") {if(!(row>>p.metallic))return fail("Invalid metallic factor");}
            else if(key=="ROUGHNESS") {if(!(row>>p.roughness))return fail("Invalid roughness factor");}
            else if(key=="NORMAL_SCALE") {if(!(row>>p.normalScale))return fail("Invalid normal scale");}
            else if(key=="OCCLUSION_STRENGTH") {if(!(row>>p.occlusionStrength))return fail("Invalid occlusion strength");}
            else if(key=="MAP") {
                unsigned slot=MaterialMapCount;
                if(!(row>>slot)||slot>=MaterialMapCount||!(row>>std::quoted(p.maps[slot].path))||!SafePath(p.maps[slot].path))return fail("Invalid material map");
            } else return fail("Unsupported material override property");
        }
        std::string extra;if((row>>extra)&&!extra.starts_with('#'))return fail("Trailing material override data");
    }
    if(!version)return fail("Missing material override version");
    for(auto& entry:result.entries)entry.data=ValidateMaterial(entry.data);
    return result;
}
std::string SerializeMaterialOverrides(const std::vector<MaterialOverride>& entries)
{
    std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(std::numeric_limits<float>::max_digits10)<<"VERSION 1\n";
    for(const auto& entry:entries) {
        const auto p=ValidateMaterial(entry.data);
        out<<"MATERIAL "<<entry.model<<' '<<entry.material<<' '<<std::quoted(entry.name)<<"\nBASE_COLOR";
        for(float f:p.baseColor)out<<' '<<f;
        out<<"\nEMISSIVE";for(float f:p.emissive)out<<' '<<f;
        out<<"\nMETALLIC "<<p.metallic<<"\nROUGHNESS "<<p.roughness<<"\nNORMAL_SCALE "<<p.normalScale<<"\nOCCLUSION_STRENGTH "<<p.occlusionStrength<<'\n';
        for(unsigned i=0;i<MaterialMapCount;++i)if(!p.maps[i].path.empty())out<<"MAP "<<i<<' '<<std::quoted(p.maps[i].path)<<'\n';
    }
    return out.str();
}
bool ApplyMaterialOverride(MaterialAsset& material,std::uint32_t model,std::uint32_t index,const std::vector<MaterialOverride>& entries)
{
    for(const auto& entry:entries) if(entry.model==model&&entry.material==index&&entry.name==material.name) {
        auto p=entry.data;
        // Optional modern maps never change legacy texture identities or alpha/cull state.
        if(!p.maps[0].Present()){p.maps[0].path=material.textures[0];p.maps[0].image=material.embeddedImages[0];}
        p.alpha=material.blending?AlphaMode::Blend:material.alphaTest?AlphaMode::Mask:AlphaMode::Opaque;
        p.alphaCutoff=material.alphaCutoff;p.doubleSided=material.culling==Culling::None;
        material.pbr=ValidateMaterial(p);material.model=MaterialModel::PBRMetallicRoughness;return true;
    }
    return false;
}
bool AssetDocument::ApplyMaterialOverrides(std::string_view text)
{
    const auto parsed=ParseMaterialOverrides(text);
    if(!parsed)return false;
    for(const auto& entry:parsed.entries)
        if(entry.model>=models_.size()||entry.material>=models_[entry.model].materials.size()||
           models_[entry.model].materials[entry.material].name!=entry.name)return false;
    for(const auto& entry:parsed.entries)
        ApplyMaterialOverride(models_[entry.model].materials[entry.material],entry.model,entry.material,parsed.entries);
    materialOverrideApplications+=parsed.entries.size();
    return true;
}
}
