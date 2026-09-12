// ZiiNAN: Diligent effect rendering integration; read-only native draw snapshot, no deferred simulation.
#include "StdAfx.h"
#include "EffectRenderBridge.h"
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
std::unordered_map<void*,std::string> textureNames;
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
    auto* device=STATEMANAGER.GetDevice(); bool ok=device!=nullptr;
    if(!ok) return false;
    const auto rs=[&](D3DRENDERSTATETYPE t) { DWORD v=0; ok=SUCCEEDED(device->GetRenderState(t,&v)) && ok; return v; };
    const auto ts=[&](D3DTEXTURESTAGESTATETYPE t) { DWORD v=0; ok=SUCCEEDED(device->GetTextureStageState(0,t,&v)) && ok; return v; };
    const auto ss=[&](D3DSAMPLERSTATETYPE t) { DWORD v=0; ok=SUCCEEDED(device->GetSamplerState(0,t,&v)) && ok; return v; };
    const auto asFloat=[](DWORD v) { float f; memcpy(&f,&v,4); return f; };
    d.blend=rs(D3DRS_ALPHABLENDENABLE)!=0; d.src=rs(D3DRS_SRCBLEND); d.dst=rs(D3DRS_DESTBLEND); d.blendOp=rs(D3DRS_BLENDOP);
    d.depthTest=rs(D3DRS_ZENABLE)!=0; d.depthWrite=rs(D3DRS_ZWRITEENABLE)!=0; d.depthFunction=rs(D3DRS_ZFUNC);
    d.cull=rs(D3DRS_CULLMODE)-1; d.alphaTest=rs(D3DRS_ALPHATESTENABLE)!=0; d.alphaFunction=rs(D3DRS_ALPHAFUNC); d.alphaReference=rs(D3DRS_ALPHAREF);
    d.colorOp=ts(D3DTSS_COLOROP); d.colorArg1=ts(D3DTSS_COLORARG1); d.colorArg2=ts(D3DTSS_COLORARG2);
    d.alphaOp=ts(D3DTSS_ALPHAOP); d.alphaArg1=ts(D3DTSS_ALPHAARG1); d.alphaArg2=ts(D3DTSS_ALPHAARG2);
    d.textureCoordinates=ts(D3DTSS_TEXCOORDINDEX); d.textureTransformFlags=ts(D3DTSS_TEXTURETRANSFORMFLAGS);
    D3DXCOLOR factor(rs(D3DRS_TEXTUREFACTOR)); d.factor={factor.r,factor.g,factor.b,factor.a};
    d.sampler.addressU=ss(D3DSAMP_ADDRESSU); d.sampler.addressV=ss(D3DSAMP_ADDRESSV);
    d.sampler.min=ss(D3DSAMP_MINFILTER); d.sampler.mag=ss(D3DSAMP_MAGFILTER); d.sampler.mip=ss(D3DSAMP_MIPFILTER);
    d.sampler.anisotropy=std::clamp(ss(D3DSAMP_MAXANISOTROPY),DWORD(1),DWORD(16));
    d.sampler.maxMip=ss(D3DSAMP_MAXMIPLEVEL); d.sampler.lodBias=asFloat(ss(D3DSAMP_MIPMAPLODBIAS)); d.sampler.border=ss(D3DSAMP_BORDERCOLOR);
    for(auto entry:{std::pair<D3DTRANSFORMSTATETYPE,std::array<float,16>*>(D3DTS_WORLD,&d.matrices.world),
         {D3DTS_VIEW,&d.matrices.view},{D3DTS_PROJECTION,&d.matrices.projection},{D3DTS_TEXTURE0,&d.textureTransform}}) {
        D3DXMATRIX matrix; ok=SUCCEEDED(device->GetTransform(entry.first,&matrix)) && ok; memcpy(entry.second->data(),&matrix,64);
    }
    if(rs(D3DRS_FOGENABLE)) {
        if(rs(D3DRS_FOGTABLEMODE)!=D3DFOG_NONE) { Report("native fog table="+std::to_string(rs(D3DRS_FOGTABLEMODE)),true); return false; }
        d.fog=rs(D3DRS_FOGVERTEXMODE); d.rangeFog=rs(D3DRS_RANGEFOGENABLE)!=0;
        d.fogParameters={asFloat(rs(D3DRS_FOGSTART)),asFloat(rs(D3DRS_FOGEND)),asFloat(rs(D3DRS_FOGDENSITY)),0};
        D3DXCOLOR color(rs(D3DRS_FOGCOLOR)); d.fogColor={color.r,color.g,color.b,color.a};
        if(d.fog==3 && d.fogParameters[0]==d.fogParameters[1]) return false;
    }
    IDirect3DVertexShader9* vs=nullptr; IDirect3DPixelShader9* ps=nullptr;
    device->GetVertexShader(&vs); device->GetPixelShader(&ps);
    const bool shaders=vs || ps; if(vs) vs->Release(); if(ps) ps->Release();
    IDirect3DBaseTexture9* second=nullptr; device->GetTexture(1,&second);
    DWORD secondOp=0; device->GetTextureStageState(1,D3DTSS_COLOROP,&secondOp);
    const bool secondUsed=second && secondOp!=D3DTOP_DISABLE; if(second) second->Release();
    IDirect3DSurface9* target=nullptr;
    if(SUCCEEDED(device->GetRenderTarget(0,&target)) && target) {
        D3DSURFACE_DESC desc{}; target->GetDesc(&desc); target->Release();
        d.opaqueTargetAlpha=desc.Format==D3DFMT_X8R8G8B8 || desc.Format==D3DFMT_R5G6B5;
    }
    const bool valid=ok && !shaders && !secondUsed && !rs(D3DRS_SEPARATEALPHABLENDENABLE) && !rs(D3DRS_STENCILENABLE) &&
        (rs(D3DRS_COLORWRITEENABLE)&15)==15 && rs(D3DRS_FILLMODE)==D3DFILL_SOLID;
    if(!valid) Report("native snapshot shaders="+std::to_string(shaders)+" second="+std::to_string(secondUsed)+
        " separate="+std::to_string(rs(D3DRS_SEPARATEALPHABLENDENABLE))+" stencil="+std::to_string(rs(D3DRS_STENCILENABLE))+
        " colorwrite="+std::to_string(rs(D3DRS_COLORWRITEENABLE))+" fill="+std::to_string(rs(D3DRS_FILLMODE)),true);
    return valid;
}
}
EffectRenderScope::EffectRenderScope(Renderer::EffectResources& resources,const char* filename,Renderer::EffectPart component)
    :previous(owner),previousAsset(asset),previousPart(part)
{
    owner=nullptr; asset=filename; part=component;
    if(!Renderer::effectRenderer || !Renderer::effectWorldFrame) return;
    Frame();
    std::string path=filename ? filename : "";
    std::replace(path.begin(),path.end(),'\\','/');
    std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c) { return char(std::tolower(c)); });
    // Damage numbers share EffectLib, but are explicitly outside milestone 7.
    if(path.find("effect/affect/damagevalue/")==std::string::npos) owner=&resources;
}
EffectRenderScope::~EffectRenderScope()
{ owner=static_cast<Renderer::EffectResources*>(previous); asset=previousAsset; part=previousPart; }
void EffectRenderBridge::Texture(CGraphicImage* image)
{
    if(!Active() || !image) return;
    Frame();
    if(auto* texture=image->GetTexturePointer()->GetD3DTexture()) textureNames[texture]=image->GetFileName();
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
    IDirect3DBaseTexture9* bound=nullptr; STATEMANAGER.GetDevice()->GetTexture(0,&bound);
    draw.textured=bound!=nullptr;
    TerrainTexturePtr texture;
    if(bound) {
        auto found=textureNames.find(bound); bound->Release();
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
