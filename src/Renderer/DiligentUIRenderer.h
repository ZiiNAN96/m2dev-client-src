#pragma once
#include "UIRenderData.h"
#include "DiligentEffectRenderer.h"

namespace Renderer
{
// ZiiNAN: Separate UI ownership over the proven material/texture/dynamic-upload binder.
class DiligentUIRenderer final : public IUIRenderer
{
    DiligentEffectRenderer m_draws;
    uint32_t m_quads=0,m_lines=0,m_scissors=0,m_binds=0;
public:
    explicit DiligentUIRenderer(DiligentD3D11Backend& backend):m_draws(backend) {}
    bool Initialize() { return m_draws.Initialize(); }
    void ResetFrame() { m_draws.ResetFrame(); m_quads=m_lines=m_scissors=m_binds=0; }
    void Shutdown() { m_draws.Shutdown(); }
    bool Failed() const { return m_draws.Failed(); }
    void ReportFailure() override { m_draws.ReportFailure(); }
    TerrainTexturePtr UploadTexture(const TerrainTextureData& data) override { return m_draws.UploadTexture(data); }
    void Draw(const EffectVertex* vertices,uint32_t count,const TerrainTexturePtr& image,const EffectDraw& d) override
    {
        if(!d.ui || (!d.floatingText && (d.depthTest || d.depthWrite)) || d.fog) { ReportFailure(); return; }
        m_draws.Draw(vertices,count,image,d,EffectPart::Mesh);
        if(Failed()) return;
        if(d.lines) m_lines+=count/2; else if(d.strip && count==4) ++m_quads;
        if(d.scissor) ++m_scissors;
        if(d.textured) ++m_binds;
    }
    uint32_t DrawCount() const { return m_draws.DrawCount(EffectPart::Mesh); }
    uint32_t Quads() const { return m_quads; }
    uint32_t Lines() const { return m_lines; }
    uint32_t ScissorBinds() const { return m_scissors; }
    uint32_t TextureBinds() const { return m_binds; }
    uint64_t Vertices() const { return m_draws.Vertices(); }
    uint32_t LiveTextureCount() const { return m_draws.LiveTextureCount(); }
    uint32_t LiveBufferCount() const { return m_draws.LiveBufferCount(); }
};
}
