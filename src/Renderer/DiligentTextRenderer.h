#pragma once
#include "TextRenderData.h"
#include "DiligentEffectRenderer.h"

namespace Renderer
{
// ZiiNAN: Diligent text rendering integration; independent atlas/buffer ownership, same native combiner.
class DiligentTextRenderer final : public ITextRenderer
{
    DiligentEffectRenderer m_draws;
public:
    explicit DiligentTextRenderer(DiligentD3D11Backend& backend):m_draws(backend) {}
    bool Initialize() { return m_draws.Initialize(); }
    void ResetFrame() { m_draws.ResetFrame(); }
    void Shutdown() { m_draws.Shutdown(); }
    bool Failed() const { return m_draws.Failed(); }
    void ReportFailure() override { m_draws.ReportFailure(); }
    TerrainTexturePtr UploadTexture(const TerrainTextureData& data) override { return m_draws.UploadTexture(data); }
    void Draw(const EffectVertex* vertices,uint32_t count,const TerrainTexturePtr& image,const EffectDraw& d) override
    {
        if(!d.ui || d.depthTest || d.depthWrite || d.fog || d.lines) { ReportFailure(); return; }
        m_draws.Draw(vertices,count,image,d,EffectPart::Mesh);
    }
    uint32_t DrawCount() const { return m_draws.DrawCount(EffectPart::Mesh); }
    uint64_t Vertices() const { return m_draws.Vertices(); }
    uint32_t LiveTextureCount() const { return m_draws.LiveTextureCount(); }
    uint32_t LiveBufferCount() const { return m_draws.LiveBufferCount(); }
};
}
