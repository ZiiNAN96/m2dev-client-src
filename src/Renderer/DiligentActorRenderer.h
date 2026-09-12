#pragma once
// ZiiNAN: Dynamic actor geometry reuses the tested 4B PNT/material implementation.
#include "ActorRenderData.h"
#include "DiligentStaticObjectRenderer.h"

namespace Renderer
{
class DiligentActorRenderer final : public IActorRenderer
{
public:
    explicit DiligentActorRenderer(DiligentD3D11Backend& backend) : m_meshes(backend) {}
    bool Initialize() { return m_meshes.Initialize(); }
    bool Failed() const { return m_meshes.Failed(); }
    void ResetFrame();
    StaticObjectGeometryPtr CreateGeometry(const ActorModelSource&) override;
    bool UpdateVertices(const StaticObjectGeometryPtr&, const std::vector<StaticObjectVertex>&) override;
    TerrainTexturePtr UploadTexture(const TerrainTextureData& data) override { return m_meshes.UploadTexture(data); }
    void Draw(const void*, const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&) override;
    void ReleaseBindings() override { m_meshes.ReleaseBindings(); }
    uint32_t VisibleActors() const { return static_cast<uint32_t>(m_actors.size()); }
    uint32_t Uploads() const { return m_uploads; }
    uint64_t VerticesUploaded() const { return m_vertices; }
    uint64_t BytesUploaded() const { return m_vertices * sizeof(StaticObjectVertex); }
    uint32_t DrawCount() const { return m_meshes.DrawCount(); }
    uint32_t LiveGeometryCount() const { return m_meshes.LiveGeometryCount(); }
    uint32_t LiveTextureCount() const { return m_meshes.LiveTextureCount(); }
    uint64_t IndexUploads() const { return m_indexUploads; }
private:
    DiligentStaticObjectRenderer m_meshes;
    std::unordered_set<const void*> m_actors;
    uint32_t m_uploads = 0;
    uint64_t m_vertices = 0, m_indexUploads = 0;
};
}
