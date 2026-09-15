#pragma once
#include "ShadowAmbientRuntime.h"
#include "Graphics/GraphicsSettings.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Sampler.h"
#include "Graphics/GraphicsEngine/interface/Query.h"

namespace Renderer {
class DiligentShadowAmbient {
    using Matrix=Graphics::Matrix4;
    Diligent::IRenderDevice* device{};
    Diligent::IDeviceContext* context{};
    Diligent::RefCntAutoPtr<Diligent::ITexture> fallback,shadow,ambient,depthCopy,colorCopy,rawAO;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> depthSRV;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITextureView>,3> cascadeDSV;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> shadowConstants,aoConstants;
    Diligent::RefCntAutoPtr<Diligent::ISampler> comparisonSampler;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> aoPipeline,compositePipeline;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> aoBinding,compositeBinding;
    Graphics::ShadowCascades cascades;
    Graphics::ShadowQualityConfig shadowConfig;
    Graphics::AmbientDepthConfig aoConfig;
    Matrix cameraView{Graphics::Identity4},cameraProjection{Graphics::Identity4};
    bool CreateAO(unsigned,unsigned);
    struct Timing {Diligent::RefCntAutoPtr<Diligent::IQuery> query;std::uint64_t serial{};};
    std::array<Timing,16> timings;
    unsigned timingIndex{};
    void CollectTimings();
    void ReleaseAO();
    void ReleaseShadows();
    void UploadShadows();
public:
    bool ambientActive{};
    ~DiligentShadowAmbient(){Shutdown();}
    bool Initialize(Diligent::IRenderDevice*,Diligent::IDeviceContext*);
    unsigned Begin(const Matrix&,const Matrix&,const Graphics::GraphicsRuntimeConfig&,unsigned width,unsigned height);
    bool BeginCascade(unsigned);
    void EndShadows(Diligent::ISwapChain*);
    void BindTargets(Diligent::ISwapChain*,bool modern);
    void Composite(Diligent::ISwapChain*,Diligent::ITextureView* sceneDepth);
    void Resize(){ReleaseAO();}
    void Reset();
    void Shutdown();
    void BindReceiver(Diligent::IPipelineState*);
    void SetReceiver(Diligent::IShaderResourceBinding*,bool release=false);
    Diligent::IBuffer* Constants()const{return shadowConstants;}
    const Graphics::ShadowQualityConfig& Config()const{return shadowConfig;}
};
}
