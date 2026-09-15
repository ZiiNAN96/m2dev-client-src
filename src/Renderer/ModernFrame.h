#pragma once
#include "Graphics/SceneLighting.h"
#include <cstdint>
namespace Renderer
{
inline unsigned liveModernRenderers{};
// Scoped game traversal: visibility is tested against light cascades, without
// changing the camera's visibility state or submitting color/effect draws.
inline bool shadowCasterCollection{};
struct ModernFrameStats
{
    std::uint64_t frames{},meshDraws{},terrainDraws{},shadowDraws{},lightUploads{},lightBufferCreations{},psoCount{},targetBytes{};
    double shadowSubmitMilliseconds{},aoSubmitMilliseconds{};
    unsigned meshShaderVariants{},terrainShaderVariants{};
};
class IModernFrame
{
public:
    virtual ~IModernFrame()=default;
    virtual void Begin(const Graphics::SceneLighting&)=0;
    virtual void End()=0;
    virtual bool BeginShadowCollection()=0;
    virtual void EndShadowCollection()=0;
    virtual bool ShadowCasterVisible(const std::array<float,3>& center,float radius) const=0;
    virtual void BeginForwardWorld()=0;
    virtual void EndForwardWorld()=0;
    virtual ModernFrameStats Stats() const=0;
};
inline IModernFrame* modernFrame=nullptr;
}
