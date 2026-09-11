#include "EterLib/StdAfx.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/LegacyD3D9Backend.h"
#include "RendererBootstrap.h"
#ifdef M2_ENABLE_DILIGENT_D3D11
#include "DiligentD3D11Backend.h"
#endif
#include <fstream>
#include <memory>

namespace Renderer
{
namespace
{
struct WindowState
{
    IRenderBackend* backend = nullptr;
    bool failed = false;
    bool minimized = false;
};

LRESULT CALLBACK WindowProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
    auto* state = reinterpret_cast<WindowState*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE)
    {
        state = static_cast<WindowState*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (message == WM_SIZE && state)
    {
        state->minimized = wParam == SIZE_MINIMIZED;
        if (state->backend && !state->backend->Resize(LOWORD(lParam), HIWORD(lParam)))
            state->failed = true;
        return 0;
    }
    if (message == WM_CLOSE)
    {
        PostQuitMessage(0); // GPU resources are released before DestroyWindow below.
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}
}

int RunRendererBootstrap(void* instance, const StartupOptions& options)
{
    std::ofstream log("renderer-bootstrap.log", std::ios::trunc);
    log << "backend=" << (options.backend == BackendKind::LegacyD3D9 ? "legacy-d3d9" : "diligent-d3d11") << std::endl;
#ifndef M2_ENABLE_DILIGENT_D3D11
    if (options.backend == BackendKind::DiligentD3D11)
    {
        log << "ERROR: build with -DM2_ENABLE_DILIGENT_D3D11=ON" << std::endl;
        MessageBoxW(nullptr, L"Diligent D3D11 is not included in this build. Enable M2_ENABLE_DILIGENT_D3D11 in CMake.", L"Renderer startup", MB_OK | MB_ICONERROR);
        return 2;
    }
#endif
    const auto module = static_cast<HINSTANCE>(instance);
    const wchar_t* className = L"Metin2RendererBootstrap";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProcedure;
    windowClass.hInstance = module;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass))
        return 3;
    WindowState state;
    HWND window = CreateWindowW(className, L"Metin2 - renderer clear/present", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, nullptr, nullptr, module, &state);
    if (!window)
    {
        UnregisterClassW(className, module);
        return 3;
    }

    CGraphicDevice legacyDevice;
    CScreen legacyScreen;
    std::unique_ptr<IRenderBackend> backend;
    if (options.backend == BackendKind::LegacyD3D9)
        backend = std::make_unique<LegacyD3D9Backend>(legacyDevice, legacyScreen);
#ifdef M2_ENABLE_DILIGENT_D3D11
    else
        backend = std::make_unique<DiligentD3D11Backend>();
#endif
    RECT client{};
    GetClientRect(window, &client);
    int result = 0;
    if (!backend || !backend->Initialize({window, static_cast<uint32_t>(client.right), static_cast<uint32_t>(client.bottom)}))
    {
        log << "ERROR: Initialize failed" << std::endl;
        result = 4;
    }
    else
    {
        log << "Initialize OK" << std::endl;
        state.backend = backend.get();
        ShowWindow(window, options.smokeTest ? SW_HIDE : SW_SHOW);
        unsigned frames = 0;
        bool running = true;
        while (running && !state.failed)
        {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
            {
                if (message.message == WM_QUIT)
                    running = false;
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!running)
                break;
            if (state.minimized)
            {
                WaitMessage();
                continue;
            }
            if (!backend->BeginFrame())
            {
                state.failed = true;
                break;
            }
            backend->Clear({true, ClearColor{0.08f, 0.16f, 0.28f, 1.0f}});
            backend->EndFrame();
            backend->Present();
            ++frames;
            if (options.smokeTest && frames == 2)
            {
                SetWindowPos(window, nullptr, 0, 0, 1024, 720, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
                log << "WM_SIZE resize " << (state.failed ? "FAILED" : "OK") << std::endl;
            }
            if (options.smokeTest && frames == 5)
                running = false;
            Sleep(1);
        }
        result = state.failed ? 5 : 0;
        log << "Frames=" << frames << " result=" << result << std::endl;
    }
    state.backend = nullptr;
    if (backend)
        backend->Shutdown();
    backend.reset();
    log << "Shutdown OK" << std::endl;
    DestroyWindow(window);
    UnregisterClassW(className, module);
    return result;
}
}
