#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace Graphics
{
// World semantics only. Direction points along the sun's rays, in world space.
struct SceneLighting
{
    std::array<float,3> sunDirection{0.f,0.f,-1.f};
    std::array<float,3> sunColor{1.f,1.f,1.f};
    float sunIntensity{};
    std::array<float,3> ambient{.25f,.25f,.25f};
    std::array<float,3> environmentColor{};
    std::array<float,3> fogColor{};
    float fogNear{},fogFar{1.f},fogDensity{};
    bool fogEnabled{},densityFog{};
};

inline SceneLighting ValidateSceneLighting(SceneLighting light)
{
    float length=0;
    for(float v:light.sunDirection) length+=v*v;
    if(!std::isfinite(length)||length<1e-12f) {
        light.sunDirection={0,0,-1}; light.sunIntensity=0;
    } else for(auto& v:light.sunDirection) v/=std::sqrt(length);
    auto nonnegative=[](float v) {return std::isfinite(v)?std::max(v,0.f):0.f;};
    light.sunIntensity=nonnegative(light.sunIntensity);
    for(auto* values:{&light.sunColor,&light.ambient,&light.environmentColor,&light.fogColor})
        for(auto& v:*values) v=nonnegative(v);
    if(!std::isfinite(light.fogNear)||!std::isfinite(light.fogFar)||light.fogNear>=light.fogFar)light.fogEnabled=false;
    light.fogDensity=nonnegative(light.fogDensity);
    return light;
}
struct LegacyEnvironmentLight
{
    std::array<float,3> direction{},diffuse{},ambient{},materialDiffuse{},materialAmbient{},environmentFill{};
    bool enabled{};
};
inline SceneLighting ResolveLegacyEnvironmentLight(const LegacyEnvironmentLight& source)
{
    SceneLighting light;light.sunDirection=source.direction;
    // Legacy light strength multiplies diffuse reflectance directly. FX uses
    // irradiance with a 1/pi diffuse BRDF, so convert the authored unit once.
    light.sunIntensity=source.enabled?3.14159265358979323846f:0.f;
    for(unsigned i=0;i<3;++i) {
        light.sunColor[i]=source.diffuse[i]*source.materialDiffuse[i];
        // Map Material.Emissive was a world-wide ambient fill, not emissive
        // asset content. It remains indirect illumination and receives AO.
        light.ambient[i]=source.environmentFill[i]+source.materialAmbient[i]*source.ambient[i];
    }
    return ValidateSceneLighting(light);
}
}
