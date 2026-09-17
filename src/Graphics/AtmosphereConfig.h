#pragma once
#include "SceneLighting.h"
#include "GraphicsSettings.h"

namespace Graphics
{
inline int developmentSunState=-1; // Only selected by the renderer-test probe.
inline double developmentSkySeconds=-1; // Negative selects the production monotonic clock.
// Periodic offsets are computed in double precision before upload. One cycle
// takes 40 minutes, independent of frame rate or time spent loading a map.
inline std::array<float,2> CloudOffset(double seconds)
{
    if(!std::isfinite(seconds))return {};
    const auto wrap=[](double x){return float(x-std::floor(x));};
    return {wrap(seconds/2400.0),wrap(seconds/4800.0)};
}
struct AtmosphereConfig
{
    float exposure{2.f};
    float bloomThreshold{2.f},bloomIntensity{.06f},bloomRadius{.65f};
    unsigned skyWidth{128},skyHeight{64};
};
inline AtmosphereConfig ResolveAtmosphere(const SceneLighting& input,const GraphicsRuntimeConfig& settings)
{
    const auto light=ValidateSceneLighting(input);
    AtmosphereConfig result;
    result.exposure*=std::exp2(light.exposureBias);
    if(settings.modernSky){result.skyWidth=512;result.skyHeight=256;}
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
