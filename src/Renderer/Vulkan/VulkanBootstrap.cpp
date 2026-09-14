#include "VulkanBootstrap.h"

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngineVulkan/interface/EngineFactoryVk.h"
#include <android/log.h>
#include <atomic>

namespace Renderer
{
namespace
{
constexpr const char* LogTag = "M2Bootstrap";
std::atomic<std::uint64_t> ErrorCount{0};

void DILIGENT_CALL_TYPE DiligentLog(Diligent::DEBUG_MESSAGE_SEVERITY severity,
                                   const char* message, const char*, const char*, int)
{
    if (severity >= Diligent::DEBUG_MESSAGE_SEVERITY_ERROR)
        ++ErrorCount;
    if (severity >= Diligent::DEBUG_MESSAGE_SEVERITY_WARNING)
        __android_log_print(severity >= Diligent::DEBUG_MESSAGE_SEVERITY_ERROR ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN,
                            LogTag, "Diligent: %s", message ? message : "(no message)");
}
}

struct VulkanBootstrap::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapchain;
    bool firstPresent = false;
};

VulkanBootstrap::VulkanBootstrap() : m_impl{std::make_unique<Impl>()} {}
VulkanBootstrap::~VulkanBootstrap() { Shutdown(); }

bool VulkanBootstrap::Initialize(Platform::NativeWindowHandle window, std::uint32_t width, std::uint32_t height)
{
    Shutdown();
    if (!window || width == 0 || height == 0)
        return false;

    try
    {
        // ZiiNAN: Cross-platform bootstrap
        auto* factory = Diligent::GetEngineFactoryVk();
        if (!factory)
            return false;
        factory->SetMessageCallback(DiligentLog);
        const auto errors = ErrorCount.load();
        Diligent::EngineVkCreateInfo engineInfo{};
#if M2_ANDROID_VULKAN_VALIDATION
        engineInfo.SetValidationLevel(Diligent::VALIDATION_LEVEL_1);
#else
        engineInfo.SetValidationLevel(Diligent::VALIDATION_LEVEL_DISABLED);
#endif
        factory->CreateDeviceAndContextsVk(engineInfo, &m_impl->device, &m_impl->context);
        if (!m_impl->device || !m_impl->context || ErrorCount.load() != errors)
        {
            Shutdown();
            return false;
        }
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Vulkan device/context initialized; validation requested=%d",
                            static_cast<int>(engineInfo.EnableValidation));

        Diligent::SwapChainDesc desc{};
        desc.Width = width;
        desc.Height = height;
        desc.ColorBufferFormat = Diligent::TEX_FORMAT_RGBA8_UNORM;
        desc.DepthBufferFormat = Diligent::TEX_FORMAT_UNKNOWN;
        Diligent::NativeWindow nativeWindow{};
        nativeWindow.pAWindow = window.value;
        factory->CreateSwapChainVk(m_impl->device, m_impl->context, desc, nativeWindow, &m_impl->swapchain);
        if (!m_impl->swapchain || ErrorCount.load() != errors)
        {
            Shutdown();
            return false;
        }
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Vulkan swapchain created (%u x %u)", width, height);
        return true;
    }
    catch (...)
    {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "Vulkan initialization threw an exception");
        Shutdown();
        return false;
    }
}

bool VulkanBootstrap::Resize(std::uint32_t width, std::uint32_t height)
{
    if (!m_impl->swapchain || width == 0 || height == 0)
        return false;
    try
    {
        const auto errors = ErrorCount.load();
        m_impl->swapchain->Resize(width, height);
        return ErrorCount.load() == errors;
    }
    catch (...)
    {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "Vulkan swapchain resize failed");
        return false;
    }
}

bool VulkanBootstrap::ClearAndPresent()
{
    if (!m_impl->swapchain || !m_impl->context)
        return false;
    try
    {
        const auto errors = ErrorCount.load();
        auto* renderTarget = m_impl->swapchain->GetCurrentBackBufferRTV();
        if (!renderTarget)
            return false;
        const float color[]{0.035f, 0.10f, 0.18f, 1.0f};
        m_impl->context->SetRenderTargets(1, &renderTarget, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_impl->context->ClearRenderTarget(renderTarget, color, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_impl->swapchain->Present(1);
        if (ErrorCount.load() != errors)
            return false;
        if (!m_impl->firstPresent)
        {
            __android_log_print(ANDROID_LOG_INFO, LogTag, "Vulkan first present returned without a reported error");
            m_impl->firstPresent = true;
        }
        return true;
    }
    catch (...)
    {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "Vulkan clear/present failed");
        return false;
    }
}

void VulkanBootstrap::Shutdown() noexcept
{
    const bool hadDevice = m_impl->device != nullptr;
    try
    {
        if (m_impl->context)
        {
            m_impl->context->SetRenderTargets(0, nullptr, nullptr, Diligent::RESOURCE_STATE_TRANSITION_MODE_NONE);
            m_impl->context->WaitForIdle();
        }
    }
    catch (...)
    {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "Vulkan idle failed during shutdown");
    }
    m_impl->swapchain.Release();
    m_impl->context.Release();
    m_impl->device.Release();
    m_impl->firstPresent = false;
    if (hadDevice)
        __android_log_print(ANDROID_LOG_INFO, LogTag, "Vulkan shutdown complete");
}
}
