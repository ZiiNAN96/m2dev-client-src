#include "DiligentActorRenderer.h"
#include <algorithm>
// ZiiNAN: One discard upload per completed pose, not per material group.
namespace Renderer
{
void DiligentActorRenderer::ResetFrame()
{
    m_meshes.ResetFrame(); m_actors.clear(); m_uploads=0; m_vertices=0; m_skinnedVertices=0;
    m_attachmentParts.clear(); m_partDraws.fill(0);
    m_mountDraws=0; m_mountUploads=0;
    const auto expired=[](const auto& entry) { return entry.expired(); };
    m_attachmentGeometry.erase(std::remove_if(m_attachmentGeometry.begin(),m_attachmentGeometry.end(),expired),m_attachmentGeometry.end());
    m_attachmentTextures.erase(std::remove_if(m_attachmentTextures.begin(),m_attachmentTextures.end(),expired),m_attachmentTextures.end());
    m_mountGeometry.erase(std::remove_if(m_mountGeometry.begin(),m_mountGeometry.end(),expired),m_mountGeometry.end());
    m_mountTextures.erase(std::remove_if(m_mountTextures.begin(),m_mountTextures.end(),expired),m_mountTextures.end());
}
StaticObjectGeometryPtr DiligentActorRenderer::CreateGeometry(const ActorModelSource& data, ActorPart part, ActorCategory category)
{
    // ZiiNAN: Diligent actor attachment rendering
    StaticObjectSource source;
    if(data.IsRigid()) source.vertices=data.rigidVertices;
    else source.vertices.resize(data.vertexCount);
    source.indices=data.indices;
    auto geometry=data.IsRigid() ? m_meshes.UploadGeometry(source) : m_meshes.UploadDynamicGeometry(source);
    if(geometry) ++m_indexUploads;
    if(geometry && part!=ActorPart::Body) m_attachmentGeometry.emplace_back(geometry);
    // ZiiNAN: Diligent mount actor rendering
    if(geometry && category==ActorCategory::Mount) m_mountGeometry.emplace_back(geometry);
    return geometry;
}
bool DiligentActorRenderer::UpdateVertices(const StaticObjectGeometryPtr& geometry, const std::vector<StaticObjectVertex>& vertices, uint32_t deformedCount, ActorCategory category)
{
    if(deformedCount>vertices.size()) return false;
    if(!m_meshes.UpdateDynamicVertices(geometry,vertices)) return false;
    if(category==ActorCategory::Mount) ++m_mountUploads;
    ++m_uploads; m_vertices+=vertices.size(); m_skinnedVertices+=deformedCount ? deformedCount : vertices.size(); return true;
}
void DiligentActorRenderer::Draw(const void* actor, const StaticObjectGeometryPtr& geometry, const TerrainTexturePtr& texture, const StaticObjectDraw& draw, ActorCategory category, ActorPart part)
{
    if(category==ActorCategory::Unsupported || part==ActorPart::Unsupported || uint32_t(part)>=m_partDraws.size()) return;
    const auto before=m_meshes.DrawCount();
    m_meshes.Draw(geometry,texture,draw);
    if(m_meshes.DrawCount()!=before) {
        m_actors.emplace(actor,category); ++m_partDraws[uint32_t(part)];
        if(category==ActorCategory::Mount) ++m_mountDraws;
        if(part!=ActorPart::Body) m_attachmentParts[actor]|=1u<<uint32_t(part);
    }
}
void DiligentActorRenderer::TrackAttachmentTexture(const TerrainTexturePtr& texture)
{
    if(texture && std::none_of(m_attachmentTextures.begin(),m_attachmentTextures.end(),
        [&](const auto& weak) { return weak.lock()==texture; })) m_attachmentTextures.emplace_back(texture);
}
uint32_t DiligentActorRenderer::VisibleAttachments() const
{
    uint32_t count=0;
    for(const auto& entry:m_attachmentParts)
        for(auto part:{ActorPart::Weapon,ActorPart::WeaponLeft,ActorPart::Hair})
            if(entry.second & (1u<<uint32_t(part))) ++count;
    return count;
}
uint32_t DiligentActorRenderer::AttachmentGeometryCount() const
{
    return static_cast<uint32_t>(std::count_if(m_attachmentGeometry.begin(),m_attachmentGeometry.end(),[](const auto& entry) { return !entry.expired(); }));
}
uint32_t DiligentActorRenderer::AttachmentTextureCount() const
{
    return static_cast<uint32_t>(std::count_if(m_attachmentTextures.begin(),m_attachmentTextures.end(),[](const auto& entry) { return !entry.expired(); }));
}
uint32_t DiligentActorRenderer::Visible(ActorCategory category) const
{
    uint32_t count=0;
    for(const auto& actor:m_actors)
        if(actor.second==category || (category==ActorCategory::Player && actor.second==ActorCategory::MountedPlayer)) ++count;
    return count;
}
// ZiiNAN: Diligent mount actor rendering
void DiligentActorRenderer::TrackMountTexture(const TerrainTexturePtr& texture)
{
    if(texture && std::none_of(m_mountTextures.begin(),m_mountTextures.end(),
        [&](const auto& weak) { return weak.lock()==texture; })) m_mountTextures.emplace_back(texture);
}
uint32_t DiligentActorRenderer::MountGeometryCount() const
{
    return static_cast<uint32_t>(std::count_if(m_mountGeometry.begin(),m_mountGeometry.end(),[](const auto& entry) { return !entry.expired(); }));
}
uint32_t DiligentActorRenderer::MountTextureCount() const
{
    return static_cast<uint32_t>(std::count_if(m_mountTextures.begin(),m_mountTextures.end(),[](const auto& entry) { return !entry.expired(); }));
}
}
