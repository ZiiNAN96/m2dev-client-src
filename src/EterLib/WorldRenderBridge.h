#pragma once
#include "Renderer/WorldRenderData.h"
class CGraphicImage;

// ZiiNAN: Diligent water rendering integration; native states/draw order remain unchanged.
class WorldRenderScope
{
    Renderer::WorldResources* previous;
    Renderer::WorldPart previousPart;
public:
    WorldRenderScope(Renderer::WorldResources&,Renderer::WorldPart);
    ~WorldRenderScope();
    WorldRenderScope(const WorldRenderScope&)=delete;
    WorldRenderScope& operator=(const WorldRenderScope&)=delete;
};
class WorldRenderBridge
{
public:
    static void Texture(CGraphicImage*);
    static void Submit(const Renderer::EffectVertex*,uint32_t,bool strip);
    static void SubmitQuad(const void* pdtVertices);
    static void Release(Renderer::WorldResources&);
};
