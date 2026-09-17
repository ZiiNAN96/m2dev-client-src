#pragma once
#include "Renderer/ModernFrame.h"
#include "Renderer/GraphicsConfig.h"
inline bool gdxTestModern{};
inline void ConfigureModernScene(bool enabled)
{
    gdxTestModern=enabled;
    if(!enabled)return;
    Graphics::GraphicsRuntimeConfig config;config.revision=1;config.style=Graphics::GraphicsStyle::Modern;
    config.shadows=Graphics::ShadowQuality::High;config.ambientOcclusion=Graphics::AmbientOcclusionQuality::SSAO;
    Renderer::ApplyGraphicsRuntimeConfig(config);
}
inline void BeginModernScene()
{
    if(!gdxTestModern)return;
    Graphics::SceneLighting light;light.sunDirection={.5f,.4f,-1};light.sunIntensity=1.5f;
    light.ambient={.3f,.3f,.3f};light.environmentColor={.08f,.08f,.08f};
    Renderer::modernFrame->Begin(light);
}
inline void EndModernScene(){if(gdxTestModern)Renderer::modernFrame->End();}
