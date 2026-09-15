#include "Graphics/GraphicsSettings.h"
#include "Renderer/DiligentD3D11Backend.h"
#include "Platform/PlatformWindow.h"
#include <iostream>
#include <stdexcept>

void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
int main()
{
    try
    {
        {
            Graphics::Store settings;
            Platform::PlatformWindow window;
            Platform::WindowCreateInfo info; info.title = "G0-X graphics snapshot test"; info.resizable = true;
            Check(window.Create(info), "create native window"); window.SetSize(640,480);
            Renderer::DiligentD3D11Backend backend;
            Check(backend.Initialize({window.GetNativeHandle().value,640,480}), "initialize D3D11");
            for (int preset = 0; preset < 4; ++preset)
            {
                settings.ApplyGraphicsPreset(static_cast<Graphics::GraphicsPreset>(preset));
                const auto event = settings.ConsumeChanges();
                Renderer::ApplyGraphicsRuntimeConfig(event.runtime);
                Check(backend.BeginFrame(), "begin after live preset apply");
                Check(backend.GetGraphicsConfig().revision == event.runtime.revision &&
                    backend.GetGraphicsConfig().viewDistance == event.runtime.viewDistance, "Diligent received resolved snapshot");
                Check(!Graphics::GraphicsFeatures{backend.GetGraphicsConfig()}.UseHDR(), "Diligent unsupported HDR stays off");
                backend.Clear({true,Renderer::ClearColor{.08f,.16f,.28f,1}});
                backend.EndFrame(); backend.Present();
                window.Minimize();
                Check(window.IsWindowMinimized(), "native minimize with settings");
                Check(backend.Resize(0,0) && !backend.BeginFrame(), "suspend with settings");
                window.Restore();
                Check(!window.IsWindowMinimized(), "native restore with settings");
                window.SetSize(800,600);
                Check(backend.Resize(800,600), "restore after settings");
            }
            backend.Shutdown(); backend.Shutdown();
            Check(!backend.BeginFrame(), "shutdown releases usable backend");
            window.Destroy();
        }
        Check(Graphics::liveSettingsStores == 0, "zero settings objects");
        std::cout << "PASS Diligent snapshot/presets/suspend/resize/shutdown GraphicsSettingsObjects=0\n";
        return 0;
    }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
