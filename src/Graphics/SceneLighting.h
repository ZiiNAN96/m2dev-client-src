#pragma once
#include <array>
#include <cstdint>

namespace Graphics
{
using LightVector = std::array<float,3>;
// Z-up, world space. Direction points FROM the surface TO the sun.
// Colors are linear RGB [0,1]. Intensities are bounded LDR multipliers, not lux.
struct DirectionalLight
{
    LightVector direction{-.577350269f,-.577350269f,.577350269f};
    LightVector color{1,1,1};
    float intensity{1};
    bool operator==(const DirectionalLight&) const = default;
};
struct SceneLighting
{
    DirectionalLight sun;
    LightVector ambientSkyColor{.21404114f,.21404114f,.21404114f};
    LightVector ambientGroundColor{.08561646f,.08561646f,.08561646f};
    float ambientIntensity{1};
    bool operator==(const SceneLighting&) const = default;
};
// Adapter input only: old maps store encoded colors and direction of light travel.
struct MapLighting
{
    LightVector direction{.5f,.5f,-.5f}, diffuse{1,1,1}, ambient{.5f,.5f,.5f};
    LightVector materialDiffuse{1,1,1}, materialAmbient{1,1,1}, materialEmissive{};
    bool sunEnabled{true};
    bool operator==(const MapLighting&) const = default;
};
SceneLighting ValidateLighting(SceneLighting);
SceneLighting ConvertMapLighting(const MapLighting&);
float LightSRGBToLinear(float);
// One main-thread scene value. Set at environment boundaries; readers never parse maps.
class SceneLightingState
{
public:
    bool Set(SceneLighting);
    bool SetMap(const MapLighting&);
    const SceneLighting& Get() const { return value_; }
    std::uint64_t Revision() const { return revision_; }
private:
    SceneLighting value_;
    MapLighting lastMap_;
    bool hasMap_{};
    std::uint64_t revision_{1};
};
}
