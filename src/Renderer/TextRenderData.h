#pragma once
#include "UIRenderData.h"

namespace Renderer
{
// ZiiNAN: Diligent text rendering integration
class ITextRenderer : public IUIRenderer {};
inline ITextRenderer* textRenderer=nullptr;
inline bool TextActive() { return textRenderer && uiFrame && uiMode && !uiSuppressionDepth; }
}
