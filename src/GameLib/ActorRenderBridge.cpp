#include "StdAfx.h"
#include "ActorRenderBridge.h"
#include "ActorInstance.h"
#include "StaticObjectBridge.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include <fstream>

// ZiiNAN: Bridge only finished native body vertices and the current diffuse material contract.
namespace
{
std::ofstream diagnostics;
CGrannyModelInstance* Body(CActorInstance& actor)
{
    // ZiiNAN: IsPC alone is not a playable-body contract (e.g. white-lion mount race 20114).
    if(!Renderer::actorRenderer || !Renderer::actorWorldFrame || !actor.IsPC() || actor.IsPoly() ||
       actor.GetLODControllerCount()<=CRaceData::PART_MAIN) return nullptr;
    return actor.GetLODControllerPointer(CRaceData::PART_MAIN)->GetModelInstance();
}
void Report(CActorInstance& actor, CGrannyModelInstance& instance, const char* status)
{
    if(!instance.GetActorRenderData().reports.insert(status).second) return;
    if(!diagnostics.is_open()) diagnostics.open("actor-renderer.log",std::ios::trunc);
    auto* thing=actor.GetBaseThingPtr(); const auto& p=actor.GetPosition();
    diagnostics << status << " race=" << actor.GetRace() << " file=" << (thing ? thing->GetFileName() : "unknown")
                << " position=" << p.x << ',' << p.y << ',' << p.z
                << " vertices=" << instance.GetModel()->GetDeformVertexCount() << std::endl;
}
}
void ReportAnimatedActorExclusion(CActorInstance& actor, const char* status)
{
    if(auto* instance=Body(actor); instance && instance->GetModel()) Report(actor,*instance,status);
}
void SubmitAnimatedActorBody(CActorInstance& actor)
{
    using namespace Renderer;
    auto* instance=Body(actor);
    if(!instance || !instance->GetModel() || !actor.isShow()) return;
    auto* model=instance->GetModel(); auto& data=instance->GetActorRenderData();
    const auto& source=model->GetActorSource();
    if(!source || model->HaveBlendThing()) { Report(actor,*instance,"excluded: non-deformed/opacity actor model"); return; }
    if(!data.ready || data.capturedFrame!=actorFrameSerial || data.vertices.size()!=source->vertexCount) {
        Report(actor,*instance,"excluded: no current CPU-deformed pose"); return;
    }
    StaticObjectDraw common;
    if(!CaptureStaticMapObjectDraw(common,false,false,true)) { Report(actor,*instance,"excluded: actor render state"); return; }
    auto& palette=instance->GetStaticObjectMaterialPalette();
    // Validate every used body group before submitting any of it. Attachments are never traversed.
    auto* first=model->GetMeshNodeList(CGrannyMesh::TYPE_DEFORM,CGrannyMaterial::TYPE_DIFFUSE_PNT);
    if(!first) { Report(actor,*instance,"excluded: no deformable diffuse groups"); return; }
    for(auto* node=first;node;node=node->pNextMeshNode)
    for(auto* group=node->pMesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode) {
        if(group->mtrlIndex>=palette.GetMaterialCount()) { Report(actor,*instance,"excluded: actor material index"); return; }
        auto& material=palette.GetMaterialRef(group->mtrlIndex);
        if(material.GetType()!=CGrannyMaterial::TYPE_DIFFUSE_PNT || material.IsSpecularEnabled() ||
           !material.GetImagePointer(0) || material.GetImagePointer(1)) {
            Report(actor,*instance,"excluded: actor specular/opacity material"); return;
        }
    }
    if(!data.geometry) data.geometry=actorRenderer->CreateGeometry(*source);
    if(!data.geometry) { Report(actor,*instance,"ERROR: actor geometry upload"); return; }
    if(data.uploadedRevision!=data.revision) {
        if(!actorRenderer->UpdateVertices(data.geometry,data.vertices)) { Report(actor,*instance,"ERROR: actor vertex upload"); return; }
        data.uploadedRevision=data.revision;
    }
    for(auto* node=first;node;node=node->pNextMeshNode) {
        const auto* world=instance->GetStaticObjectWorldMatrix(node->iMesh);
        if(!world) { Report(actor,*instance,"ERROR: actor mesh matrix"); return; }
        auto draw=common;
        memcpy(draw.matrices.world.data(),world,64);
        D3DXMATRIX view,normal; memcpy(&view,draw.matrices.view.data(),64);
        normal=(*world)*view;
        if(!D3DXMatrixInverse(&normal,nullptr,&normal)) { Report(actor,*instance,"excluded: singular actor matrix"); return; }
        D3DXMatrixTranspose(&normal,&normal); memcpy(draw.normalTransform.data(),&normal,64);
        draw.baseVertex=node->pMesh->GetVertexBasePosition(); draw.vertexCount=node->pMesh->GetVertexCount();
        for(auto* group=node->pMesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode) {
            auto& material=palette.GetMaterialRef(group->mtrlIndex);
            const std::string name=material.GetImagePointer(0)->GetFileName();
            auto& texture=data.textures[name];
            if(!texture) texture=LoadStaticObjectTextureFile(name.c_str(),*actorRenderer);
            if(!texture) { Report(actor,*instance,"ERROR: actor texture upload"); return; }
            draw.cull=material.IsTwoSided() ? StaticObjectCull::None : common.cull;
            draw.firstIndex=group->idxPos; draw.indexCount=group->triCount*3;
            actorRenderer->Draw(&actor,data.geometry,texture,draw);
        }
    }
    Report(actor,*instance,"submitted: CPU-skinned main body");
}
