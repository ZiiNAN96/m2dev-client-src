// ZiiNAN: UI defaults, nested exclusion and complete draw-state validation also run with Diligent OFF.
#include "Renderer/UIRenderData.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    if(uiRenderer || uiFrame || uiMode || UIActive() || uiSuppressionDepth) return 1;
    { UIExcludeScope first; { UIExcludeScope second; if(uiSuppressionDepth!=2 || UIActive()) return 2; }
      if(uiSuppressionDepth!=1) return 3; }
    if(uiSuppressionDepth) return 4;
    EffectDraw draw; draw.ui=true; draw.depthTest=draw.depthWrite=false;
    if(EffectDrawValid(draw,4)) return 5;
    draw.viewport={0,0,800,600};
    if(!EffectDrawValid(draw,4)) return 6;
    draw.lines=true; if(!EffectDrawValid(draw,2) || EffectDrawValid(draw,3)) return 7;
    draw.depthTest=true; if(EffectDrawValid(draw,2)) return 8; draw.depthTest=false;
    draw.fog=1; if(EffectDrawValid(draw,2)) return 9; draw.fog=0;
    draw.ui=false; if(EffectDrawValid(draw,2)) return 10;
    draw.lines=false; draw.scissor=true; if(EffectDrawValid(draw,4)) return 11;
    std::cout<<"UI default gate, nested exclusions, viewport/depth/fog/topology contract: PASS\n";
}
