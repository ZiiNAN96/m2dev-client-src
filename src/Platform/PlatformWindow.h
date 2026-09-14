#pragma once

#include "NativeTypes.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

namespace Platform
{
// ZiiNAN: Platform abstraction
enum class WindowStyle
{
    Windowed,
    Popup,
};

struct WindowCreateInfo
{
    std::string title;
    WindowStyle style = WindowStyle::Windowed;
    bool resizable = false;
    bool quitOnClose = true;
    int classBrush = 4;
    int iconResource = 0;
    int cursorResource = 32512;
};

struct NativeMessage
{
    NativeWindowHandle window;
    std::uint32_t id = 0;
    std::uintptr_t wParam = 0;
    std::intptr_t lParam = 0;
};

using NativeMessageHandler = std::function<std::intptr_t(const NativeMessage&, bool& handled)>;

enum class PollResult
{
    Idle,
    Dispatched,
    Quit,
};

struct NativeCursorHandle
{
    void* value = nullptr;
    explicit operator bool() const noexcept { return value != nullptr; }
    bool operator==(const NativeCursorHandle&) const = default;
};

class PlatformWindow;

class PlatformChildWindow
{
public:
    ~PlatformChildWindow();
    PlatformChildWindow(PlatformChildWindow&&) noexcept;
    PlatformChildWindow& operator=(PlatformChildWindow&&) noexcept;

    PlatformChildWindow(const PlatformChildWindow&) = delete;
    PlatformChildWindow& operator=(const PlatformChildWindow&) = delete;

    NativeWindowHandle GetNativeHandle() const noexcept;
    bool Resize(std::uint32_t width, std::uint32_t height);
    void Show(bool visible);
    Rect GetClientRect() const;
    bool IsVisible() const noexcept;

private:
    struct Impl;
    explicit PlatformChildWindow(std::unique_ptr<Impl> impl) noexcept;
    std::unique_ptr<Impl> m_impl;

    friend class PlatformWindow;
};

class PlatformWindow
{
public:
    PlatformWindow();
    ~PlatformWindow();
    PlatformWindow(PlatformWindow&&) noexcept;
    PlatformWindow& operator=(PlatformWindow&&) noexcept;

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    void SetInstance(void* instance) noexcept;
    void SetMessageHandler(NativeMessageHandler handler);

    bool Create(const WindowCreateInfo& info);
    void Destroy();
    NativeWindowHandle GetNativeHandle() const noexcept;
    bool IsNativeHandleValid(NativeWindowHandle handle) const noexcept;

    PollResult PollEvents();
    bool HasPendingEvents() const;
    void WaitForEvents() const;
    void RequestQuit(int exitCode) const;
    bool RequestClose() const;
    std::intptr_t DefaultWindowProcedure(const NativeMessage& message);

    bool IsVisible() const noexcept;
    bool IsActive() const noexcept;
    bool IsWindowMinimized() const noexcept;
    void Show(bool visible);
    void SetFocus();
    void Minimize();
    void Restore();
    void SetPosition(int x, int y);
    void SetSize(int width, int height);
    void AdjustClientSize(int width, int height);
    void SetTitle(const char* title);

    Rect GetClientRect() const;
    Rect GetWindowRect() const;
    Point GetMousePosition() const;
    int GetScreenWidth() const;
    int GetScreenHeight() const;

    bool GetCursorScreenPosition(Point& point) const;
    bool ScreenToClient(Point& point) const;
    bool ClientToScreen(Point& point) const;
    bool SetCursorScreenPosition(Point point) const;
    void CaptureMouse();
    void ReleaseMouseCapture();
    bool HasMouseCapture() const;

    NativeCursorHandle LoadCursorResource(int resource, int width = 32, int height = 32) const;
    void DestroyCursorResource(NativeCursorHandle cursor) const;
    void SetCursor(NativeCursorHandle cursor) const;
    int AdjustCursorVisibility(bool visible) const;
    void SetCursorVisible(bool visible) const;

    bool HasWindowWithTitle(const char* title) const;
    void EnterFullscreen(std::uint32_t width, std::uint32_t height, std::uint32_t bitsPerPixel);
    void MinimizeFullscreen(std::uint32_t width, std::uint32_t height);

    std::unique_ptr<PlatformChildWindow> CreateChildRenderSurface(std::uint32_t width, std::uint32_t height);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
