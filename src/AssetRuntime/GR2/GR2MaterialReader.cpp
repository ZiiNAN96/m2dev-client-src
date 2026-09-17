#include "GR2Reader.h"
#include "EterBase/MapLoadTrace.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <algorithm>

namespace AssetRuntime::GR2
{
namespace
{
std::string Lower(std::string value) { for(auto& c:value) if(c>='A'&&c<='Z') c+=('a'-'A'); return value; }
Object Map(Types& t,Object object) { return t.Child(object,t.Find(object,"Map")?"Map":"Material"); }
std::string Texture(Types& t,Object material,std::string_view usage,std::set<Ref>& seen)
{
    if(!material) return {};
    Require(seen.size()<64 && seen.insert(material.data).second,"cyclic material graph");
    const auto maps=t.Array(material,"Maps");
    for(auto map:maps) if(Lower(t.Text(map,"Usage"))==Lower(std::string(usage))) {
        auto child=Map(t,map); auto result=Texture(t,child,"Diffuse Color",seen); seen.erase(material.data); return result;
    }
    if(auto texture=t.Child(material,"Texture")) { seen.erase(material.data); return t.Text(texture,"FromFileName"); }
    seen.erase(material.data); return {};
}
std::string Texture(Types& t,Object material,std::string_view usage) { std::set<Ref> seen; return Texture(t,material,usage,seen); }
}
MaterialAsset ReadMaterial(Types& t,Object source)
{
    MapLoadTrace::Scope gr2Detail("Assets","GR2 materials");
    AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Material);
    MaterialAsset result; if(!source) return result;
    result.name=t.Text(source,"Name"); result.hasMatchingTextures=true;
    result.matchingTextures={Texture(t,source,"Diffuse Color"),Texture(t,source,"Opacity")};
    result.textures=result.matchingTextures;
    const auto maps=t.Array(source,"Maps");
    if(maps.size()>1 && Lower(result.name).starts_with("blend")) {
        result.textures={Texture(t,Map(t,maps[0]),"Diffuse Color"),Texture(t,Map(t,maps[1]),"Diffuse Color")};
    }
    result.blending=!result.textures[1].empty(); result.stage=result.blending?MaterialStage::DiffuseOpacity:MaterialStage::Diffuse;
    if(auto extended=t.Child(source,"ExtendedData");extended && t.Find(extended,"Two-sided"))
        result.culling=t.Integer(extended,"Two-sided")==1?Culling::None:Culling::Clockwise;
    return result;
}
}
