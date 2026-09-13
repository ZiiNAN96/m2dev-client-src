#pragma once
#include "EffectRenderData.h"

namespace Renderer
{
// ZiiNAN: Diligent UI rendering integration; no widget/layout/font API.
class IUIRenderer : public ITextureUploader
{
public:
    virtual void Draw(const EffectVertex*,uint32_t,const TerrainTexturePtr&,const EffectDraw&)=0;
    virtual void ReportFailure()=0;
};
inline IUIRenderer* uiRenderer=nullptr;
inline bool uiFrame=false,uiMode=false;
inline uint32_t uiSuppressionDepth=0;
inline bool UIActive() { return uiRenderer && uiFrame && uiMode && !uiSuppressionDepth; }
struct UIExcludeScope
{
    UIExcludeScope() { ++uiSuppressionDepth; }
    ~UIExcludeScope() { --uiSuppressionDepth; }
    UIExcludeScope(const UIExcludeScope&)=delete;
    UIExcludeScope& operator=(const UIExcludeScope&)=delete;
};
}
