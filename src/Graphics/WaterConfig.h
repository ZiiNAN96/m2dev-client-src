#pragma once
#include "GraphicsSettings.h"
#include <array>
#include <algorithm>
#include <cmath>

namespace Graphics
{
// World units are centimetres. Material defaults apply to every existing map.
struct WaterMaterial
{
    float roughness{.16f}, normalStrength{.14f}, refractionPixels{3.f};
    float shoreDistance{35.f}, maxDepth{2500.f};
    std::array<float,3> absorption{.0018f,.00075f,.00045f};
    std::array<float,3> deepColor{.018f,.085f,.105f};
    std::array<float,2> uvScale{1.f/650.f,1.f/370.f};
    std::array<float,4> uvSpeed{.019f,.011f,-.013f,.017f};
};
inline WaterMaterial ValidateWater(WaterMaterial value)
{
    const WaterMaterial defaults;
    auto clamp=[](float v,float fallback,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
    value.roughness=clamp(value.roughness,defaults.roughness,.07f,.5f);
    value.normalStrength=clamp(value.normalStrength,defaults.normalStrength,0,.4f);
    value.refractionPixels=clamp(value.refractionPixels,defaults.refractionPixels,0,8);
    value.shoreDistance=clamp(value.shoreDistance,defaults.shoreDistance,1,150);
    value.maxDepth=clamp(value.maxDepth,defaults.maxDepth,1,10000);
    for(unsigned i=0;i<3;++i) {
        value.absorption[i]=clamp(value.absorption[i],defaults.absorption[i],0,.01f);
        value.deepColor[i]=clamp(value.deepColor[i],defaults.deepColor[i],0,1);
    }
    for(unsigned i=0;i<2;++i)value.uvScale[i]=clamp(value.uvScale[i],defaults.uvScale[i],.0001f,.02f);
    for(unsigned i=0;i<4;++i)value.uvSpeed[i]=clamp(value.uvSpeed[i],defaults.uvSpeed[i],-.1f,.1f);
    return value;
}
struct WaterConfig
{
    WaterMaterial material;
    bool enabled{},secondNormal{},refraction{},absorption{},ssr{},halfResolution{};
    unsigned maxTraversal{64};
};
inline WaterConfig ResolveWater(const GraphicsRuntimeConfig& graphics,WaterMaterial material={})
{
    WaterConfig result;result.material=ValidateWater(material);
    result.enabled=graphics.style==GraphicsStyle::Modern;
    if(!result.enabled)return result;
    const auto q=std::clamp(int(graphics.water),0,3);
    result.secondNormal=result.refraction=result.absorption=q>=1;
    result.ssr=q>=2;result.halfResolution=q==2;result.maxTraversal=q==3?128:64;
    return result;
}
inline constexpr float WaterF0=.02037318784f; // ((1.333 - 1)/(1.333 + 1))^2
inline float WaterFresnel(float cosine)
{
    cosine=std::isfinite(cosine)?std::clamp(cosine,0.f,1.f):1.f;
    return WaterF0+(1-WaterF0)*std::pow(1-cosine,5.f);
}
inline std::array<float,4> WaterUVOffset(double seconds,const WaterMaterial& material={})
{
    const auto valid=ValidateWater(material);std::array<float,4> result{};
    if(!std::isfinite(seconds))return result;
    for(unsigned i=0;i<4;++i) {const double value=seconds*valid.uvSpeed[i];result[i]=float(value-std::floor(value));}
    return result;
}
}
