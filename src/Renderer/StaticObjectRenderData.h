#pragma once
#include "TerrainRenderData.h"

namespace Renderer
{
// Exactly the existing TPNT layout, not a new scene/mesh representation.
using StaticObjectVertex = std::array<float, 8>;
struct StaticObjectSource
{
    std::vector<StaticObjectVertex> vertices;
    std::vector<uint16_t> indices;
};
struct StaticObjectGeometry { virtual ~StaticObjectGeometry() = default; };
using StaticObjectGeometryPtr = std::shared_ptr<StaticObjectGeometry>;
enum class StaticObjectCull : uint32_t { None, Clockwise, CounterClockwise };
enum class StaticObjectAlphaTest : uint32_t { Disabled, GreaterEqual, Greater };
struct StaticObjectDraw
{
    TerrainMatrices matrices{};
    std::array<float,16> normalTransform{};
    std::array<float,4> ambient{1,1,1,1}, diffuse{}, lightDirection{};
    std::array<float,4> fogColor{}, fogParameters{}; // start, end, density, unused
    TerrainFog fog = TerrainFog::None;
    bool rangeFog = false, normalizeNormals = false;
    StaticObjectCull cull = StaticObjectCull::Clockwise;
    TerrainSampling sampling{};
    bool anisotropic = false;
    uint32_t maxAnisotropy = 1;
    // Only the original SRCALPHA/INVSRCALPHA blend; no generic material graph.
    bool blend = false, depthWrite = true, textureAlpha = false, diffuseAlphaOnly = false;
    StaticObjectAlphaTest alphaTest = StaticObjectAlphaTest::Disabled;
    uint32_t alphaReference = 0;
    TerrainTexturePtr cameraAlpha;
    std::array<float,16> cameraAlphaTransform{};
    TerrainSampling cameraAlphaSampling{};
    bool cameraAlphaAnisotropic = false;
    uint32_t cameraAlphaMaxAnisotropy = 1;
    // Existing point light 1 left by character selection, needed by PCBlocker MODULATE.
    std::array<float,4> pointPositionRange{}, pointAttenuation{}, pointAmbient{}, pointDiffuse{};
    uint32_t firstIndex = 0, indexCount = 0, baseVertex = 0, vertexCount = 0;
};
class IStaticObjectRenderer : public ITextureUploader
{
public:
    virtual StaticObjectGeometryPtr UploadGeometry(const StaticObjectSource&) = 0;
    virtual void Draw(const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&) = 0;
    virtual void ReleaseBindings() = 0;
};
inline IStaticObjectRenderer* staticObjectRenderer = nullptr;
inline unsigned staticObjectLoadDepth = 0;
// Synchronous map-resource loading only; never selects/switches a backend.
struct StaticObjectLoadScope
{
    const bool active = staticObjectRenderer != nullptr;
    StaticObjectLoadScope() { if(active) ++staticObjectLoadDepth; }
    ~StaticObjectLoadScope() { if(active) --staticObjectLoadDepth; }
    StaticObjectLoadScope(const StaticObjectLoadScope&) = delete;
    StaticObjectLoadScope& operator=(const StaticObjectLoadScope&) = delete;
};
}
