#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include "TerrainTextureData.h"
#include "TerrainSplatData.h"

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
    virtual TerrainTexturePtr UploadTexture(const TerrainTextureData& data) = 0;
    virtual void ReleaseTexture(TerrainTexturePtr& texture) = 0;
    virtual void BeginTerrain(const TerrainMatrices& matrices, bool legacyStatesMatch,
                              const std::array<float,16>* textureTransform = nullptr) = 0;
    virtual void DrawTerrain(const TerrainBufferPtr& vertices, const TerrainBufferPtr& indices,
                             uint32_t indexCount, bool triangleStrip, const TerrainTexturePtr& texture = {}) = 0;
    virtual TerrainSplatMaterialPtr CreateSplatMaterial(const TerrainTexturePtr& color, const TerrainTexturePtr& alpha) = 0;
    virtual void ReleaseSplatMaterial(TerrainSplatMaterialPtr& material) = 0;
    virtual void SetSplatVertices(const TerrainSplatVertex* vertices, uint32_t count) = 0;
    virtual void DrawSplat(const TerrainBufferPtr&, const TerrainBufferPtr&, uint32_t count, bool strip,
                          const TerrainSplatMaterialPtr&, const TerrainSplatParameters&) = 0;
    virtual void DrawTerrainSolid(const TerrainBufferPtr&, const TerrainBufferPtr&, uint32_t count, bool strip,
                                  const std::array<float,4>& color) = 0;
};
// Bound only during startup, before maps/loaders exist; unbound after map destruction.
// Null is the legacy default. No backend selection or switching in terrain code.
inline ITerrainRenderer* terrainRenderer = nullptr;
}
