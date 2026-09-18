#include "StdAfx.h"
#include "PythonApplication.h"
#include "DisplayConfiguration.h"

void CPythonApplication::RememberDisplayWindow()
{
    ::GetWindowRect(static_cast<HWND>(GetNativeHandle().value), &m_displayWindowRect);
}

bool CPythonApplication::ApplyDisplayConfiguration(const Graphics::GraphicsSettings& settings, bool restorePosition)
{
    const auto window = static_cast<HWND>(GetNativeHandle().value);
    if (!window) return false;
    const auto monitor = DisplayConfiguration::Query(window);
    auto validated = settings;
    if (!DisplayConfiguration::Normalize(validated, monitor, false)) return false;
    const bool borderless = settings.displayMode == Graphics::DisplayMode::Borderless;
    const DWORD style = borderless ? WS_POPUP : DisplayConfiguration::WindowStyle;
    RECT outer{0, 0, LONG(settings.resolutionWidth), LONG(settings.resolutionHeight)};
    if (!borderless && !AdjustWindowRectEx(&outer, style, FALSE, 0)) return false;
    const LONG width = outer.right - outer.left, height = outer.bottom - outer.top;
    const auto& work = monitor.info.rcWork;
    LONG x = borderless ? monitor.info.rcMonitor.left : work.left + (work.right - work.left - width) / 2;
    LONG y = borderless ? monitor.info.rcMonitor.top : work.top + (work.bottom - work.top - height) / 2;
    if (restorePosition && !borderless)
    {
        x = std::clamp(m_displayWindowRect.left, work.left, std::max(work.left, work.right - width));
        y = std::clamp(m_displayWindowRect.top, work.top, std::max(work.top, work.bottom - height));
    }
    m_applyingDisplay = true;
    // A timeout also runs while minimized. Restore without taking focus before
    // checking the client area; never leave the renderer permanently suspended.
    if (IsIconic(window)) ShowWindow(window, SW_SHOWNOACTIVATE);
    SetLastError(0);
    const auto oldStyle = SetWindowLongPtrW(window, GWL_STYLE, style | WS_VISIBLE | WS_CLIPCHILDREN);
    bool ok = oldStyle != 0 || GetLastError() == 0;
    if (ok) ok = SetWindowPos(window, HWND_NOTOPMOST, x, y, width, height,
        SWP_FRAMECHANGED | SWP_NOACTIVATE) != FALSE;
    m_applyingDisplay = false;
    RECT client{};
    if (!ok || !::GetClientRect(window, &client) ||
        unsigned(client.right) != settings.resolutionWidth || unsigned(client.bottom) != settings.resolutionHeight) return false;
    if (m_terrainPresentation && !m_terrainPresentation->Resize(settings.resolutionWidth, settings.resolutionHeight)) return false;
    if (m_terrainPresentation && !m_grpDevice.ResizeBackBuffer(settings.resolutionWidth, settings.resolutionHeight)) return false;
    m_isWindowed = !borderless;
    m_isMinimizedWnd = false;
    m_isWindowFullScreenEnable = FALSE; // No ChangeDisplaySettings / legacy fullscreen on Alt-Tab.
    OnSizeChange(settings.resolutionWidth, settings.resolutionHeight);
    return true;
}
