// ZiiNAN: Diligent effect rendering integration; read-only native draw snapshot, no deferred simulation.
#include "StdAfx.h"
#include "EterLib/NativeStateView.h"
#include "EffectRenderBridge.h"
#include "EterLib/NativeMaterialSnapshot.h"
#include "EterLib/StateManager.h"
#include "EterLib/GrpImage.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include <fstream>
#include <set>

namespace
{
using namespace Renderer;
EffectResources* owner=nullptr;
const char* asset=nullptr;
EffectPart part=EffectPart::Particle;
uint64_t serial=~uint64_t(0);
std::unordered_map<const void*,std::string> textureNames;
std::unordered_map<std::string,std::weak_ptr<TerrainTexture>> textures;
std::set<std::string> reported;
std::ofstream diagnostics;
bool Active() { return owner && effectWorldFrame && effectRenderer; }
void Frame()
{
    if(serial==effectFrameSerial) return;
    serial=effectFrameSerial; textureNames.clear();
    for(auto it=textures.begin();it!=textures.end();) if(it->second.expired()) it=textures.erase(it); else ++it;
}
void Report(const std::string& reason,bool error)
{
    if(error && effectRenderer) effectRenderer->ReportFailure();
    const std::string key=std::string(asset ? asset : "trail")+" "+reason;
    if(reported.size()>=256 || !reported.insert(key).second) return;
    if(!diagnostics.is_open()) diagnostics.open("effect-renderer.log",std::ios::trunc);
    diagnostics << (error ? "ERROR " : "") << key << std::endl;
}
bool Snapshot(EffectDraw& d)
{
    std::string error;
    const bool valid=CaptureNativeMaterial(d,error);
    if(!valid && !error.empty()) Report(error,true);
    return valid;
}
}
EffectRenderScope::EffectRenderScope(Renderer::EffectResources& resources,const char* filename,Renderer::EffectPart component)
    :previous(owner),previousAsset(asset),previousPart(part)
{
    owner=nullptr; asset=filename; part=component;
    if(!Renderer::effectRenderer || !Renderer::effectWorldFrame) return;
    Frame();
    // ZiiNAN: Diligent floating text rendering; damage keeps its native effect/texture lifetime.
    owner=&resources;
}
EffectRenderScope::~EffectRenderScope()
{ owner=static_cast<Renderer::EffectResources*>(previous); asset=previousAsset; part=previousPart; }
void EffectRenderBridge::Texture(CGraphicImage* image)
{
    if(!Active() || !image) return;
    Frame();
    if(auto* texture=image->GetTexturePointer()->GetTextureBinding().Identity()) textureNames[texture]=image->GetFileName();
}
void EffectRenderBridge::Part(Renderer::EffectPart p) { part=p; }
void EffectRenderBridge::VisibleParticle() { if(Active()) ++Renderer::effectVisibleParticles; }
HRESULT EffectRenderBridge::DrawPrimitiveUP(D3DPRIMITIVETYPE topology,UINT primitives,const void* vertices,UINT stride)
{
    const HRESULT result=STATEMANAGER.DrawPrimitiveUP(topology,primitives,vertices,stride);
    return SubmitNativeDraw(topology,primitives,vertices,stride,result);
}
HRESULT EffectRenderBridge::SubmitNativeDraw(D3DPRIMITIVETYPE topology,UINT primitives,const void* vertices,UINT stride,HRESULT result)
{
    if(!Active() || !primitives) return result;
    if(FAILED(result)) { Report("native draw rejected HRESULT="+std::to_string(uint32_t(result)),false); return result; }
    if((topology!=D3DPT_TRIANGLELIST && topology!=D3DPT_TRIANGLESTRIP) || (stride!=20 && stride!=24) || !vertices || primitives>UINT32_MAX/3) {
        Report("unsupported geometry",true); return result;
    }
    EffectDraw draw; draw.strip=topology==D3DPT_TRIANGLESTRIP;
    const uint32_t count=draw.strip ? primitives+2 : primitives*3;
    const auto bound=NativeStateView().GetTextureBinding(0);
    draw.textured=bool(bound);
    TerrainTexturePtr texture;
    if(bound) {
        auto found=textureNames.find(bound.Identity());
        if(found==textureNames.end()) { Report("unresolved native texture",true); return result; }
        auto& owned=owner->textures[found->second];
        if(!owned) {
            auto& shared=textures[found->second]; owned=shared.lock();
            if(!owned) { owned=LoadStaticObjectTextureFile(found->second.c_str(),*effectRenderer); shared=owned; }
            if(owned) Report("texture "+found->second,false);
        }
        if(!owned) { Report("texture upload "+found->second,true); return result; }
        texture=owned;
    }
    if(!Snapshot(draw) || !EffectDrawValid(draw,count)) {
        Report("unsupported state src="+std::to_string(draw.src)+" dst="+std::to_string(draw.dst)+
            " color="+std::to_string(draw.colorOp)+" alpha="+std::to_string(draw.alphaOp)+
            " coords="+std::to_string(draw.textureCoordinates)+" transform="+std::to_string(draw.textureTransformFlags)+
            " cull="+std::to_string(draw.cull)+" z="+std::to_string(draw.depthFunction)+" alphafunc="+std::to_string(draw.alphaFunction)+
            " args="+std::to_string(draw.colorArg1)+","+std::to_string(draw.colorArg2)+","+std::to_string(draw.alphaArg1)+","+std::to_string(draw.alphaArg2)+
            " sampler="+std::to_string(draw.sampler.addressU)+","+std::to_string(draw.sampler.addressV)+","+std::to_string(draw.sampler.min)+","+std::to_string(draw.sampler.mag)+","+std::to_string(draw.sampler.mip),true);
        return result;
    }
    std::vector<EffectVertex> upload(count);
    auto* bytes=static_cast<const uint8_t*>(vertices);
    for(uint32_t i=0;i<count;++i) {
        auto& v=upload[i]; memcpy(v.position.data(),bytes+i*stride,12);
        if(stride==24) memcpy(&v.color,bytes+i*stride+12,4);
        memcpy(v.uv.data(),bytes+i*stride+stride-8,8);
    }
    effectRenderer->Draw(upload.data(),count,texture,draw,part);
    Report("submitted part="+std::to_string(uint32_t(part))+" src="+std::to_string(draw.src)+
        " dst="+std::to_string(draw.dst)+" color="+std::to_string(draw.colorOp)+" fog="+std::to_string(draw.fog),false);
    return result;
}
