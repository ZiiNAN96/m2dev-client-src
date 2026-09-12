#pragma once
#include "WorldRenderData.h"
#include "DiligentEffectRenderer.h"

namespace Renderer
{
// ZiiNAN: Separate world ownership/counters; reuse M7's deterministic GPU material binder.
class DiligentWorldRenderer final : public IWorldRenderer
{
    DiligentEffectRenderer m_draws;
    std::array<uint32_t,3> m_counts{};
    uint64_t m_waterVertices=0;
public:
    explicit DiligentWorldRenderer(DiligentD3D11Backend& backend):m_draws(backend) {}
    bool Initialize() { return m_draws.Initialize(); }
    void ResetFrame() { m_draws.ResetFrame(); m_counts.fill(0); m_waterVertices=0; }
    void Shutdown() { m_draws.Shutdown(); }
    bool Failed() const { return m_draws.Failed(); }
    void ReportFailure() override { m_draws.ReportFailure(); }
    void ReleaseBindings() override { m_draws.ReleaseBindings(); }
    TerrainTexturePtr UploadTexture(const TerrainTextureData& data) override { return m_draws.UploadTexture(data); }
    void Draw(const EffectVertex* v,uint32_t count,const TerrainTexturePtr& texture,const EffectDraw& draw,WorldPart part) override
    {
        if(uint32_t(part)>=m_counts.size()) { ReportFailure(); return; }
        m_draws.Draw(v,count,texture,draw,EffectPart::Mesh);
        if(Failed()) return;
        ++m_counts[uint32_t(part)];
        if(part==WorldPart::Water) m_waterVertices+=count;
    }
    uint32_t DrawCount(WorldPart part) const { return m_counts.at(uint32_t(part)); }
    uint64_t WaterVertices() const { return m_waterVertices; }
    uint32_t LiveTextureCount() const { return m_draws.LiveTextureCount(); }
    uint32_t LiveBufferCount() const { return m_draws.LiveBufferCount(); }
};
}
