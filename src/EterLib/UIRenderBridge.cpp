// ZiiNAN: Diligent UI rendering integration; consume native vertices and per-draw states.
#include "StdAfx.h"
#include "UIRenderBridge.h"
#include "NativeMaterialSnapshot.h"
#include "GrpImage.h"
#include "StateManager.h"
#include <fstream>
#include <set>

namespace UIRenderBridge
{
namespace
{
void Failure(const std::string& message)
{
    Renderer::uiRenderer->ReportFailure();
    static std::set<std::string> reported;
    static std::ofstream log("ui-renderer.log",std::ios::trunc);
    if(reported.size()<64 && reported.insert(message).second) log<<message<<std::endl;
}
}
void Submit(const void* pdt,uint32_t count,Primitive primitive,CGraphicImage* image,HRESULT result)
{
    using namespace Renderer;
    if(!UIActive() || FAILED(result) || !count) return;
    if(!pdt || count>UINT32_MAX/sizeof(EffectVertex)) { Failure("invalid UI geometry"); return; }
    EffectDraw draw; std::string error;
    if(!CaptureNativeMaterial(draw,error)) { Failure("UI material: "+error); return; }
    auto* device=STATEMANAGER.GetDevice();
    D3DVIEWPORT9 viewport{}; RECT clip{}; DWORD scissor=0;
    if(FAILED(device->GetViewport(&viewport)) || FAILED(device->GetScissorRect(&clip)) ||
       FAILED(device->GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor))) { Failure("UI viewport/scissor snapshot"); return; }
    draw.ui=true; draw.fog=0;
    draw.floatingText=floatingTextDepth!=0; // ZiiNAN: Ground-label boxes and guild marks share native tail depth.
    if(!draw.floatingText) draw.depthTest=draw.depthWrite=false;
    draw.lines=primitive==Primitive::Lines; draw.strip=primitive==Primitive::Strip || primitive==Primitive::IndexedQuad;
    draw.viewport={viewport.X,viewport.Y,viewport.Width,viewport.Height};
    draw.scissor=scissor!=0; draw.clip={clip.left,clip.top,clip.right,clip.bottom};
    if(draw.scissor && (clip.right<=clip.left || clip.bottom<=clip.top)) return;
    IDirect3DBaseTexture9* bound=nullptr;
    if(FAILED(device->GetTexture(0,&bound))) { Failure("UI texture snapshot"); return; }
    const bool matches=!image || bound==image->GetTexturePointer()->GetD3DTexture();
    draw.textured=bound!=nullptr; if(bound) bound->Release();
    if(!matches || (draw.textured && !image)) { Failure("UI texture owner mismatch"); return; }
    TerrainTexturePtr texture;
    if(draw.textured) {
        texture=image->GetUITexture(*uiRenderer);
        if(!texture) { Failure(std::string("UI texture upload: ")+image->GetFileName()); return; }
    }
    static_assert(sizeof(TPDTVertex)==sizeof(EffectVertex));
    std::vector<EffectVertex> vertices(count); memcpy(vertices.data(),pdt,size_t(count)*sizeof(EffectVertex));
    // Native DEFAULT_IB_FILL_RECT is 0,2,1,2,3,1, unlike the unindexed bar strip.
    if(primitive==Primitive::IndexedQuad) {
        if(count!=4) { Failure("invalid UI indexed quad"); return; }
        std::swap(vertices[1],vertices[2]);
    }
    // Some native untextured helpers leave UV bytes unspecified; these have no rendering meaning.
    if(!draw.textured) for(auto& v:vertices) v.uv={0,0};
    if(primitive==Primitive::Fan) {
        if(count<3) return;
        std::vector<EffectVertex> triangles; triangles.reserve((size_t(count)-2)*3);
        for(uint32_t i=1;i+1<count;++i) { triangles.push_back(vertices[0]); triangles.push_back(vertices[i]); triangles.push_back(vertices[i+1]); }
        vertices.swap(triangles);
    }
    if(!EffectDrawValid(draw,uint32_t(vertices.size()))) { Failure("unsupported UI draw contract"); return; }
    uiRenderer->Draw(vertices.data(),uint32_t(vertices.size()),texture,draw);
}
}
