#pragma once
#include <array>
#include <cstdint>
#include <memory>

namespace Renderer
{
// Views are consumed synchronously. The original terrain layout/ownership stays in GameLib.
struct TerrainBuffer { virtual ~TerrainBuffer() = default; };
using TerrainBufferPtr = std::shared_ptr<TerrainBuffer>;
struct TerrainMatrices
{
    std::array<float, 16> world, view, projection;
};
class ITerrainRenderer
{
public:
    virtual ~ITerrainRenderer() = default;
    virtual TerrainBufferPtr UploadVertices(const void* vertices, uint32_t count, uint32_t stride) = 0;
    virtual TerrainBufferPtr UploadIndices(const uint16_t* indices, uint32_t count) = 0;
    virtual void BeginTerrain(const TerrainMatrices& matrices, bool legacyStatesMatch) = 0;
    virtual void DrawTerrain(const TerrainBufferPtr& vertices, const TerrainBufferPtr& indices,
                             uint32_t indexCount, bool triangleStrip) = 0;
};
// Bound only during startup, before maps/loaders exist; unbound after map destruction.
// Null is the legacy default. No backend selection or switching in terrain code.
inline ITerrainRenderer* terrainRenderer = nullptr;
}
