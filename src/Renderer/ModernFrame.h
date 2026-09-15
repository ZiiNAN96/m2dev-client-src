#pragma once
#include "Graphics/SceneLighting.h"
#include "TerrainRenderData.h"
#include <cstdint>
namespace Renderer
{
struct EffectVertex;struct EffectDraw;
inline unsigned liveModernRenderers{};
// Scoped game traversal: visibility is tested against light cascades, without
// changing the camera's visibility state or submitting color/effect draws.
inline bool shadowCasterCollection{};
struct ModernFrameStats
{
    std::uint64_t frames{},meshDraws{},terrainDraws{},shadowDraws{},lightUploads{},lightBufferCreations{},psoCount{},targetBytes{};
    double shadowSubmitMilliseconds{},aoSubmitMilliseconds{};
    double atmosphereSubmitMilliseconds{},bloomSubmitMilliseconds{},toneMapSubmitMilliseconds{},compositeSubmitMilliseconds{};
    std::uint64_t hdrTargetBytes{},atmosphereTargetBytes{},toneMappedFrames{};
    std::uint64_t legacyMaterialDraws{},pbrMaterialDraws{},authoredShimmerDraws{};
    std::uint64_t waterFrames{},waterDraws{},ssrFrames{},ssrFallbacks{},waterTargetBytes{},waterResourceCreations{};
    double waterSubmitMilliseconds{},ssrSubmitMilliseconds{};
    unsigned meshShaderVariants{},terrainShaderVariants{};
};
class IModernFrame
{
public:
    virtual ~IModernFrame()=default;
    virtual void Begin(const Graphics::SceneLighting&,bool deferToneMapping=false)=0;
    virtual void SetCamera(const TerrainMatrices&)=0;
    virtual void End()=0;
    virtual void FinishWorld()=0;
    virtual void DrawWater(const EffectVertex*,unsigned,const EffectDraw&)=0;
    virtual void FinishWater()=0;
    virtual bool BeginShadowCollection()=0;
    virtual void EndShadowCollection()=0;
    virtual bool ShadowCasterVisible(const std::array<float,3>& center,float radius) const=0;
    virtual void BeginForwardWorld()=0;
    virtual void EndForwardWorld()=0;
    virtual ModernFrameStats Stats() const=0;
};
inline IModernFrame* modernFrame=nullptr;
}
