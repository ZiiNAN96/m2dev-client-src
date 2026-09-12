#include "StdAfx.h"
#include "ActorRenderBridge.h"
#include "ActorInstance.h"
#include "StaticObjectBridge.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include "EterLib/StateManager.h"
#include <fstream>

// ZiiNAN: One scoped main-model bridge for original player, NPC and mob material draws.
bool IsDiligentActorCandidate(CActorInstance& actor)
{
    static_assert(CActorInstance::TYPE_PC==6 && CActorInstance::TYPE_NPC==1 && CActorInstance::TYPE_ENEMY==0);
    return !actor.IsPoly() &&
        Renderer::ClassifyActor(actor.GetActorType(),actor.GetRace())!=Renderer::ActorCategory::Unsupported;
}
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
                DWORD value=0; const auto result=ms_lpd3dDevice->GetSamplerState(stage,state,&value);
                out << "sampler " << stage << ':' << state << '=' << value << " hr=" << result << '\n';
            }
        }
        for(DWORD index=0;index<8;++index) {
            BOOL enabled=FALSE; const auto result=ms_lpd3dDevice->GetLightEnable(index,&enabled);
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
                << " position=" << p.x << ',' << p.y << ',' << p.z
                << " vertices=" << instance.GetModel()->GetVertexCount()
                << " deform_vertices=" << instance.GetModel()->GetDeformVertexCount()
                << " rigid_vertices=" << instance.GetModel()->GetRigidVertexCount() << std::endl;
}
void Submit(void* context, const ActorNativeDraw& native)
{
    auto& actor=*static_cast<CActorInstance*>(context);
    auto* instance=actor.GetLODControllerPointer(CRaceData::PART_MAIN)->GetModelInstance();
    if(!actorRenderer || !actorWorldFrame || !instance || actorDrawTarget.instance!=instance || !actor.isShow()) return;
    auto* model=instance->GetModel(); auto& data=instance->GetActorRenderData();
    const auto& source=model->GetActorSource();
    if(!source) { Report(actor,*instance,"excluded: main model outside PNT skin contract"); return; }
    if(!data.ready || data.capturedFrame!=actorFrameSerial || data.vertices.size()!=source->vertexCount) {
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
        if(!texture) texture=LoadStaticObjectTextureFile(name.c_str(),*actorRenderer);
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
    if(!data.geometry) data.geometry=actorRenderer->CreateGeometry(*source);
    if(!data.geometry) { Report(actor,*instance,"ERROR: actor geometry upload"); return; }
    if(data.uploadedRevision!=data.revision) {
        if(!actorRenderer->UpdateVertices(data.geometry,data.vertices,source->deformVertexCount)) {
            Report(actor,*instance,"ERROR: actor vertex upload"); return;
        }
        data.uploadedRevision=data.revision;
    }
    actorRenderer->Draw(&actor,data.geometry,texture,draw,ClassifyActor(actor.GetActorType(),actor.GetRace()));
    Report(actor,*instance,"submitted: CPU-skinned main body");
    Report(actor,*instance,"material group="+std::to_string(native.material)+" stage="+std::to_string(uint32_t(draw.actorStage))+
        " alpha_test="+std::to_string(uint32_t(draw.alphaTest))+" blend="+std::to_string(draw.blend)+
        " rigid="+std::to_string(native.rigid)+" texture="+material.GetImagePointer(0)->GetFileName());
}
}
Renderer::ActorDrawTarget MakeAnimatedActorTarget(CActorInstance& actor)
{
    using namespace Renderer;
    if(!actorRenderer || !actorWorldFrame || actor.GetLODControllerCount()<=CRaceData::PART_MAIN) return {};
    auto* instance=actor.GetLODControllerPointer(CRaceData::PART_MAIN)->GetModelInstance();
    if(!instance || !instance->GetModel()) return {};
    if(!IsDiligentActorCandidate(actor)) {
        Report(actor,*instance,"excluded: actor category outside 5B body scope"); return {};
    }
    return {instance,&actor,Submit};
}
