// ZiiNAN: Diligent text rendering integration; runs in ON and OFF builds.
#include "Renderer/TextRenderData.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    struct Probe final : ITextRenderer {
        TerrainTexturePtr UploadTexture(const TerrainTextureData&) override { return {}; }
        void Draw(const EffectVertex*,uint32_t,const TerrainTexturePtr&,const EffectDraw&) override {}
        void ReportFailure() override {}
    } probe;
    if(textRenderer || TextActive()) return 1;
    textRenderer=&probe;
    if(TextActive()) return 2;
    uiFrame=uiMode=true;
    if(!TextActive()) return 3;
    { UIExcludeScope eventText; if(TextActive()) return 4;
      { UIExcludeScope nested; if(TextActive()) return 5; } }
    if(!TextActive()) return 6;
    uiMode=false; if(TextActive()) return 7;
    uiMode=true; uiFrame=false; if(TextActive()) return 8;
    textRenderer=nullptr; uiMode=false;
    EffectDraw draw; draw.ui=true; draw.depthTest=draw.depthWrite=false; draw.viewport={0,0,800,600};
    if(draw.colorWriteMask!=15 || !EffectDrawValid(draw,4)) return 9;
    for(uint32_t mask=0;mask<16;++mask) { draw.colorWriteMask=mask; if(!EffectDrawValid(draw,4)) return 10; }
    draw.colorWriteMask=16; if(EffectDrawValid(draw,4)) return 11;
    draw.colorWriteMask=15; draw.depthTest=draw.depthWrite=true;
    if(EffectDrawValid(draw,4)) return 12;
    draw.floatingText=true; if(!EffectDrawValid(draw,4)) return 13;
    draw.ui=false; if(EffectDrawValid(draw,4)) return 14;
    { FloatingTextScope outer; if(floatingTextDepth!=1) return 15;
      { FloatingTextScope inner; if(floatingTextDepth!=2) return 16; }
      if(floatingTextDepth!=1) return 17; }
    if(floatingTextDepth) return 18;
    std::cout<<"Text startup/frame/UI gate, nested special-text exclusion, complete color mask: PASS\n";
}
