#include "SceneLighting.h"
#include <algorithm>
#include <cmath>

namespace Graphics
{
namespace
{
float Bounded(float value,float fallback,float maximum=1)
{ return std::isfinite(value)?std::clamp(value,0.f,maximum):fallback; }
LightVector Color(LightVector value,const LightVector& fallback)
{ for(unsigned i=0;i<3;++i)value[i]=Bounded(value[i],fallback[i]);return value; }
}
float LightSRGBToLinear(float value)
{
    value=Bounded(value,.5f);
    return value<=.04045f?value/12.92f:std::pow((value+.055f)/1.055f,2.4f);
}
SceneLighting ValidateLighting(SceneLighting value)
{
    const SceneLighting defaults;
    auto& d=value.sun.direction;
    const double length=std::hypot(double(d[0]),double(d[1]),double(d[2]));
    if(!std::isfinite(length)||length<1e-12)d=defaults.sun.direction;
    else for(auto& component:d)component=float(double(component)/length);
    value.sun.color=Color(value.sun.color,defaults.sun.color);
    value.sun.intensity=Bounded(value.sun.intensity,defaults.sun.intensity,4);
    value.ambientSkyColor=Color(value.ambientSkyColor,defaults.ambientSkyColor);
    value.ambientGroundColor=Color(value.ambientGroundColor,defaults.ambientGroundColor);
    value.ambientIntensity=Bounded(value.ambientIntensity,defaults.ambientIntensity,2);
    return value;
}
SceneLighting ConvertMapLighting(const MapLighting& input)
{
    SceneLighting result;
    const auto diffuse=Color(input.diffuse,{1,1,1}),ambient=Color(input.ambient,{.5f,.5f,.5f});
    const auto materialDiffuse=Color(input.materialDiffuse,{1,1,1}),materialAmbient=Color(input.materialAmbient,{1,1,1}),emissive=Color(input.materialEmissive,{0,0,0});
    for(unsigned c=0;c<3;++c){
        result.sun.direction[c]=-input.direction[c];
        result.sun.color[c]=LightSRGBToLinear(diffuse[c])*LightSRGBToLinear(materialDiffuse[c]);
        // Old maps often carry their entire scene fill in the global material's
        // emissive term (A1/B1 background Ambient is zero). This is scene fill,
        // distinct from authored per-material emissive maps in the BRDF.
        result.ambientSkyColor[c]=std::min(1.f,LightSRGBToLinear(ambient[c])*LightSRGBToLinear(materialAmbient[c])+LightSRGBToLinear(emissive[c]));
        // Preserve the map hue; ground receives 40% of the sky approximation.
        result.ambientGroundColor[c]=result.ambientSkyColor[c]*.4f;
    }
    if(*std::max_element(result.ambientSkyColor.begin(),result.ambientSkyColor.end())==0)
        for(unsigned c=0;c<3;++c){result.ambientSkyColor[c]=.03f;result.ambientGroundColor[c]=.012f;}
    result.sun.intensity=input.sunEnabled?1.f:0.f;
    return ValidateLighting(result);
}
bool SceneLightingState::Set(SceneLighting value)
{
    hasMap_=false;
    value=ValidateLighting(value);
    if(value==value_)return false;
    value_=value;++revision_;return true;
}
bool SceneLightingState::SetMap(const MapLighting& value)
{
    if(hasMap_&&value==lastMap_)return false;
    const bool changed=Set(ConvertMapLighting(value));
    lastMap_=value;hasMap_=true;return changed;
}
}
