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
    StaticObjectGeometryPtr CreateGeometry(const ActorModelSource&, ActorPart part = ActorPart::Body, ActorCategory category = ActorCategory::Player) override;
    bool UpdateVertices(const StaticObjectGeometryPtr&, const std::vector<StaticObjectVertex>&, uint32_t deformedCount = 0, ActorCategory category = ActorCategory::Player) override;
    TerrainTexturePtr UploadTexture(const TerrainTextureData& data) override { return m_meshes.UploadTexture(data); }
    void Draw(const void*, const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&, ActorCategory category = ActorCategory::Player, ActorPart part = ActorPart::Body) override;
    // ZiiNAN: Diligent actor attachment rendering
    void TrackAttachmentTexture(const TerrainTexturePtr&) override;
    uint32_t VisibleAttachments() const;
    uint32_t WeaponDraws() const { return m_partDraws[1]+m_partDraws[3]; }
    uint32_t HairDraws() const { return m_partDraws[4]; }
    uint32_t AttachmentGeometryCount() const;
    uint32_t AttachmentTextureCount() const;
    // ZiiNAN: Diligent mount actor rendering
    void TrackMountTexture(const TerrainTexturePtr&) override;
    uint32_t MountGeometryCount() const;
    uint32_t MountTextureCount() const;
    uint32_t MountDraws() const { return m_mountDraws; }
    uint32_t MountUploads() const { return m_mountUploads; }
    void ReleaseBindings() override { m_meshes.ReleaseBindings(); }
    uint32_t VisibleActors() const { return static_cast<uint32_t>(m_actors.size()); }
    // ZiiNAN: Categories count distinct actors, not material draws.
    uint32_t Visible(ActorCategory category) const;
    uint64_t SkinnedVerticesUploaded() const { return m_skinnedVertices; }
    uint32_t Uploads() const { return m_uploads; }
    uint64_t VerticesUploaded() const { return m_vertices; }
    uint64_t BytesUploaded() const { return m_vertices * sizeof(StaticObjectVertex); }
    uint32_t DrawCount() const { return m_meshes.DrawCount(); }
    uint32_t LiveGeometryCount() const { return m_meshes.LiveGeometryCount(); }
    uint32_t LiveTextureCount() const { return m_meshes.LiveTextureCount(); }
    uint64_t IndexUploads() const { return m_indexUploads; }
private:
    DiligentStaticObjectRenderer m_meshes;
    std::unordered_map<const void*,ActorCategory> m_actors;
    std::unordered_map<const void*,uint32_t> m_attachmentParts;
    std::array<uint32_t,5> m_partDraws{};
    std::vector<std::weak_ptr<StaticObjectGeometry>> m_attachmentGeometry;
    std::vector<std::weak_ptr<TerrainTexture>> m_attachmentTextures;
    std::vector<std::weak_ptr<StaticObjectGeometry>> m_mountGeometry;
    std::vector<std::weak_ptr<TerrainTexture>> m_mountTextures;
    uint32_t m_mountDraws = 0, m_mountUploads = 0;
    uint32_t m_uploads = 0;
    uint64_t m_vertices = 0, m_indexUploads = 0, m_skinnedVertices = 0;
};
}
