#include "DiligentActorRenderer.h"
// ZiiNAN: One discard upload per completed pose, not per material group.
namespace Renderer
{
void DiligentActorRenderer::ResetFrame()
{
    m_meshes.ResetFrame(); m_actors.clear(); m_uploads=0; m_vertices=0;
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
bool DiligentActorRenderer::UpdateVertices(const StaticObjectGeometryPtr& geometry, const std::vector<StaticObjectVertex>& vertices)
{
    if(!m_meshes.UpdateDynamicVertices(geometry,vertices)) return false;
    ++m_uploads; m_vertices+=vertices.size(); return true;
}
void DiligentActorRenderer::Draw(const void* actor, const StaticObjectGeometryPtr& geometry, const TerrainTexturePtr& texture, const StaticObjectDraw& draw)
{
    const auto before=m_meshes.DrawCount();
    m_meshes.Draw(geometry,texture,draw);
    if(m_meshes.DrawCount()!=before) m_actors.insert(actor);
}
}
