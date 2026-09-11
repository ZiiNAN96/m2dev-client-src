#include "DiligentD3D11BackendInternal.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"

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
    auto* target = m_impl->swapChain->GetCurrentBackBufferRTV();
    auto* depth = m_impl->swapChain->GetDepthBufferDSV();
    if (!target || !depth)
        return false;
    m_impl->context->SetRenderTargets(1, &target, depth, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_impl->inFrame = true;
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
    m_impl->context->ClearDepthStencil(m_impl->swapChain->GetDepthBufferDSV(),
        Diligent::CLEAR_DEPTH_FLAG, 1.0f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void DiligentD3D11Backend::EndFrame()
{
    if (m_impl)
        m_impl->inFrame = false;
}

void DiligentD3D11Backend::Present()
{
    if (m_impl && !m_impl->suspended && !m_impl->inFrame)
        m_impl->swapChain->Present(1);
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
    m_impl->swapChain.Release();
    m_impl->context.Release();
    m_impl->device.Release();
    m_impl.reset();
}
}
