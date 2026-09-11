#pragma once

// Implementation-only Diligent types; also used by the native GPU readback test.
#include "DiligentD3D11Backend.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"

struct Renderer::DiligentD3D11Backend::Impl
{
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> device;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> context;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> swapChain;
    Renderer::ClearColor color{0.08f, 0.16f, 0.28f, 1.0f};
    bool suspended = false;
    bool inFrame = false;
};
