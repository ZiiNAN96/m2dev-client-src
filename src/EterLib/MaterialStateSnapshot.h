#pragma once
#include "Renderer/EffectRenderData.h"
#include <string>
bool CaptureMaterialState(Renderer::EffectDraw&,std::string& error,bool allowSecondary=false);
bool ResolveWaterDiffuse(const Renderer::EffectVertex*,uint32_t,std::vector<Renderer::EffectVertex>&);
