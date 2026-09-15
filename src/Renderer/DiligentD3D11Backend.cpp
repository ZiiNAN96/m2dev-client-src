#include "DiligentD3D11BackendInternal.h"
#include "TerrainPresentation.h"
#include "FirstUseAudit.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"

namespace Renderer
{
DiligentD3D11Backend::DiligentD3D11Backend() = default;
DiligentD3D11Backend::~DiligentD3D11Backend() { Shutdown(); }

bool DiligentD3D11Backend::Initialize(const InitializeInfo& info)
{
    if (m_impl || !info.window || !info.width || !info.height || !info.windowed)
        return false;
    try
    {
        auto state = std::make_unique<Impl>();
        auto* factory = Diligent::GetEngineFactoryD3D11();
        if (!factory)
            return false;
        Diligent::EngineD3D11CreateInfo engineInfo;
        // A release bootstrap does not require the optional Windows debug layer.
        factory->CreateDeviceAndContextsD3D11(engineInfo, &state->device, &state->context);
        if (!state->device || !state->context)
            return false;
        Diligent::SwapChainDesc swapInfo;
        swapInfo.Width = info.width;
        swapInfo.Height = info.height;
        swapInfo.ColorBufferFormat = Diligent::TEX_FORMAT_RGBA8_UNORM;
        swapInfo.DepthBufferFormat = Diligent::TEX_FORMAT_D24_UNORM_S8_UINT;
        factory->CreateSwapChainD3D11(state->device, state->context, swapInfo, {},
                                     Diligent::NativeWindow{info.window}, &state->swapChain);
        if (!state->swapChain)
            return false;
        const auto& adapter=state->device->GetAdapterInfo();
        graphicsCapabilities={adapter.Texture.MaxTexture2DDimension,adapter.Memory.LocalMemory};
        m_impl = std::move(state);
        return true;
    }
    catch (...)
    {
        return false;
    }
}

bool DiligentD3D11Backend::BeginFrame()
{
    if (!m_impl || m_impl->suspended || m_impl->inFrame)
        return false;
    const auto& config = GetGraphicsRuntimeConfig();
    if (m_graphicsConfig.revision != config.revision) m_graphicsConfig = config;
    if(config.style==Graphics::GraphicsStyle::Modern){
        if(!m_impl->modern)m_impl->modern=std::make_unique<DiligentModernRenderer>(*this);
        m_impl->modern->ResetFrame();modernFrame=m_impl->modern.get();
    } else {
        if(modernFrame==m_impl->modern.get())modernFrame=nullptr;
        m_impl->modern.reset();
    }
    auto* target = m_impl->swapChain->GetCurrentBackBufferRTV();
    auto* depth = m_impl->DepthDSV();
    if (!target || !depth)
        return false;
    m_impl->context->SetRenderTargets(1, &target, depth, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_impl->inFrame = true;
    // ZiiNAN: GPU skinning production path — asynchronous timestamps only in explicit benchmarks.
    if(skinningBenchmarkEnabled) {
        auto& s=*m_impl;
        auto& slot=s.benchmarkQueries[skinningBenchmarkCurrent.serial%s.benchmarkQueries.size()];
        if(!slot.query) {
            Diligent::QueryDesc desc;desc.Name="B6-X complete world frame";desc.Type=Diligent::QUERY_TYPE_DURATION;
            s.device->CreateQuery(desc,&slot.query);
        }
        if(slot.query && !slot.frame) { slot.frame=skinningBenchmarkCurrent.serial;s.activeTiming=&slot;s.context->BeginQuery(slot.query); }
        else ++skinningBenchmarkDropped;
    }
    return true;
}

void DiligentD3D11Backend::Clear(const ClearInfo& info)
{
    if (!m_impl || !m_impl->inFrame)
        return;
    if (info.color)
        m_impl->color = *info.color;
    if (info.colorAndDepth)
        m_impl->context->ClearRenderTarget(m_impl->swapChain->GetCurrentBackBufferRTV(),
                                          m_impl->color.data(), Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_impl->context->ClearDepthStencil(m_impl->DepthDSV(),
        Diligent::CLEAR_DEPTH_FLAG, info.depthValue, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void DiligentD3D11Backend::EndFrame()
{
    if(m_impl && m_impl->activeTiming) {
        m_impl->context->EndQuery(m_impl->activeTiming->query);m_impl->activeTiming=nullptr;
    }
    if (m_impl)
        m_impl->inFrame = false;
}

// ZiiNAN: Read only a completed draw surface before Present, never a discarded swap buffer.
bool DiligentD3D11Backend::CaptureRGB(std::vector<uint8_t>& pixels,uint32_t& width,uint32_t& height)
{
    using namespace Diligent;
    pixels.clear(); width=height=0;
    if(!m_impl || !m_impl->inFrame || m_impl->suspended) return false;
    try {
        auto& s=*m_impl; auto* source=s.swapChain->GetCurrentBackBufferRTV()->GetTexture();
        auto desc=source->GetDesc();
        if(desc.Format!=TEX_FORMAT_RGBA8_UNORM || !desc.Width || !desc.Height) return false;
        desc.Name="M11 screenshot staging"; desc.Usage=USAGE_STAGING; desc.BindFlags=BIND_NONE;
        desc.CPUAccessFlags=CPU_ACCESS_READ; desc.MiscFlags=MISC_TEXTURE_FLAG_NONE;
        RefCntAutoPtr<ITexture> staging; s.device->CreateTexture(desc,nullptr,&staging);
        if(!staging) return false;
        CopyTextureAttribs copy; copy.pSrcTexture=source; copy.pDstTexture=staging;
        copy.SrcTextureTransitionMode=copy.DstTextureTransitionMode=RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        s.context->CopyTexture(copy); s.context->WaitForIdle();
        pixels.resize(size_t(desc.Width)*desc.Height*3);
        MappedTextureSubresource mapped;
        s.context->MapTextureSubresource(staging,0,0,MAP_READ,MAP_FLAG_NONE,nullptr,mapped);
        if(!mapped.pData) { pixels.clear(); return false; }
        for(uint32_t y=0;y<desc.Height;++y) {
            const auto* src=static_cast<const uint8_t*>(mapped.pData)+y*mapped.Stride;
            auto* dst=pixels.data()+size_t(y)*desc.Width*3;
            for(uint32_t x=0;x<desc.Width;++x) { memcpy(dst+x*3,src+x*4,3); }
        }
        s.context->UnmapTextureSubresource(staging,0,0);
        width=desc.Width; height=desc.Height; return true;
    } catch(...) { pixels.clear(); return false; }
}

void DiligentD3D11Backend::Present()
{
    if (m_impl && !m_impl->suspended && !m_impl->inFrame)
    {
        const auto start=skinningBenchmarkEnabled ? PrototypeClock::now() : PrototypeClock::time_point{};
        AssetRuntime::AnimationStallAudit::WorkScope stallPresentWait(AssetRuntime::AnimationStallAudit::Work::PresentWait);
        m_impl->swapChain->Present(1);
        if(awaitingWorldPresent){awaitingWorldPresent=false;LogClientLifecycle("WorldPresented");}
        stallPresentWait.Stop();
        if (AssetRuntime::AnimationStallAudit::enabled) ++AssetRuntime::AnimationStallAudit::swapchainPresents;
        if(skinningBenchmarkEnabled) {
            skinningBenchmarkCurrent.presentUs+=PrototypeMicroseconds(start);
            m_impl->CollectTimings();
        }
    }
}

bool DiligentD3D11Backend::Resize(uint32_t width, uint32_t height)
{
    if (!m_impl || m_impl->inFrame)
        return false;
    m_impl->suspended = width == 0 || height == 0;
    if (m_impl->suspended)
        return true;
    try
    {
        // Release context bindings before DXGI replaces the back buffers.
        m_impl->context->SetRenderTargets(0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);
        if(m_impl->modern)m_impl->modern->ReleaseWindowResources();
        m_impl->swapChain->Resize(width, height);
        const auto& desc = m_impl->swapChain->GetDesc();
        return desc.Width == width && desc.Height == height;
    }
    catch (...)
    {
        return false;
    }
}

void DiligentD3D11Backend::Shutdown()
{
    if (!m_impl)
        return;
    m_impl->inFrame = false;
    m_impl->context->SetRenderTargets(0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);
    m_impl->context->Flush();
    m_impl->context->WaitForIdle();
    if(modernFrame==m_impl->modern.get())modernFrame=nullptr;
    m_impl->modern.reset();
    if(skinningBenchmarkEnabled) m_impl->CollectTimings();
    for(auto& slot:m_impl->benchmarkQueries) { slot.query.Release();slot.frame=0; }
    m_impl->swapChain.Release();
    m_impl->context.Release();
    m_impl->device.Release();
    m_impl.reset();
}
}
