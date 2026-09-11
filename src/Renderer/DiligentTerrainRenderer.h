#pragma once
#include "TerrainRenderData.h"
#include "DiligentD3D11Backend.h"
#include <atomic>

namespace Renderer
{
class DiligentTerrainRenderer final : public ITerrainRenderer
{
public:
    explicit DiligentTerrainRenderer(DiligentD3D11Backend& backend);
    ~DiligentTerrainRenderer() override;
    bool Initialize();
    void ResetFrame();
    bool HasTerrain() const;
    bool Failed() const;
    uint32_t DrawCount() const;
    uint32_t TexturedDrawCount() const;
    uint32_t LiveTextureCount() const;
    uint32_t TextureUploadCount() const;
    uint32_t LiveAlphaCount() const;
    uint32_t LiveMaterialCount() const;
    uint32_t SplatDrawCount() const;
    std::array<uint32_t,3> LastTextureSize() const;
    TerrainBufferPtr UploadVertices(const void*, uint32_t count, uint32_t stride) override;
    TerrainBufferPtr UploadIndices(const uint16_t*, uint32_t count) override;
    TerrainTexturePtr UploadTexture(const TerrainTextureData&) override;
    void ReleaseTexture(TerrainTexturePtr&) override;
    void BeginTerrain(const TerrainMatrices&, bool legacyStatesMatch,
                      const std::array<float,16>* textureTransform = nullptr) override;
    void DrawTerrain(const TerrainBufferPtr&, const TerrainBufferPtr&, uint32_t count, bool strip,
                     const TerrainTexturePtr& texture = {}) override;
    TerrainSplatMaterialPtr CreateSplatMaterial(const TerrainTexturePtr&, const TerrainTexturePtr&) override;
    void ReleaseSplatMaterial(TerrainSplatMaterialPtr&) override;
    void SetSplatVertices(const TerrainSplatVertex*, uint32_t count) override;
    void DrawSplat(const TerrainBufferPtr&, const TerrainBufferPtr&, uint32_t count, bool strip,
                  const TerrainSplatMaterialPtr&, const TerrainSplatParameters&) override;
    void DrawTerrainSolid(const TerrainBufferPtr&, const TerrainBufferPtr&, uint32_t count, bool strip,
                          const std::array<float,4>& color) override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
