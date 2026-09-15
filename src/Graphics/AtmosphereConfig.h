#pragma once
#include "SceneLighting.h"
#include "GraphicsSettings.h"

namespace Graphics
{
inline int developmentSunState=-1; // Only selected by the renderer-test probe.
struct AtmosphereConfig
{
    float exposure{2.f};
    float bloomThreshold{2.f},bloomIntensity{.06f},bloomRadius{.65f};
    unsigned skyWidth{128},skyHeight{64};
    bool highQualityFog{};
};
inline AtmosphereConfig ResolveAtmosphere(const SceneLighting& input,const GraphicsRuntimeConfig& settings)
{
    const auto light=ValidateSceneLighting(input);
    AtmosphereConfig result;
    result.exposure*=std::exp2(light.exposureBias);
    if(settings.modernSky){result.skyWidth=256;result.skyHeight=128;}
    result.highQualityFog=settings.highQualityFog;
    return result;
}
// Development-only fixed states. All consumers still receive the same validated
// SceneLighting direction; there is no separate sky sun or moving clock.
inline SceneLighting WithDevelopmentSun(SceneLighting light,unsigned state)
{
    constexpr std::array<std::array<float,3>,3> rays{{{-.94f,.12f,-.32f},{-.15f,.12f,-.98f},{.94f,.12f,-.32f}}};
    if(state<rays.size())light.sunDirection=rays[state];
    return ValidateSceneLighting(light);
}
}
