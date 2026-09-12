#pragma once
#include "EffectRenderData.h"
#include "DiligentD3D11Backend.h"

namespace Renderer
{
// ZiiNAN: Diligent effect rendering integration on the existing world/depth surface.
class DiligentEffectRenderer final : public IEffectRenderer
{
public:
    explicit DiligentEffectRenderer(DiligentD3D11Backend&);
    ~DiligentEffectRenderer() override;
    bool Initialize();
    void ResetFrame();
    void ReleaseBindings();
    void Shutdown();
    bool Failed() const;
    void ReportFailure() override;
    TerrainTexturePtr UploadTexture(const TerrainTextureData&) override;
    void Draw(const EffectVertex*,uint32_t,const TerrainTexturePtr&,const EffectDraw&,EffectPart) override;
    uint32_t DrawCount(EffectPart) const;
    uint64_t Vertices() const;
    uint64_t UploadBytes() const;
    uint64_t BufferBytes() const;
    uint32_t LiveTextureCount() const;
    uint32_t LiveBufferCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
