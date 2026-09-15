#pragma once
#include <array>
#include <cstdint>

namespace Graphics {
using Matrix4 = std::array<float,16>;
using Vector3 = std::array<float,3>;
inline constexpr Matrix4 Identity4{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
struct ShadowQualityConfig {
    static constexpr int RasterDepthBias=2;
    unsigned cascades{}, resolution{1024}, filterRadius{1};
    float distance{4000}, splitLambda{.65f}, casterExtension{2500};
    float depthBias{.00015f}, slopeBias{1.25f}, normalOffset{.6f};
};
struct AmbientDepthConfig {
    unsigned quality{}, divisor{2}, directions{4}, steps{4};
    float radius{60}, intensity{.45f}, fadeStart{2000}, fadeEnd{5000};
};
ShadowQualityConfig ValidateShadowConfig(ShadowQualityConfig);
AmbientDepthConfig ValidateAmbientConfig(AmbientDepthConfig);
ShadowQualityConfig ResolveShadowQuality(unsigned level, float viewDistance);
AmbientDepthConfig AmbientQuality(unsigned level);
Matrix4 Multiply(const Matrix4&,const Matrix4&);
bool Inverse(const Matrix4&,Matrix4&);
Vector3 TransformPoint(const Vector3&,const Matrix4&);
struct ShadowCascade {
    Matrix4 view{Identity4}, projection{Identity4}, worldToClip{Identity4};
    float nearDistance{}, farDistance{}, radius{}, texelSize{};
};
struct ShadowCascades {
    std::array<ShadowCascade,3> cascade{};
    unsigned count{};
};
std::array<float,3> CascadeSplits(float nearDistance,float farDistance,unsigned count,float lambda);
// Sun direction is WORLD surface -> sun, exactly SceneLighting.sun.direction.
// The light camera looks along -sun; there is no independent shadow sun state.
ShadowCascades BuildCascades(const Matrix4& view,const Matrix4& projection,
    const Vector3& sunDirection,ShadowQualityConfig);
bool IntersectsCascade(const ShadowCascade&,const Vector3& center,float radius);
}
