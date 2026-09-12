#pragma once
// ZiiNAN: Completed CPU-skinned PNT data only; Granny owns all animation and bones.
#include "StaticObjectRenderData.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace Renderer
{
struct ActorModelSource
{
    uint32_t vertexCount = 0;
    std::vector<uint16_t> indices;
};
struct ActorInstanceData
{
    std::vector<StaticObjectVertex> vertices;
    uint64_t revision = 0, uploadedRevision = 0, capturedFrame = 0;
    bool ready = false;
    StaticObjectGeometryPtr geometry;
    std::unordered_map<std::string,TerrainTexturePtr> textures;
    std::unordered_set<std::string> reports;
};
class IActorRenderer : public ITextureUploader
{
public:
    virtual StaticObjectGeometryPtr CreateGeometry(const ActorModelSource&) = 0;
    virtual bool UpdateVertices(const StaticObjectGeometryPtr&, const std::vector<StaticObjectVertex>&) = 0;
    virtual void Draw(const void* actor, const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&) = 0;
    virtual void ReleaseBindings() = 0;
};
inline IActorRenderer* actorRenderer = nullptr;
inline bool actorWorldFrame = false;
inline uint64_t actorFrameSerial = 0; // ZiiNAN: Reject poses not deformed for the current world frame.
inline const void* actorDeformTarget = nullptr;
// ZiiNAN: Select only the existing main body, never hair/weapon/horse deformations.
struct ActorDeformScope
{
    const void* previous = actorDeformTarget;
    explicit ActorDeformScope(const void* instance)
    { actorDeformTarget = actorRenderer && actorWorldFrame ? instance : nullptr; }
    ~ActorDeformScope() { actorDeformTarget = previous; }
    ActorDeformScope(const ActorDeformScope&) = delete;
    ActorDeformScope& operator=(const ActorDeformScope&) = delete;
};
}
