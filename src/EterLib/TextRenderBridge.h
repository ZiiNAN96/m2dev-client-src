#pragma once
#include "Renderer/TextRenderData.h"

class CGraphicFontTexture;
namespace TextRenderBridge
{
void Submit(const void* pdt,uint32_t count,CGraphicFontTexture* font,bool indexedQuad=false);
}
