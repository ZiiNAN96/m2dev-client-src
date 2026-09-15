#pragma once

// Implementation-only Diligent types; also used by the native GPU readback test.
#include "DiligentD3D11Backend.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Query.h"
#include "SkinningBenchmark.h"
#include "DiligentModernRenderer.h"

struct Renderer::DiligentD3D11Backend::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapChain;
    std::unique_ptr<Renderer::DiligentModernRenderer> modern;
    Diligent::ITextureView* DepthDSV() const {
        if(modern)if(auto* depth=modern->DepthView())return depth;
        return swapChain->GetDepthBufferDSV();
    }
    void BindTargets() {
        if(modern&&(modern->Active()||modern->HDRWorldActive())){modern->BindTargets();return;}
        auto* target=swapChain->GetCurrentBackBufferRTV();
        context->SetRenderTargets(1,&target,DepthDSV(),Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    Renderer::ClearColor color{0.08f, 0.16f, 0.28f, 1.0f};
    bool suspended = false;
    bool inFrame = false;
    struct TimingSlot { Diligent::RefCntAutoPtr<Diligent::IQuery> query; uint64_t frame=0; };
    std::array<TimingSlot,64> benchmarkQueries;
    TimingSlot* activeTiming=nullptr;
    void CollectTimings() {
        for(auto& slot:benchmarkQueries) if(slot.frame) {
            Diligent::QueryDataDuration data;
            if(slot.query->GetData(&data,sizeof(data),true)) {
                if(data.Frequency && Renderer::skinningBenchmarkGpuTimes.size()<Renderer::skinningBenchmarkLimit)
                    Renderer::skinningBenchmarkGpuTimes[slot.frame]=double(data.Duration)*1e6/double(data.Frequency);
                else ++Renderer::skinningBenchmarkDropped;
                slot.frame=0;
            }
        }
    }
};
