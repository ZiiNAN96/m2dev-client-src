#include "Renderer/EffectRenderData.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    EffectDraw d;
    if(effectRenderer || effectWorldFrame || !EffectDrawValid(d,4) || EffectDrawValid(d,2)) return 1;
    d.strip=false; if(!EffectDrawValid(d,6) || EffectDrawValid(d,4)) return 2;
    d.colorOp=7; if(EffectDrawValid(d,6)) return 3;
    d.colorOp=8; if(!EffectDrawValid(d,6)) return 4;
    d.alphaReference=256; if(EffectDrawValid(d,6)) return 5;
    d.alphaReference=255; d.cull=3; if(EffectDrawValid(d,6)) return 6;
    d.cull=0; d.textureTransformFlags=3; if(EffectDrawValid(d,6)) return 7;
    EffectResources owner; auto texture=std::make_shared<TerrainTexture>(); std::weak_ptr<TerrainTexture> weak=texture;
    owner.textures["native"]=texture; texture.reset(); owner.textures.clear(); if(!weak.expired()) return 8;
    std::cout<<"Effect geometry/material policy, legacy-default gate and owner lifetime: PASS\n";
}
