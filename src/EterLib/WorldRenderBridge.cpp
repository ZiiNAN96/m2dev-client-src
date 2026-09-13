// ZiiNAN: Diligent special world rendering; synchronous snapshots, no second simulation.
#include "StdAfx.h"
#include "EterLib/NativeStateView.h"
#include "WorldRenderBridge.h"
#include "NativeMaterialSnapshot.h"
#include "StateManager.h"
#include "GrpImage.h"
#include "StaticObjectTextureLoader.h"
#include <fstream>
#include <set>

namespace
{
using namespace Renderer;
WorldResources* owner=nullptr;
WorldPart part=WorldPart::Water;
uint64_t serial=~uint64_t(0);
std::unordered_map<void*,std::string> textureNames;
std::set<std::string> reported;
std::ofstream diagnostics;
bool Active() { return owner && worldRenderer && worldSurfaceFrame; }
void Report(const std::string& message,bool error)
{
    if(error && worldRenderer) worldRenderer->ReportFailure();
    const auto key=std::to_string(uint32_t(part))+" "+message;
    if(reported.size()>=128 || !reported.insert(key).second) return;
    if(!diagnostics.is_open()) diagnostics.open("world-renderer.log",std::ios::trunc);
    diagnostics<<(error ? "ERROR " : "")<<key<<std::endl;
}
}
WorldRenderScope::WorldRenderScope(Renderer::WorldResources& resources,Renderer::WorldPart p):previous(owner),previousPart(part)
{
    owner=worldRenderer && worldSurfaceFrame ? &resources : nullptr; part=p;
    if(serial!=worldSurfaceSerial) { serial=worldSurfaceSerial; textureNames.clear(); }
}
WorldRenderScope::~WorldRenderScope() { owner=previous; part=previousPart; }
void WorldRenderBridge::Texture(CGraphicImage* image)
{
    if(Active() && image) textureNames[image->GetTexturePointer()->GetD3DTexture()]=image->GetFileName();
}
void WorldRenderBridge::Release(Renderer::WorldResources& resources)
{
    if(worldRenderer && !resources.textures.empty()) worldRenderer->ReleaseBindings();
    resources.textures.clear();
}
void WorldRenderBridge::SubmitQuad(const void* pdt,HRESULT result)
{
    if(!Active()) return;
    static_assert(sizeof(TPDTVertex)==sizeof(EffectVertex));
    EffectVertex vertices[4]; memcpy(vertices,pdt,sizeof(vertices));
    Submit(vertices,4,true,result);
}
void WorldRenderBridge::Submit(const Renderer::EffectVertex* vertices,uint32_t count,bool strip,HRESULT result)
{
    if(!Active() || FAILED(result)) return;
    EffectDraw draw; draw.strip=strip; std::string error;
    if(!CaptureNativeMaterial(draw,error) || !EffectDrawValid(draw,count)) {
        Report("unsupported material "+error+" color="+std::to_string(draw.colorOp)+" coords="+std::to_string(draw.textureCoordinates),true); return;
    }
    IDirect3DBaseTexture9* bound=nullptr;
    NativeStateView().GetTexture(0,&bound);
    draw.textured=bound!=nullptr;
    TerrainTexturePtr texture;
    // Gradient sky uses only diffuse; its native, potentially stale image is immaterial.
    const bool gradient=part==WorldPart::Sky && draw.colorOp==3 && draw.colorArg2==0 && draw.alphaOp==1;
    if(bound) {
        auto found=textureNames.find(bound); bound->Release();
        if(!gradient) {
            if(found==textureNames.end()) { Report("unresolved native texture",true); return; }
            auto& stored=owner->textures[found->second];
            if(!stored) { stored=LoadStaticObjectTextureFile(found->second.c_str(),*worldRenderer); Report("texture "+found->second,false); }
            if(!stored) { Report("texture upload failed "+found->second,true); return; }
            texture=stored;
        }
    }
    if(gradient) { draw.textured=false; draw.colorArg1=0; }
    std::vector<EffectVertex> litVertices;
    if(part==WorldPart::Water && !draw.textured) {
        if(!ResolveNativeWaterDiffuse(vertices,count,litVertices)) { Report("unsupported native water diffuse",true); return; }
        if(!litVertices.empty()) vertices=litVertices.data();
    }
    worldRenderer->Draw(vertices,count,texture,draw,part);
    if(part==WorldPart::Water) waterTexturesResident=uint32_t(owner->textures.size());
    Report("submitted color="+std::to_string(draw.colorOp)+" alpha="+std::to_string(draw.alphaOp)+
        " src="+std::to_string(draw.src)+" dst="+std::to_string(draw.dst)+" zwrite="+std::to_string(draw.depthWrite)+
        " fog="+std::to_string(draw.fog)+" textured="+std::to_string(draw.textured),false);
}
