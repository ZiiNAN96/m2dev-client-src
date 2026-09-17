// ZiiNAN: Diligent effect rendering integration; read-only native draw snapshot, no deferred simulation.
#include "StdAfx.h"
#include "EterLib/DrawStateView.h"
#include "EffectRenderBridge.h"
#include "EterLib/MaterialStateSnapshot.h"
#include "EterLib/DrawState.h"
#include "EterLib/GrpImage.h"
#include "Renderer/Diagnostics.h"
#include <fstream>
#include <set>
#include <vector>

namespace
{
using namespace Renderer;
EffectResources* owner=nullptr;
const char* asset=nullptr;
EffectPart part=EffectPart::Particle;
uint64_t serial=~uint64_t(0);
// Native effect images own decoded pixels and uploader-specific GPU textures.
// This lookup is only used synchronously within the current effect frame.
std::unordered_map<const void*,CGraphicImage*> textureImages;
std::set<std::string> reported;
struct EffectDiagnostics {
    std::ofstream stream;
    std::vector<std::string> pending;
    ~EffectDiagnostics() { Flush(); }
    void Flush() {
        if(pending.empty()) return;
        if(!stream.is_open()) stream.open("effect-renderer.log",std::ios::trunc);
        for(const auto& line:pending) stream << line << '\n';
        pending.clear();stream.flush();
    }
    void Record(const std::string& key,bool error) {
        // Report caps unique messages at 256. Successful first-use diagnostics
        // must not stall rendering on file I/O; errors still flush immediately.
        pending.push_back(error ? "ERROR "+key : key);
        if(error) Flush();
    }
} diagnostics;
bool Active() { return owner && effectWorldFrame && effectRenderer; }
void Frame()
{
    if(serial==effectFrameSerial) return;
    serial=effectFrameSerial; textureImages.clear();
}
void Report(const std::string& reason,bool error)
{
    if(error && effectRenderer) effectRenderer->ReportFailure();
    if(!verboseDiagnostics) return;
    const std::string key=std::string(asset ? asset : "trail")+" "+reason;
    if(reported.size()>=256 || !reported.insert(key).second) return;
    diagnostics.Record(key,error);
}
bool Snapshot(EffectDraw& d)
{
    std::string error;
    const bool valid=CaptureMaterialState(d,error);
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
    if(auto* texture=image->GetTexturePointer()->GetTextureBinding().Identity()) textureImages[texture]=image;
}
void EffectRenderBridge::Part(Renderer::EffectPart p) { part=p; }
void EffectRenderBridge::VisibleParticle() { if(Active()) ++Renderer::effectVisibleParticles; }
void EffectRenderBridge::Submit(Renderer::PrimitiveTopology topology,UINT primitives,const void* vertices,UINT stride)
{
    if(!Active() || !primitives) return;
    if((topology!=Renderer::TopologyTriangleList && topology!=Renderer::TopologyTriangleStrip) || (stride!=20 && stride!=24) || !vertices || primitives>UINT32_MAX/3) {
        Report("unsupported geometry",true); return;
    }
    EffectDraw draw; draw.strip=topology==Renderer::TopologyTriangleStrip;
    const uint32_t count=draw.strip ? primitives+2 : primitives*3;
    const auto bound=DrawStateView().GetTextureBinding(0);
    draw.textured=bool(bound);
    TerrainTexturePtr texture;
    if(bound) {
        auto found=textureImages.find(bound.Identity());
        if(found==textureImages.end()) { Report("unresolved native texture",true); return; }
        const std::string filename=found->second->GetFileName();
        auto& owned=owner->textures[filename];
        if(!owned) {
            // Reuse the already decoded native image instead of reopening its pack
            // whenever a short-lived particle instance drops its last weak handle.
            // The image cache is invalidated by device/resource destruction and
            // distinguishes uploader lifetimes, including recreation at one address.
            owned=found->second->GetAssetTexture(*effectRenderer);
            if(owned) Report("texture "+filename,false);
        }
        if(!owned) { Report("texture upload "+filename,true); return; }
        texture=owned;
    }
    if(!Snapshot(draw) || !EffectDrawValid(draw,count)) {
        Report("unsupported state src="+std::to_string(draw.src)+" dst="+std::to_string(draw.dst)+
            " color="+std::to_string(draw.colorOp)+" alpha="+std::to_string(draw.alphaOp)+
            " coords="+std::to_string(draw.textureCoordinates)+" transform="+std::to_string(draw.textureTransformFlags)+
            " cull="+std::to_string(draw.cull)+" z="+std::to_string(draw.depthFunction)+" alphafunc="+std::to_string(draw.alphaFunction)+
            " args="+std::to_string(draw.colorArg1)+","+std::to_string(draw.colorArg2)+","+std::to_string(draw.alphaArg1)+","+std::to_string(draw.alphaArg2)+
            " sampler="+std::to_string(draw.sampler.addressU)+","+std::to_string(draw.sampler.addressV)+","+std::to_string(draw.sampler.min)+","+std::to_string(draw.sampler.mag)+","+std::to_string(draw.sampler.mip),true);
        return;
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
    return;
}
