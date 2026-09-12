#pragma once
// ZiiNAN: Completed CPU-skinned PNT data only; Granny owns all animation and bones.
#include "StaticObjectRenderData.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace Renderer
{
// ZiiNAN: Bounded 5B categories; native packets do not distinguish NPC-typed pets.
enum class ActorCategory : uint32_t { Player, Npc, Mob, Unsupported };
inline ActorCategory ClassifyActor(uint32_t type, uint32_t race)
{
    if(type==6 && race<8) return ActorCategory::Player;
    if(type==1 && race<20100) return ActorCategory::Npc;
    if(type==0 && race<8000) return ActorCategory::Mob;
    return ActorCategory::Unsupported;
}
struct ActorModelSource
{
    uint32_t vertexCount = 0;
    std::vector<uint16_t> indices;
    // ZiiNAN: Rigid pieces embedded in PART_MAIN, not attachment parts.
    uint32_t deformVertexCount = 0;
    std::vector<StaticObjectVertex> rigidVertices;
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
    virtual bool UpdateVertices(const StaticObjectGeometryPtr&, const std::vector<StaticObjectVertex>&, uint32_t deformedCount = 0) = 0;
    virtual void Draw(const void* actor, const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&, ActorCategory category = ActorCategory::Player) = 0;
    virtual void ReleaseBindings() = 0;
};
inline IActorRenderer* actorRenderer = nullptr;
inline bool actorWorldFrame = false;
inline uint64_t actorFrameSerial = 0; // ZiiNAN: Reject poses not deformed for the current world frame.
inline const void* actorDeformTarget = nullptr;
// ZiiNAN: Observe only the selected native body draw after its material is applied.
struct ActorNativeDraw
{
    uint32_t mesh, material, firstIndex, indexCount, baseVertex, vertexCount;
    bool rigid;
};
struct ActorDrawTarget
{
    const void* instance = nullptr;
    void* context = nullptr;
    void (*submit)(void*, const ActorNativeDraw&) = nullptr;
};
inline ActorDrawTarget actorDrawTarget;
inline void SubmitActorNativeDraw(const void* instance, const ActorNativeDraw& draw)
{
    if(actorDrawTarget.instance==instance && actorDrawTarget.submit)
        actorDrawTarget.submit(actorDrawTarget.context,draw);
}
struct ActorDrawScope
{
    ActorDrawTarget previous = actorDrawTarget;
    explicit ActorDrawScope(ActorDrawTarget target) { actorDrawTarget=target; }
    ~ActorDrawScope() { actorDrawTarget=previous; }
    ActorDrawScope(const ActorDrawScope&) = delete;
    ActorDrawScope& operator=(const ActorDrawScope&) = delete;
};
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
