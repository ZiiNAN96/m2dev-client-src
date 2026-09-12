#pragma once
#include "Renderer/EffectRenderData.h"
class CGraphicImage;

// ZiiNAN: Native simulation and legacy draws remain at their original call sites.
class EffectRenderScope
{
public:
    EffectRenderScope(Renderer::EffectResources&,const char* asset,Renderer::EffectPart part=Renderer::EffectPart::Particle);
    ~EffectRenderScope();
    EffectRenderScope(const EffectRenderScope&)=delete;
    EffectRenderScope& operator=(const EffectRenderScope&)=delete;
private:
    void* previous;
    const char* previousAsset;
    Renderer::EffectPart previousPart;
};
class EffectRenderBridge
{
public:
    static void Texture(CGraphicImage*);
    static void Part(Renderer::EffectPart);
    static void VisibleParticle();
    static HRESULT DrawPrimitiveUP(D3DPRIMITIVETYPE,UINT,const void*,UINT);
    static HRESULT SubmitNativeDraw(D3DPRIMITIVETYPE,UINT,const void*,UINT,HRESULT);
};
