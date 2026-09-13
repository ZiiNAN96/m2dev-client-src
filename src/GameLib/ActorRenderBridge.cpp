#include "StdAfx.h"
#include "EterLib/NativeStateView.h"
#include "ActorRenderBridge.h"
#include "ActorInstance.h"
#include "StaticObjectBridge.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include "EterLib/StateManager.h"
#include <fstream>

// ZiiNAN: Diligent mount actor rendering
static Renderer::ActorCategory RenderCategory(CActorInstance& actor)
{
    static_assert(CActorInstance::TYPE_PC==6 && CActorInstance::TYPE_NPC==1 && CActorInstance::TYPE_ENEMY==0);
    const auto ordinary=Renderer::ClassifyActor(actor.GetActorType(),actor.GetRace());
    return Renderer::actorMountPair.Classify(&actor,ordinary);
}
bool IsDiligentActorCandidate(CActorInstance& actor)
{ return RenderCategory(actor)!=Renderer::ActorCategory::Unsupported; }
namespace
{
using namespace Renderer;
std::ofstream diagnostics;
// ZiiNAN: Bounded, read-only evidence for rejected native states, never per-frame logging.
struct ActorStateDiagnostic : CGraphicBase
{
    static void Write(std::ostream& out)
    {
        out << "capture rejected frame=" << actorFrameSerial << '\n';
        for(auto state:{D3DRS_ZENABLE,D3DRS_ZFUNC,D3DRS_ZWRITEENABLE,D3DRS_CULLMODE,
            D3DRS_ALPHATESTENABLE,D3DRS_ALPHAFUNC,D3DRS_ALPHAREF,D3DRS_ALPHABLENDENABLE,
            D3DRS_SRCBLEND,D3DRS_DESTBLEND,D3DRS_BLENDOP,D3DRS_SEPARATEALPHABLENDENABLE,
            D3DRS_COLORVERTEX,D3DRS_SPECULARENABLE,D3DRS_LIGHTING,D3DRS_FOGENABLE,
            D3DRS_FOGVERTEXMODE,D3DRS_FOGTABLEMODE,D3DRS_FOGSTART,D3DRS_FOGEND})
            out << "state " << state << '=' << STATEMANAGER.GetRenderState(state) << '\n';
        for(DWORD stage=0;stage<2;++stage) {
            for(auto state:{D3DTSS_COLOROP,D3DTSS_COLORARG1,D3DTSS_COLORARG2,D3DTSS_ALPHAOP,
                D3DTSS_ALPHAARG1,D3DTSS_ALPHAARG2,D3DTSS_TEXCOORDINDEX,D3DTSS_TEXTURETRANSFORMFLAGS}) {
                DWORD value=0; STATEMANAGER.GetTextureStageState(stage,state,&value);
                out << "stage " << stage << ':' << state << '=' << value << '\n';
            }
            for(auto state:{D3DSAMP_ADDRESSU,D3DSAMP_ADDRESSV,D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER,
                D3DSAMP_MIPFILTER,D3DSAMP_MAXMIPLEVEL,D3DSAMP_MIPMAPLODBIAS,D3DSAMP_MAXANISOTROPY}) {
                DWORD value=0; const auto result=NativeStateView().GetSamplerState(stage,state,&value);
                out << "sampler " << stage << ':' << state << '=' << value << " hr=" << result << '\n';
            }
        }
        for(DWORD index=0;index<8;++index) {
            BOOL enabled=FALSE; const auto result=NativeStateView().GetLightEnable(index,&enabled);
            out << "light " << index << '=' << enabled << " hr=" << result << '\n';
        }
    }
};
void Report(CActorInstance& actor, CGrannyModelInstance& instance, const std::string& status)
{
    if(!instance.GetActorRenderData().reports.insert(status).second) return;
    if(!diagnostics.is_open()) diagnostics.open("actor-renderer.log",std::ios::trunc);
    if(status.find("excluded: actor render state")==0) ActorStateDiagnostic::Write(diagnostics);
    auto* thing=actor.GetBaseThingPtr(); const auto& p=actor.GetPosition();
    diagnostics << status << " race=" << actor.GetRace() << " file=" << (thing ? thing->GetFileName() : "unknown")
                << " vid=" << actor.GetVirtualID() << " part=" << uint32_t(actorDrawTarget.targets.Find(&instance))
                << " category=" << uint32_t(RenderCategory(actor))
                << " position=" << p.x << ',' << p.y << ',' << p.z
                << " vertices=" << instance.GetModel()->GetVertexCount()
                << " deform_vertices=" << instance.GetModel()->GetDeformVertexCount()
                << " rigid_vertices=" << instance.GetModel()->GetRigidVertexCount() << std::endl;
}
void Submit(void* context, const void* nativeInstance, ActorPart part, const ActorNativeDraw& native)
{
    auto& actor=*static_cast<CActorInstance*>(context);
    // ZiiNAN: Diligent actor attachment rendering
    auto* instance=actor.GetLODControllerPointer(uint32_t(part))->GetModelInstance();
    if(!actorRenderer || !actorWorldFrame || !instance || nativeInstance!=instance || !actor.isShow()) return;
    auto* body=actor.GetLODControllerPointer(CRaceData::PART_MAIN)->GetModelInstance();
    if(part!=ActorPart::Body && (!body || !body->GetActorRenderData().ready ||
        body->GetActorRenderData().capturedFrame!=actorFrameSerial)) return;
    auto* model=instance->GetModel(); auto& data=instance->GetActorRenderData();
    // ZiiNAN: Diligent mount actor rendering
    const auto category=RenderCategory(actor);
    if(category==ActorCategory::Mount || category==ActorCategory::MountedPlayer) {
        for(const auto* member:{actorMountPair.rider,actorMountPair.mount}) {
            auto* paired=static_cast<CActorInstance*>(const_cast<void*>(member));
            auto* pairedBody=paired && paired->GetLODControllerCount()>CRaceData::PART_MAIN ?
                paired->GetLODControllerPointer(CRaceData::PART_MAIN)->GetModelInstance() : nullptr;
            if(!pairedBody || !paired->isShow() || !pairedBody->GetActorRenderData().ready ||
                pairedBody->GetActorRenderData().capturedFrame!=actorFrameSerial) {
                Report(actor,*instance,"excluded: mount pair has no current visible pose"); return;
            }
        }
    }
    const auto& source=model->GetActorSource();
    if(!source) { Report(actor,*instance,"excluded: model outside captured actor PNT contract"); return; }
    if(!data.ready || data.capturedFrame!=actorFrameSerial || (!source->IsRigid() && data.vertices.size()!=source->vertexCount)) {
        Report(actor,*instance,"excluded: no current CPU-deformed pose"); return;
    }
    StaticObjectDraw draw;
    if(!CaptureStaticMapObjectDraw(draw,false,false,true)) {
        Report(actor,*instance,"excluded: actor render state material="+std::to_string(native.material)); return;
    }
    auto& palette=instance->GetStaticObjectMaterialPalette();
    if(native.material>=palette.GetMaterialCount()) { Report(actor,*instance,"ERROR: actor material index"); return; }
    auto& material=palette.GetMaterialRef(native.material);
    // OneTexture's opacity pass uses the same native stage-0 image, not a synthetic second mask.
    auto load=[&](CGraphicImage* image) -> TerrainTexturePtr {
        if(!image) return {};
        const std::string name=image->GetFileName();
        auto& texture=data.textures[name];
        if(!texture) {
            texture=LoadStaticObjectTextureFile(name.c_str(),*actorRenderer);
            if(texture && part!=ActorPart::Body) actorRenderer->TrackAttachmentTexture(texture);
            if(texture && category==ActorCategory::Mount) actorRenderer->TrackMountTexture(texture);
        }
        return texture;
    };
    const auto texture=load(material.GetImagePointer(0));
    if(!texture) { Report(actor,*instance,"ERROR: actor diffuse texture upload"); return; }
    if(draw.actorStage==ActorMaterialStage::Specular) {
        draw.sphereMap=load(material.GetSphereMapImage());
        if(!draw.sphereMap) { Report(actor,*instance,"excluded: missing native sphere map"); return; }
    }
    const auto* world=instance->GetStaticObjectWorldMatrix(native.mesh);
    if(!world) { Report(actor,*instance,"ERROR: actor mesh matrix"); return; }
    memcpy(draw.matrices.world.data(),world,64);
    D3DXMATRIX view,normal; memcpy(&view,draw.matrices.view.data(),64);
    normal=(*world)*view;
    if(!D3DXMatrixInverse(&normal,nullptr,&normal)) { Report(actor,*instance,"excluded: singular actor matrix"); return; }
    D3DXMatrixTranspose(&normal,&normal); memcpy(draw.normalTransform.data(),&normal,64);
    draw.baseVertex=native.baseVertex+(native.rigid ? source->deformVertexCount : 0);
    draw.vertexCount=native.vertexCount; draw.firstIndex=native.firstIndex; draw.indexCount=native.indexCount;
    if(!data.geometry) data.geometry=actorRenderer->CreateGeometry(*source,part,category);
    if(!data.geometry) { Report(actor,*instance,"ERROR: actor geometry upload"); return; }
    if(!source->IsRigid() && data.uploadedRevision!=data.revision) {
        if(!actorRenderer->UpdateVertices(data.geometry,data.vertices,source->deformVertexCount,category)) {
            Report(actor,*instance,"ERROR: actor vertex upload"); return;
        }
        data.uploadedRevision=data.revision;
    }
    actorRenderer->Draw(&actor,data.geometry,texture,draw,category,part);
    Report(actor,*instance,part==ActorPart::Body ? "submitted: CPU-skinned main body" :
        source->IsRigid() ? "submitted: rigid attachment" : "submitted: CPU-skinned attachment");
    Report(actor,*instance,"material group="+std::to_string(native.material)+" stage="+std::to_string(uint32_t(draw.actorStage))+
        " alpha_test="+std::to_string(uint32_t(draw.alphaTest))+" blend="+std::to_string(draw.blend)+
        " rigid="+std::to_string(native.rigid)+" mesh="+std::to_string(native.mesh)+
        " cull="+std::to_string(uint32_t(draw.cull))+" depth_write="+std::to_string(draw.depthWrite)+
        " texture="+material.GetImagePointer(0)->GetFileName());
}
}
// ZiiNAN: Diligent actor attachment rendering
Renderer::ActorInstanceSet GetAnimatedActorParts(CActorInstance& actor)
{
    using namespace Renderer;
    static_assert(CRaceData::PART_MAIN==0 && CRaceData::PART_WEAPON==1 &&
        CRaceData::PART_WEAPON_LEFT==3 && CRaceData::PART_HAIR==4);
    ActorInstanceSet result;
    if(!actorRenderer || !actorWorldFrame || !IsDiligentActorCandidate(actor)) return result;
    for(auto part:{ActorPart::Body,ActorPart::Weapon,ActorPart::WeaponLeft,ActorPart::Hair}) {
        const auto index=uint32_t(part);
        if(index<actor.GetLODControllerCount()) result.instances[index]=actor.GetLODControllerPointer(index)->GetModelInstance();
    }
    return result;
}
Renderer::ActorDrawTarget MakeAnimatedActorTarget(CActorInstance& actor)
{
    using namespace Renderer;
    if(!actorRenderer || !actorWorldFrame || actor.GetLODControllerCount()<=CRaceData::PART_MAIN) return {};
    auto* instance=actor.GetLODControllerPointer(CRaceData::PART_MAIN)->GetModelInstance();
    if(!instance || !instance->GetModel()) return {};
    if(!IsDiligentActorCandidate(actor)) {
        Report(actor,*instance,"excluded: unknown native actor category"); return {};
    }
    return {GetAnimatedActorParts(actor),&actor,Submit};
}
Renderer::ActorMountPair MakeAnimatedMountPair(CActorInstance& rider,CActorInstance* mount)
{
    using namespace Renderer;
    if(!actorRenderer || !actorWorldFrame || !mount || mount==&rider || rider.IsPoly() ||
        ClassifyActor(rider.GetActorType(),rider.GetRace())!=ActorCategory::Player) return {};
    return {&rider,mount};
}
