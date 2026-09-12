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
    // ZiiNAN: Diligent actor attachment rendering
    uint32_t deformVertexCount = 0;
    std::vector<StaticObjectVertex> rigidVertices;
    bool IsRigid() const { return vertexCount && !deformVertexCount && rigidVertices.size()==vertexCount; }
};
enum class ActorPart : uint32_t { Body=0, Weapon=1, WeaponLeft=3, Hair=4, Unsupported=5 };
struct ActorInstanceSet
{
    std::array<const void*,5> instances{};
    ActorPart Find(const void* instance) const
    {
        if(instance) for(auto part:{ActorPart::Body,ActorPart::Weapon,ActorPart::WeaponLeft,ActorPart::Hair})
            if(instances[uint32_t(part)]==instance) return part;
        return ActorPart::Unsupported;
    }
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
    virtual StaticObjectGeometryPtr CreateGeometry(const ActorModelSource&, ActorPart part = ActorPart::Body) = 0;
    virtual bool UpdateVertices(const StaticObjectGeometryPtr&, const std::vector<StaticObjectVertex>&, uint32_t deformedCount = 0) = 0;
    virtual void Draw(const void* actor, const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&, ActorCategory category = ActorCategory::Player, ActorPart part = ActorPart::Body) = 0;
    virtual void TrackAttachmentTexture(const TerrainTexturePtr&) {}
    virtual void ReleaseBindings() = 0;
};
inline IActorRenderer* actorRenderer = nullptr;
inline bool actorWorldFrame = false;
inline uint64_t actorFrameSerial = 0; // ZiiNAN: Reject poses not deformed for the current world frame.
inline ActorInstanceSet actorDeformTargets;
// ZiiNAN: Diligent actor attachment rendering
struct ActorNativeDraw
{
    uint32_t mesh, material, firstIndex, indexCount, baseVertex, vertexCount;
    bool rigid;
};
struct ActorDrawTarget
{
    ActorInstanceSet targets;
    void* context = nullptr;
    void (*submit)(void*, const void*, ActorPart, const ActorNativeDraw&) = nullptr;
};
inline ActorDrawTarget actorDrawTarget;
inline void SubmitActorNativeDraw(const void* instance, const ActorNativeDraw& draw)
{
    const auto part=actorDrawTarget.targets.Find(instance);
    if(part!=ActorPart::Unsupported && actorDrawTarget.submit)
        actorDrawTarget.submit(actorDrawTarget.context,instance,part,draw);
}
struct ActorDrawScope
{
    ActorDrawTarget previous = actorDrawTarget;
    explicit ActorDrawScope(ActorDrawTarget target) { actorDrawTarget=target; }
    ~ActorDrawScope() { actorDrawTarget=previous; }
    ActorDrawScope(const ActorDrawScope&) = delete;
    ActorDrawScope& operator=(const ActorDrawScope&) = delete;
};
// ZiiNAN: Diligent actor attachment rendering
struct ActorDeformScope
{
    ActorInstanceSet previous = actorDeformTargets;
    explicit ActorDeformScope(ActorInstanceSet targets)
    { actorDeformTargets = actorRenderer && actorWorldFrame ? targets : ActorInstanceSet{}; }
    ~ActorDeformScope() { actorDeformTargets = previous; }
    ActorDeformScope(const ActorDeformScope&) = delete;
    ActorDeformScope& operator=(const ActorDeformScope&) = delete;
};
}
