#pragma once
#include "Renderer/TreeRenderData.h"
class CSpeedTreeWrapper;
class CGraphicImage;

// ZiiNAN: Only this bridge translates native SpeedTree data/state into renderer contracts.
class TreeRenderBridge
{
public:
    static void Capture(CSpeedTreeWrapper&,const char* asset);
    static void Draw(CSpeedTreeWrapper const&,Renderer::TreePart,uint32_t lod,uint32_t first,uint32_t count);
    static void Billboard(CSpeedTreeWrapper const&,const float* coords,const float* uv);
};
class TreeCameraMaskScope
{
public:
    explicit TreeCameraMaskScope(CGraphicImage*);
    ~TreeCameraMaskScope();
    TreeCameraMaskScope(const TreeCameraMaskScope&)=delete;
    TreeCameraMaskScope& operator=(const TreeCameraMaskScope&)=delete;
private:
    CGraphicImage* previous;
};
