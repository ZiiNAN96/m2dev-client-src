#include "RendererBootstrap.h"
#include "DiligentD3D11Backend.h"
#include "Platform/PlatformWindow.h"
#include "Platform/PlatformTime.h"
#include <fstream>
#include <memory>

namespace Renderer
{
// ZiiNAN: Platform abstraction
int RunRendererBootstrap(void* instance, const StartupOptions& options)
{
    std::ofstream log("renderer-bootstrap.log", std::ios::trunc);
    log << "backend=diligent-d3d11" << std::endl;
    Platform::PlatformWindow window;
    window.SetInstance(instance);
    Platform::WindowCreateInfo create;
    create.title = "Metin2 - renderer clear/present";
    create.resizable = true;
    if (!window.Create(create)) return 3;
    window.SetSize(800, 600);
    auto client = window.GetClientRect();
    auto backend = std::make_unique<DiligentD3D11Backend>();
    int result = 0;
    if (!backend->Initialize({window.GetNativeHandle().value,
        static_cast<uint32_t>(client.right), static_cast<uint32_t>(client.bottom)}))
    {
        log << "ERROR: Initialize failed" << std::endl;
        result = 4;
    }
    else
    {
        log << "Initialize OK" << std::endl;
        window.Show(!options.smokeTest);
        unsigned frames = 0;
        bool running = true, failed = false;
        while (running && !failed)
        {
            Platform::PollResult polled;
            while ((polled = window.PollEvents()) == Platform::PollResult::Dispatched) {}
            if (polled == Platform::PollResult::Quit) break;
            if (window.IsWindowMinimized())
            {
                window.WaitForEvents();
                continue;
            }
            const auto next = window.GetClientRect();
            if (next.right != client.right || next.bottom != client.bottom)
            {
                failed = !backend->Resize(static_cast<uint32_t>(next.right), static_cast<uint32_t>(next.bottom));
                client = next;
            }
            if (failed) break;
            if (!backend->BeginFrame()) { failed = true; break; }
            backend->Clear({true, ClearColor{0.08f, 0.16f, 0.28f, 1.0f}});
            backend->EndFrame();
            backend->Present();
            ++frames;
            if (options.smokeTest && frames == 2)
            {
                window.SetSize(1024, 720);
                const auto resized = window.GetClientRect();
                failed = !backend->Resize(static_cast<uint32_t>(resized.right), static_cast<uint32_t>(resized.bottom));
                client = resized;
                log << "WM_SIZE resize " << (failed ? "FAILED" : "OK") << std::endl;
            }
            if (options.smokeTest && frames == 5) running = false;
            Platform::Time::SleepMilliseconds(1);
        }
        result = failed ? 5 : 0;
        log << "Frames=" << frames << " result=" << result << std::endl;
    }
    backend->Shutdown();
    backend.reset();
    log << "Shutdown OK" << std::endl;
    window.Destroy();
    return result;
}
}
