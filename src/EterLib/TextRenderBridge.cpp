// ZiiNAN: Diligent text rendering integration
#include "StdAfx.h"
#include "EterLib/NativeStateView.h"
#include "TextRenderBridge.h"
#include "GrpFontTexture.h"
#include "NativeMaterialSnapshot.h"
#include "StateManager.h"
#include <fstream>
#include <set>

namespace TextRenderBridge
{
namespace
{
void Failure(const std::string& message)
{
    Renderer::textRenderer->ReportFailure();
    static std::set<std::string> reported;
    static std::ofstream log("text-renderer.log",std::ios::trunc);
    if(reported.size()<32 && reported.insert(message).second) log<<message<<std::endl;
}
}
void Submit(const void* pdt,uint32_t count,CGraphicFontTexture* font,HRESULT result,bool indexedQuad)
{
    using namespace Renderer;
    if(!TextActive() || FAILED(result) || !count) return;
    if(!pdt || count>UINT32_MAX/sizeof(EffectVertex)) { Failure("invalid text vertices"); return; }
    EffectDraw draw; std::string error;
    if(!CaptureNativeMaterial(draw,error)) { Failure("text material: "+error); return; }
    D3DVIEWPORT9 viewport{}; RECT clip{}; DWORD scissor=0;
    if(FAILED(NativeStateView().GetViewport(&viewport)) || FAILED(NativeStateView().GetScissorRect(&clip)) ||
       FAILED(NativeStateView().GetRenderState(D3DRS_SCISSORTESTENABLE,&scissor))) { Failure("text viewport/scissor"); return; }
    draw.ui=true; draw.strip=indexedQuad; draw.fog=0;
    draw.floatingText=floatingTextDepth!=0; // ZiiNAN: Diligent floating text rendering
    if(!draw.floatingText) draw.depthTest=draw.depthWrite=false;
    draw.viewport={viewport.X,viewport.Y,viewport.Width,viewport.Height};
    draw.scissor=scissor!=0; draw.clip={clip.left,clip.top,clip.right,clip.bottom};
    if(draw.scissor && (clip.right<=clip.left || clip.bottom<=clip.top)) return;
    const auto bound=NativeStateView().GetTextureBinding(0);
    draw.textured=bool(bound);
    TerrainTexturePtr texture;
    if(bound && font) texture=font->GetTextTexture(bound);
    if(draw.textured && !texture) { Failure("text atlas owner/page missing"); return; }
    static_assert(sizeof(SVertex)==sizeof(EffectVertex) && sizeof(TPDTVertex)==sizeof(EffectVertex));
    std::vector<EffectVertex> vertices(count); memcpy(vertices.data(),pdt,size_t(count)*sizeof(EffectVertex));
    if(indexedQuad) {
        if(count!=4) { Failure("invalid text rectangle"); return; }
        std::swap(vertices[1],vertices[2]);
    }
    if(!draw.textured) for(auto& vertex:vertices) vertex.uv={0,0};
    if(!EffectDrawValid(draw,count)) { Failure("unsupported text draw state"); return; }
    textRenderer->Draw(vertices.data(),count,texture,draw);
}
}
