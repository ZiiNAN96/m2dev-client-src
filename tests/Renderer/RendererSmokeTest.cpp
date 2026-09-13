#include "EterLib/StdAfx.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/Camera.h"
#include <d3d11.h>
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Graphics/GraphicsEngineD3D11/interface/RenderDeviceD3D11.h"
#include <wrl/client.h>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>

// The game supplies this member in CameraProcedure.cpp; this test has no game loop.
float CCamera::CAMERA_MAX_DISTANCE = 2500.0f;

namespace
{
void Check(bool condition, const char* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

bool PixelMatches(const unsigned char* pixel, const Renderer::ClearColor& color, bool bgra)
{
    for (unsigned channel = 0; channel < 3; ++channel)
    {
        const int actual = pixel[bgra ? 2 - channel : channel];
        if (std::abs(actual - static_cast<int>(color[channel] * 255.0f)) > 1)
            return false;
    }
    return true;
}

bool PixelsMatch(const void* data, size_t pitch, uint32_t width, uint32_t height,
                 const Renderer::ClearColor& color, bool bgra)
{
    const auto* bytes = static_cast<const unsigned char*>(data);
    return PixelMatches(bytes, color, bgra) &&
           PixelMatches(bytes + (height / 2) * pitch + (width / 2) * 4, color, bgra) &&
           PixelMatches(bytes + (height - 1) * pitch + (width - 1) * 4, color, bgra);
}


}

namespace Renderer
{
class BackendTestAccess
{
public:
    static bool Released(const DiligentD3D11Backend& backend) { return !backend.m_impl; }
    static void Readback(DiligentD3D11Backend& backend, uint32_t width, uint32_t height, const ClearColor& color)
    {
        using namespace Diligent;
        auto& state = *backend.m_impl;
        auto* target = state.swapChain->GetCurrentBackBufferRTV()->GetTexture();
        auto desc = target->GetDesc();
        Check(desc.Width == width && desc.Height == height, "Diligent back buffer extent mismatch");
        Check(desc.Format == TEX_FORMAT_RGBA8_UNORM, "Unexpected Diligent pixel format");
        desc.Name = "Renderer smoke test readback";
        desc.Usage = USAGE_STAGING;
        desc.BindFlags = BIND_NONE;
        desc.CPUAccessFlags = CPU_ACCESS_READ;
        desc.MiscFlags = MISC_TEXTURE_FLAG_NONE;
        RefCntAutoPtr<ITexture> staging;
        state.device->CreateTexture(desc, nullptr, &staging);
        Check(staging != nullptr, "Diligent staging allocation failed");
        CopyTextureAttribs copy;
        copy.pSrcTexture = target;
        copy.pDstTexture = staging;
        copy.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        state.context->CopyTexture(copy);
        state.context->WaitForIdle();
        MappedTextureSubresource mapped;
        state.context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
        Check(mapped.pData != nullptr, "Diligent mapping failed");
        const bool matches = PixelsMatch(mapped.pData, mapped.Stride, width, height, color, false);
        state.context->UnmapTextureSubresource(staging, 0, 0);
        Check(matches, "Diligent clear pixels differ");
        RefCntAutoPtr<IRenderDeviceD3D11> nativeDevice(state.device, IID_RenderDeviceD3D11);
        Check(SUCCEEDED(nativeDevice->GetD3D11Device()->GetDeviceRemovedReason()), "D3D11 device removed");
    }
};
}

int main(int argc, char** argv)
{
    using namespace Renderer;
    const bool diligent = true;
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    const wchar_t* className = L"Metin2RendererGpuTest";
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = className;
    if (!RegisterClassW(&windowClass))
        return 2;
    HWND window = CreateWindowW(className, className, WS_OVERLAPPEDWINDOW,
        0, 0, 640, 480, nullptr, nullptr, instance, nullptr);
    if (!window)
        return 2;
    auto implementation=std::make_unique<DiligentD3D11Backend>();
    auto* concrete=implementation.get();
    std::unique_ptr<IRenderBackend> backend=std::move(implementation);
    const auto readback=[concrete](uint32_t w,uint32_t h,const ClearColor& color) { BackendTestAccess::Readback(*concrete,w,h,color); };
    const auto released=[concrete] { return BackendTestAccess::Released(*concrete); };
    int result = 0;
    try
    {
        Check(!backend->Initialize({}), "Invalid initialization must fail");
        Check(!backend->BeginFrame(), "Begin before Initialize must fail");
        Check(backend->Initialize({window, 640, 480}), "Initialize failed");
        Check(!backend->Initialize({window, 640, 480}), "Duplicate Initialize must fail");
        const auto frame = [&](uint32_t w, uint32_t h, const ClearColor& color)
        {
            Check(backend->BeginFrame(), "BeginFrame failed");
            Check(!backend->BeginFrame(), "Nested BeginFrame must fail");
            Check(!backend->Resize(w, h), "Resize inside frame must fail");
            backend->Clear({true, color});
            backend->Clear({}); // Depth-only clear must preserve the color buffer.
            // ZiiNAN: Exercise the production screenshot readback, including resized extents.
            if(diligent) {
                std::vector<uint8_t> rgb; uint32_t rw=0,rh=0;
                Check(static_cast<DiligentD3D11Backend*>(backend.get())->CaptureRGB(rgb,rw,rh),"screenshot readback");
                Check(rw==w && rh==h && rgb.size()==size_t(w)*h*3,"screenshot extent/stride");
                for(size_t i:{size_t(0),size_t(w)*(h/2)+w/2,size_t(w)*h-1})
                    for(unsigned c=0;c<3;++c) Check(std::abs(int(rgb[i*3+c])-int(color[c]*255))<=1,"screenshot RGB channels");
            }
            backend->EndFrame();
            readback(w, h, color);
            backend->Present();
        };
        frame(640, 480, {0.125f, 0.25f, 0.75f, 1.0f});
        frame(640, 480, {0.75f, 0.25f, 0.125f, 1.0f});
        std::cout << "Initialize / clear pixel readback / depth-only preservation / present: PASS\n";
        for (const auto& extent : {std::array<uint32_t, 2>{800, 600}, {320, 240}, {800, 600}})
        {
            Check(backend->Resize(extent[0], extent[1]), "Resize failed");
            frame(extent[0], extent[1], {0.2f, 0.4f, 0.6f, 1.0f});
        }
        Check(backend->Resize(0, 0), "Minimize failed");
        Check(!backend->BeginFrame(), "Rendering must suspend at zero extent");
        backend->Present();
        Check(backend->Resize(640, 480), "Restore failed");
        frame(640, 480, {0.6f, 0.4f, 0.2f, 1.0f});
        std::cout << "Resize / back buffer extent and pixels / minimize / restore: PASS\n";
        backend->Shutdown();
        backend->Shutdown();
        Check(released(), "Backend resources still owned after Shutdown");
        Check(!backend->BeginFrame(), "Begin after Shutdown must fail");
        Check(!backend->Resize(640, 480), "Resize after Shutdown must fail");
        Check(backend->Initialize({window, 640, 480}), "Reinitialize after Shutdown failed");
        frame(640, 480, {0.1f, 0.5f, 0.9f, 1.0f});
        backend->Shutdown();
        Check(released(), "Resources retained after second Shutdown");
        std::cout << "Idempotent Shutdown / resource release / fresh Initialize: PASS\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        result = 1;
    }
    backend->Shutdown();
    backend.reset();
    DestroyWindow(window);
    UnregisterClassW(className, instance);
    return result;
}
