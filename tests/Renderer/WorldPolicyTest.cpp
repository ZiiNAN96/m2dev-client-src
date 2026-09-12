#include "Renderer/WorldRenderData.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    if(worldRenderer || worldSurfaceFrame || waterGeometryCount.load()) return 1;
    {
        auto geometry=std::make_shared<WaterGeometry>();
        if(waterGeometryCount!=1) return 2;
        std::weak_ptr<WaterGeometry> weak=geometry; geometry.reset();
        if(!weak.expired() || waterGeometryCount) return 3;
    }
    EffectDraw cloud; cloud.colorOp=20;
    if(!EffectDrawValid(cloud,4)) return 4;
    cloud.alphaOp=20; if(EffectDrawValid(cloud,4)) return 5;
    WorldResources owner; auto texture=std::make_shared<TerrainTexture>();
    std::weak_ptr<TerrainTexture> weak=texture;
    owner.textures["water-frame"]=texture; texture.reset(); owner.textures.clear();
    if(!weak.expired()) return 6;
    std::cout<<"World legacy gate, cloud-only combiner, patch and texture lifetime: PASS\n";
}
