#pragma once
#include "EffectRenderData.h"
#include "WaterDiagnostics.h"
#include "Graphics/WaterConfig.h"
#include "Graphics/SceneLighting.h"
#include "Components/interface/ShadowMapManager.hpp"
#include <memory>

namespace Diligent {class PostFXContext;struct IDeviceContext;struct IRenderDevice;}
namespace Renderer
{
struct WaterStats
{
    std::uint64_t frames{},draws{},ssrFrames{},ssrFallbacks{},targetBytes{},resourceCreations{};
    double waterCpuMilliseconds{},ssrCpuMilliseconds{};
};
struct WaterFrameInputs
{
    Diligent::CameraAttribs camera{};
    Diligent::ShadowMapAttribs shadows{};
    Graphics::SceneLighting lighting;
    Diligent::ITextureView *scene{},*depth{},*normal{},*sky{},*shadow{};
    Diligent::ISampler *skySampler{},*shadowSampler{};
    bool shadowsEnabled{};
};
class DiligentWater final
{
public:
    DiligentWater(Diligent::IRenderDevice*,Diligent::IDeviceContext*);
    ~DiligentWater();
    void Prepare(unsigned width,unsigned height,const Graphics::GraphicsRuntimeConfig&);
    void Begin(const WaterFrameInputs&);
    void Draw(const EffectVertex*,unsigned,const EffectDraw&);
    void Finish(Diligent::PostFXContext*,Diligent::IBuffer* camera,Diligent::ITextureView* motion);
    void ResetFrame();
    bool NeedsPrewarm() const;
    void ReleaseWindowResources();
    WaterStats Stats() const;
private:
    struct Impl;std::unique_ptr<Impl> impl_;
};
}
