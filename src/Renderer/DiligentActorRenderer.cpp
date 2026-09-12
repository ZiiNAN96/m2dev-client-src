#include "DiligentActorRenderer.h"
// ZiiNAN: One discard upload per completed pose, not per material group.
namespace Renderer
{
void DiligentActorRenderer::ResetFrame()
{
    m_meshes.ResetFrame(); m_actors.clear(); m_uploads=0; m_vertices=0; m_skinnedVertices=0;
}
StaticObjectGeometryPtr DiligentActorRenderer::CreateGeometry(const ActorModelSource& data)
{
    StaticObjectSource source;
    source.vertices.resize(data.vertexCount);
    source.indices=data.indices;
    auto geometry=m_meshes.UploadDynamicGeometry(source);
    if(geometry) ++m_indexUploads;
    return geometry;
}
bool DiligentActorRenderer::UpdateVertices(const StaticObjectGeometryPtr& geometry, const std::vector<StaticObjectVertex>& vertices, uint32_t deformedCount)
{
    if(deformedCount>vertices.size()) return false;
    if(!m_meshes.UpdateDynamicVertices(geometry,vertices)) return false;
    ++m_uploads; m_vertices+=vertices.size(); m_skinnedVertices+=deformedCount ? deformedCount : vertices.size(); return true;
}
void DiligentActorRenderer::Draw(const void* actor, const StaticObjectGeometryPtr& geometry, const TerrainTexturePtr& texture, const StaticObjectDraw& draw, ActorCategory category)
{
    if(category==ActorCategory::Unsupported) return;
    const auto before=m_meshes.DrawCount();
    m_meshes.Draw(geometry,texture,draw);
    if(m_meshes.DrawCount()!=before) m_actors.emplace(actor,category);
}
uint32_t DiligentActorRenderer::Visible(ActorCategory category) const
{
    uint32_t count=0;
    for(const auto& actor:m_actors) if(actor.second==category) ++count;
    return count;
}
}
