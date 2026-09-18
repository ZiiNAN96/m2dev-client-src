#pragma once
#include "Graphics/GraphicsSettings.h"
#include <windows.h>
#include <algorithm>
#include <vector>

// Monitor capabilities and window geometry, never another settings store.
namespace DisplayConfiguration
{
using Resolution = std::pair<unsigned, unsigned>;
inline constexpr DWORD WindowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
struct Monitor
{
    MONITORINFOEXW info{};
    std::vector<Resolution> modes;
    Resolution desktop{}, safeWindow{};
};
inline Monitor Query(HWND window)
{
    Monitor result;
    result.info.cbSize = sizeof(result.info);
    if (!GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &result.info)) return result;
    const auto& rect = result.info.rcMonitor;
    result.desktop = {unsigned(rect.right - rect.left), unsigned(rect.bottom - rect.top)};
    RECT frame{};
    AdjustWindowRectEx(&frame, WindowStyle, FALSE, 0);
    const auto& work = result.info.rcWork;
    const auto maxWidth = unsigned(std::max(1L, work.right - work.left - (frame.right - frame.left)));
    const auto maxHeight = unsigned(std::max(1L, work.bottom - work.top - (frame.bottom - frame.top)));
    DEVMODEW mode{}; mode.dmSize = sizeof(mode);
    for (DWORD i = 0; EnumDisplaySettingsW(result.info.szDevice, i, &mode); ++i)
    {
        if (mode.dmBitsPerPel != 32 || mode.dmPelsWidth < 800 || mode.dmPelsHeight < 600 ||
            mode.dmPelsWidth > maxWidth || mode.dmPelsHeight > maxHeight) continue;
        result.modes.emplace_back(mode.dmPelsWidth, mode.dmPelsHeight);
    }
    std::sort(result.modes.begin(), result.modes.end());
    result.modes.erase(std::unique(result.modes.begin(), result.modes.end()), result.modes.end());
    // If the monitor supplies no fitting mode, derive a safe window from its
    // actual work area. This is the sole fallback, not an invented mode list.
    result.safeWindow = result.modes.empty() ? Resolution{maxWidth, maxHeight} : result.modes.back();
    if (result.modes.empty()) result.modes.push_back(result.safeWindow);
    return result;
}
inline bool Normalize(Graphics::GraphicsSettings& s, const Monitor& monitor, bool fallback)
{
    using Graphics::DisplayMode;
    if (!monitor.desktop.first || !monitor.desktop.second) return false;
    const Resolution requested{s.resolutionWidth, s.resolutionHeight};
    const bool supported = s.displayMode == DisplayMode::Borderless ? requested == monitor.desktop :
        s.displayMode == DisplayMode::Windowed && std::find(monitor.modes.begin(), monitor.modes.end(), requested) != monitor.modes.end();
    if (supported) return true;
    if (!fallback) return false;
    s.displayMode = DisplayMode::Windowed;
    s.resolutionWidth = monitor.safeWindow.first;
    s.resolutionHeight = monitor.safeWindow.second;
    return true;
}
inline void Copy(Graphics::GraphicsSettings& target, const Graphics::GraphicsSettings& source)
{
    target.resolutionWidth = source.resolutionWidth;
    target.resolutionHeight = source.resolutionHeight;
    target.displayMode = source.displayMode;
}
inline bool Equal(const Graphics::GraphicsSettings& a, const Graphics::GraphicsSettings& b)
{
    return a.resolutionWidth == b.resolutionWidth && a.resolutionHeight == b.resolutionHeight && a.displayMode == b.displayMode;
}
}
