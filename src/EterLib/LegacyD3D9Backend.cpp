#include "StdAfx.h"
#include "LegacyD3D9Backend.h"
#include "GrpDevice.h"
#include "GrpScreen.h"

namespace Renderer
{
LegacyD3D9Backend::LegacyD3D9Backend(CGraphicDevice& device, CScreen& screen)
    : m_device(device), m_screen(screen)
{
}

LegacyD3D9Backend::~LegacyD3D9Backend() { Shutdown(); }

bool LegacyD3D9Backend::Initialize(const InitializeInfo& info)
{
    if (m_initialized || !info.window || !info.width || !info.height)
        return false;

    m_device.InitBackBufferCount(2);
    m_createResult = m_device.Create(static_cast<HWND>(info.window), info.width, info.height,
                                    info.windowed, info.bitsPerPixel, info.refreshRate);
    m_initialized = (m_createResult & CGraphicDevice::CREATE_OK) != 0 ||
                    m_createResult == CGraphicDevice::CREATE_REFRESHRATE;
    if (!m_initialized)
        m_device.Destroy();
    return m_initialized;
}

bool LegacyD3D9Backend::BeginFrame()
{
    if (!m_initialized || m_suspended || m_inFrame)
        return false;
    m_inFrame = m_screen.Begin();
    return m_inFrame;
}

void LegacyD3D9Backend::Clear(const ClearInfo& info)
{
    if (!m_inFrame)
        return;
    if (info.color)
    {
        const auto& c = *info.color;
        m_screen.SetClearColor(c[0], c[1], c[2], c[3]);
    }
    if (info.colorAndDepth)
        m_screen.Clear();
    else
        m_screen.ClearDepthBuffer();
}

void LegacyD3D9Backend::EndFrame()
{
    if (m_inFrame)
        m_screen.End();
    m_inFrame = false;
}

void LegacyD3D9Backend::Present()
{
    if (m_initialized && !m_suspended && !m_inFrame)
        m_screen.Show();
}

bool LegacyD3D9Backend::Resize(uint32_t width, uint32_t height)
{
    if (!m_initialized || m_inFrame)
        return false;
    m_suspended = width == 0 || height == 0;
    return m_suspended || m_device.ResizeBackBuffer(width, height);
}

void LegacyD3D9Backend::Shutdown()
{
    EndFrame();
    if (m_initialized)
        m_device.Destroy();
    m_initialized = false;
    m_suspended = false;
}
}
