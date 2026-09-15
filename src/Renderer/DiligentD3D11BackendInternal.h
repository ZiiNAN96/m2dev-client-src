#pragma once

// Implementation-only Diligent types; also used by the native GPU readback test.
#include "DiligentD3D11Backend.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Graphics/GraphicsEngine/interface/Query.h"
#include "SkinningBenchmark.h"
#include "SceneLightingRuntime.h"
#include "DiligentShadowAmbient.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

struct Renderer::DiligentD3D11Backend::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapChain;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> lightBuffer;
    Renderer::DiligentShadowAmbient depthEffects;
    std::uint64_t lightRevision{};
    ~Impl() { if(lightBuffer) { --Renderer::liveLightBuffers; --Renderer::liveSceneLightingResources; } }
    bool SyncSceneLighting() {
        using namespace Diligent;
        if(!lightBuffer) {
            BufferDesc desc;desc.Name="ZiiNAN shared scene lighting";desc.Size=64;
            desc.Usage=USAGE_DYNAMIC;desc.BindFlags=BIND_UNIFORM_BUFFER;desc.CPUAccessFlags=CPU_ACCESS_WRITE;
            device->CreateBuffer(desc,nullptr,&lightBuffer);
            if(!lightBuffer)return false;
            ++Renderer::liveLightBuffers;++Renderer::liveSceneLightingResources;
        }
        if(lightRevision==Renderer::sceneLighting.Revision())return true;
        const auto& light=Renderer::sceneLighting.Get();
        MapHelper<std::array<float,16>> mapped(context,lightBuffer,MAP_WRITE,MAP_FLAG_DISCARD);
        if(!mapped)return false;
        for(unsigned c=0;c<3;++c){
            (*mapped)[c]=light.sun.direction[c];
            (*mapped)[4+c]=light.sun.color[c]*light.sun.intensity;
            (*mapped)[8+c]=light.ambientSkyColor[c]*light.ambientIntensity;
            (*mapped)[12+c]=light.ambientGroundColor[c]*light.ambientIntensity;
        }
        (*mapped)[3]=(*mapped)[7]=(*mapped)[11]=(*mapped)[15]=0;
        lightRevision=Renderer::sceneLighting.Revision();++Renderer::lightBufferUpdates;
        return true;
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
