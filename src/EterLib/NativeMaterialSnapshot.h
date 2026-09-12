#pragma once
#include "Renderer/EffectRenderData.h"
#include <string>
bool CaptureNativeMaterial(Renderer::EffectDraw&,std::string& error);
bool ResolveNativeWaterDiffuse(const Renderer::EffectVertex*,uint32_t,std::vector<Renderer::EffectVertex>&);
