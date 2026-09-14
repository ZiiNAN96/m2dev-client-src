#include "AndroidFilesystem.h"
#include "AndroidInput.h"
#include "AndroidLifecycle.h"
#include "AndroidWindow.h"
#include "Platform/PlatformTime.h"
#include "Renderer/Vulkan/VulkanBootstrap.h"

#include <android/log.h>
#include <android_native_app_glue.h>

namespace
{
constexpr const char* LogTag = "M2Bootstrap";

class Application final
{
public:
    explicit Application(android_app& app) : m_app{app} {}
    ~Application() { Shutdown(); }

    void Run()
    {
        m_app.userData = this;
        m_app.onAppCmd = [](android_app* app, std::int32_t command)
        {
            auto& self = *static_cast<Application*>(app->userData);
            try { self.Command(command); }
            catch (...) { self.Fail("application command failed"); }
        };
        m_app.onInputEvent = [](android_app* app, AInputEvent* event) -> std::int32_t
        {
            auto& self = *static_cast<Application*>(app->userData);
            try { return self.m_input.Handle(event) ? 1 : 0; }
            catch (...) { self.Fail("input dispatch failed"); return 0; }
        };

        const auto storage = Platform::Android::GetAppStorage(*m_app.activity);
        if (!Platform::Time::Initialize() || !storage.IsValid())
            Fail("platform clock/storage initialization failed");
        else
            __android_log_print(ANDROID_LOG_INFO, LogTag, "platform init; read-only assets and private writable storage available");

        while (!m_app.destroyRequested)
        {
            int events = 0;
            android_poll_source* source = nullptr;
            const int result = ALooper_pollOnce(m_lifecycle.CanRender() && !m_failed ? 0 : -1,
                                               nullptr, &events, reinterpret_cast<void**>(&source));
            if (result >= 0 && source)
                source->process(&m_app, source);
            if (m_app.destroyRequested)
                break;
            if (result == ALOOPER_POLL_ERROR)
            {
                Fail("Android event polling failed");
                break;
            }
            // A wakeup or callback can carry a pause/termination command. Drain it before presenting.
            if (result != ALOOPER_POLL_TIMEOUT)
                continue;
            if (!m_failed && m_lifecycle.CanRender() && !m_renderer.ClearAndPresent())
                Fail("Vulkan clear/present returned failure");
        }
        Shutdown();
        m_app.onInputEvent = nullptr;
        m_app.onAppCmd = nullptr;
        m_app.userData = nullptr;
    }

private:
    void Fail(const char* message) noexcept
    {
        if (!m_failed)
        {
            __android_log_print(ANDROID_LOG_ERROR, LogTag, "%s; finishing activity", message);
            m_failed = true;
            m_lifecycle.SetRendererReady(false);
            ANativeActivity_finish(m_app.activity);
        }
    }

    void ReleaseSurface()
    {
        m_lifecycle.DestroySurface();
        m_input.Cancel();
        m_renderer.Shutdown();
        m_window.Reset();
        m_rendererReady = false;
    }

    void UpdateSurface()
    {
        const auto width = m_window.Width();
        const auto height = m_window.Height();
        m_lifecycle.SetSurfaceSize(width, height);
        if (m_failed || !m_window.GetNativeHandle() || width == 0 || height == 0)
            return;
        if (!m_rendererReady)
            m_rendererReady = m_renderer.Initialize(m_window.GetNativeHandle(), width, height);
        else if (!m_renderer.Resize(width, height))
            m_rendererReady = false;
        m_lifecycle.SetRendererReady(m_rendererReady);
        if (!m_rendererReady)
            Fail("Vulkan surface initialization/resize failed");
    }

    void Command(std::int32_t command)
    {
        switch (command)
        {
        case APP_CMD_INIT_WINDOW:
            ReleaseSurface();
            m_window.Attach(m_app.window);
            __android_log_print(ANDROID_LOG_INFO, LogTag, "surface available");
            UpdateSurface();
            break;
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED:
        case APP_CMD_CONFIG_CHANGED:
            UpdateSurface();
            break;
        case APP_CMD_RESUME:
            m_lifecycle.SetResumed(true);
            __android_log_print(ANDROID_LOG_INFO, LogTag, "application resume");
            break;
        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
            m_lifecycle.SetResumed(false);
            m_input.Cancel();
            __android_log_print(ANDROID_LOG_INFO, LogTag, "application pause");
            break;
        case APP_CMD_GAINED_FOCUS:
            m_lifecycle.SetFocused(true);
            break;
        case APP_CMD_LOST_FOCUS:
            m_lifecycle.SetFocused(false);
            m_input.Cancel();
            break;
        case APP_CMD_TERM_WINDOW:
            ReleaseSurface();
            __android_log_print(ANDROID_LOG_INFO, LogTag, "surface destroyed; renderer released before native window");
            break;
        case APP_CMD_DESTROY:
            Shutdown();
            break;
        default:
            break;
        }
    }

    void Shutdown()
    {
        if (m_shutdown)
            return;
        m_lifecycle.Destroy();
        ReleaseSurface();
        m_shutdown = true;
        __android_log_print(ANDROID_LOG_INFO, LogTag, "application shutdown complete");
    }

    android_app& m_app;
    Platform::Android::Lifecycle m_lifecycle;
    Platform::Android::Window m_window;
    Renderer::VulkanBootstrap m_renderer;
    Platform::Android::Input m_input;
    bool m_rendererReady = false;
    bool m_failed = false;
    bool m_shutdown = false;
};
}

// ZiiNAN: Cross-platform bootstrap
extern "C" void android_main(android_app* app)
{
    if (!app || !app->activity)
        return;
    try
    {
        Application application{*app};
        application.Run();
    }
    catch (...)
    {
        __android_log_print(ANDROID_LOG_ERROR, LogTag, "bootstrap exception; finishing activity");
        ANativeActivity_finish(app->activity);
    }
}
