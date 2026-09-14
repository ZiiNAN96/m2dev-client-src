#pragma once
// ZiiNAN: Completed CPU-skinned PNT data only; Granny owns all animation and bones.
#include "StaticObjectRenderData.h"
#include "GpuSkinningPrototype.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

namespace Renderer
{
// ZiiNAN: All native actor types share the existing geometry/material contract.
enum class ActorCategory : uint32_t { Player, Npc, Mob, Mount, MountedPlayer, Special, Unsupported };
inline ActorCategory ClassifyActor(uint32_t type, uint32_t race)
{
    if(type==6 && race<8) return ActorCategory::Player;
    if(type==1) return ActorCategory::Npc;
    if(type==0 || type==2 || type==7) return ActorCategory::Mob;
    if(type<=10) return ActorCategory::Special;
    return ActorCategory::Unsupported;
}
struct ActorModelSource
{
    uint32_t vertexCount = 0;
    std::vector<uint16_t> indices;
    // ZiiNAN: Diligent actor attachment rendering
    uint32_t deformVertexCount = 0;
    std::vector<StaticObjectVertex> rigidVertices;
    std::vector<uint32_t> indices32;
    bool IsRigid() const { return vertexCount && !deformVertexCount && rigidVertices.size()==vertexCount; }
};
enum class ActorPart : uint32_t { Body=0, Weapon=1, WeaponLeft=3, Hair=4, Unsupported=5 };
struct ActorInstanceSet
{
    std::array<const void*,5> instances{};
    const void* prototypeBody=nullptr;
    // ZiiNAN: GPU skinning actor coverage; explicit CPU-only scopes remain usable by parity tests.
    bool gpuSkinning=false;
    ActorCategory category=ActorCategory::Player;
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
    bool gpuPrototype = false;
    StaticObjectGeometryPtr geometry;
    std::unordered_map<std::string,TerrainTexturePtr> textures;
    std::unordered_set<std::string> reports;
};
class IActorRenderer : public ITextureUploader
{
public:
    // ZiiNAN: Diligent GPU skinning prototype
    virtual bool PreparePrototype(StaticObjectGeometryPtr&, const SkinningModelData&,
        const std::vector<std::shared_ptr<const BoneRemap>>&, const BonePalette&, const ActorModelSource* = nullptr,
        ActorPart = ActorPart::Body, ActorCategory = ActorCategory::Player) { return false; }
    virtual StaticObjectGeometryPtr CreateGeometry(const ActorModelSource&, ActorPart part = ActorPart::Body, ActorCategory category = ActorCategory::Player) = 0;
    virtual bool UpdateVertices(const StaticObjectGeometryPtr&, const std::vector<StaticObjectVertex>&, uint32_t deformedCount = 0, ActorCategory category = ActorCategory::Player) = 0;
    virtual void Draw(const void* actor, const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&, ActorCategory category = ActorCategory::Player, ActorPart part = ActorPart::Body) = 0;
    virtual void TrackAttachmentTexture(const TerrainTexturePtr&) {}
    virtual void TrackMountTexture(const TerrainTexturePtr&) {}
    virtual void ReleaseBindings() = 0;
};
inline IActorRenderer* actorRenderer = nullptr;
inline bool actorWorldFrame = false;
// ZiiNAN: Character-select uses the same pose/material renderer in an explicit scope.
struct ActorPreviewScope
{
    bool previous=actorWorldFrame;
    explicit ActorPreviewScope(bool active) { if(active) actorWorldFrame=true; }
    ~ActorPreviewScope() { actorWorldFrame=previous; }
    ActorPreviewScope(const ActorPreviewScope&)=delete;
    ActorPreviewScope& operator=(const ActorPreviewScope&)=delete;
};
inline uint64_t actorFrameSerial = 0; // ZiiNAN: Reject poses not deformed for the current world frame.
// ZiiNAN: Diligent mount actor rendering
struct ActorMountPair
{
    const void* rider = nullptr;
    const void* mount = nullptr;
    explicit operator bool() const { return rider && mount && rider!=mount; }
    ActorCategory Classify(const void* actor, ActorCategory ordinary) const
    {
        if(*this && actor==mount) return ActorCategory::Mount;
        if(*this && actor==rider && ordinary==ActorCategory::Player) return ActorCategory::MountedPlayer;
        return ordinary;
    }
};
inline ActorMountPair actorMountPair;
struct ActorMountScope
{
    ActorMountPair previous = actorMountPair;
    explicit ActorMountScope(ActorMountPair pair) { actorMountPair = pair; }
    ~ActorMountScope() { actorMountPair = previous; }
    ActorMountScope(const ActorMountScope&) = delete;
    ActorMountScope& operator=(const ActorMountScope&) = delete;
};
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
// ZiiNAN: Scoped map Things reuse the same PNT draw hook, never the actor list.
struct ThingDrawTarget { void* context=nullptr; void (*submit)(void*,const void*,const ActorNativeDraw&)=nullptr; };
inline ThingDrawTarget thingDrawTarget;
struct ThingDrawScope
{
    ThingDrawTarget previous=thingDrawTarget;
    explicit ThingDrawScope(ThingDrawTarget target) { thingDrawTarget=target; }
    ~ThingDrawScope() { thingDrawTarget=previous; }
    ThingDrawScope(const ThingDrawScope&)=delete;
    ThingDrawScope& operator=(const ThingDrawScope&)=delete;
};
inline void SubmitActorNativeDraw(const void* instance, const ActorNativeDraw& draw)
{
    const auto part=actorDrawTarget.targets.Find(instance);
    if(part!=ActorPart::Unsupported && actorDrawTarget.submit)
        actorDrawTarget.submit(actorDrawTarget.context,instance,part,draw);
    else if(thingDrawTarget.submit) thingDrawTarget.submit(thingDrawTarget.context,instance,draw);
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
