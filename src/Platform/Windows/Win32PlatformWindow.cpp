#include "Platform/PlatformWindow.h"

#include <windows.h>

#include <atomic>
#include <utility>

namespace Platform
{
namespace
{
struct SharedWindowState
{
    HWND window = nullptr;
    std::size_t childCount = 0;
    bool restoreClipChildren = false;
};

std::atomic<unsigned long> nextWindowClass{1};

std::wstring ToWide(const std::string& value)
{
    if (value.empty())
        return {};
    const int count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                          static_cast<int>(value.size()), nullptr, 0);
    if (count <= 0)
        return {};
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                            result.data(), count) != count)
        return {};
    return result;
}
}

struct PlatformWindow::Impl
{
    HINSTANCE instance = GetModuleHandleW(nullptr);
    std::shared_ptr<SharedWindowState> shared = std::make_shared<SharedWindowState>();
    NativeMessageHandler messageHandler;
    std::wstring className;
    bool classRegistered = false;
    bool active = false;
    bool visible = false;
    bool minimized = false;
    bool quitOnClose = true;

    ~Impl()
    {
        messageHandler = {};
        Destroy();
    }

    static LRESULT CALLBACK WindowProcedure(HWND window, UINT id, WPARAM wParam, LPARAM lParam)
    {
        auto* impl = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (!impl)
            return DefWindowProcW(window, id, wParam, lParam);
        if (impl->messageHandler)
        {
            bool handled = false;
            const NativeMessage message{{window}, id, static_cast<std::uintptr_t>(wParam), static_cast<std::intptr_t>(lParam)};
            const auto result = impl->messageHandler(message, handled);
            if (handled)
                return static_cast<LRESULT>(result);
        }
        return impl->DefaultProcedure(window, id, wParam, lParam);
    }

    LRESULT DefaultProcedure(HWND window, UINT id, WPARAM wParam, LPARAM lParam)
    {
        if (id == WM_SIZE)
        {
            minimized = wParam == SIZE_MINIMIZED;
            active = !minimized;
            visible = !minimized;
            if (minimized)
                InvalidateRect(window, nullptr, TRUE);
        }
        else if (id == WM_ACTIVATEAPP)
        {
            active = wParam == WA_ACTIVE || wParam == WA_CLICKACTIVE;
        }
        else if (id == WM_CLOSE && quitOnClose)
        {
            PostQuitMessage(0);
            return 0;
        }
        return DefWindowProcW(window, id, wParam, lParam);
    }

    void Destroy()
    {
        if (shared->window && IsWindow(shared->window))
            DestroyWindow(shared->window);
        shared->window = nullptr;
        active = false;
        visible = false;
        minimized = false;
        if (classRegistered)
        {
            UnregisterClassW(className.c_str(), instance);
            classRegistered = false;
        }
    }
};

struct PlatformChildWindow::Impl
{
    std::shared_ptr<SharedWindowState> parent;
    HWND window = nullptr;

    ~Impl()
    {
        if (window && IsWindow(window))
            DestroyWindow(window);
        if (!parent || parent->childCount == 0)
            return;
        --parent->childCount;
        if (parent->childCount == 0 && parent->restoreClipChildren && parent->window && IsWindow(parent->window))
        {
            const LONG_PTR style = GetWindowLongPtrW(parent->window, GWL_STYLE);
            SetWindowLongPtrW(parent->window, GWL_STYLE, style & ~static_cast<LONG_PTR>(WS_CLIPCHILDREN));
            parent->restoreClipChildren = false;
        }
    }
};

PlatformChildWindow::PlatformChildWindow(std::unique_ptr<Impl> impl) noexcept : m_impl(std::move(impl)) {}
PlatformChildWindow::~PlatformChildWindow() = default;
PlatformChildWindow::PlatformChildWindow(PlatformChildWindow&&) noexcept = default;
PlatformChildWindow& PlatformChildWindow::operator=(PlatformChildWindow&&) noexcept = default;

NativeWindowHandle PlatformChildWindow::GetNativeHandle() const noexcept
{
    return {m_impl ? m_impl->window : nullptr};
}

bool PlatformChildWindow::Resize(std::uint32_t width, std::uint32_t height)
{
    if (!m_impl || !m_impl->window || !width || !height)
        return false;
    return SetWindowPos(m_impl->window, nullptr, 0, 0, static_cast<int>(width), static_cast<int>(height),
                        SWP_NOZORDER | SWP_NOACTIVATE) != FALSE;
}

void PlatformChildWindow::Show(bool visible)
{
    if (m_impl && m_impl->window)
        ShowWindow(m_impl->window, visible ? SW_SHOWNOACTIVATE : SW_HIDE);
}

Rect PlatformChildWindow::GetClientRect() const
{
    RECT rect{};
    if (m_impl && m_impl->window)
        ::GetClientRect(m_impl->window, &rect);
    return {static_cast<int>(rect.left), static_cast<int>(rect.top),
            static_cast<int>(rect.right), static_cast<int>(rect.bottom)};
}

bool PlatformChildWindow::IsVisible() const noexcept
{
    return m_impl && m_impl->window && IsWindowVisible(m_impl->window) != FALSE;
}

PlatformWindow::PlatformWindow() : m_impl(std::make_unique<Impl>()) {}
PlatformWindow::~PlatformWindow() = default;
PlatformWindow::PlatformWindow(PlatformWindow&&) noexcept = default;
PlatformWindow& PlatformWindow::operator=(PlatformWindow&&) noexcept = default;

void PlatformWindow::SetInstance(void* instance) noexcept
{
    if (m_impl && instance)
        m_impl->instance = static_cast<HINSTANCE>(instance);
}

void PlatformWindow::SetMessageHandler(NativeMessageHandler handler)
{
    if (m_impl)
        m_impl->messageHandler = std::move(handler);
}

bool PlatformWindow::Create(const WindowCreateInfo& info)
{
    if (!m_impl)
        return false;
    Destroy();
    m_impl->shared = std::make_shared<SharedWindowState>();
    m_impl->quitOnClose = info.quitOnClose;
    m_impl->className = L"ZiiNANPlatformWindow-" + std::to_wstring(nextWindowClass.fetch_add(1));

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = Impl::WindowProcedure;
    windowClass.hInstance = m_impl->instance;
    windowClass.hCursor = LoadCursorW(m_impl->instance, MAKEINTRESOURCEW(info.cursorResource));
    if (!windowClass.hCursor)
        windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    if (info.iconResource)
        windowClass.hIcon = LoadIconW(m_impl->instance, MAKEINTRESOURCEW(info.iconResource));
    windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(info.classBrush));
    windowClass.lpszClassName = m_impl->className.c_str();
    if (!RegisterClassW(&windowClass))
        return false;
    m_impl->classRegistered = true;

    DWORD style = info.style == WindowStyle::Popup
        ? WS_POPUP
        : WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    if (info.resizable && info.style == WindowStyle::Windowed)
        style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
    const std::wstring title = ToWide(info.title);
    const HWND window = CreateWindowW(m_impl->className.c_str(), title.c_str(), style,
                                      0, 0, 0, 0, nullptr, nullptr, m_impl->instance, nullptr);
    if (!window)
    {
        m_impl->Destroy();
        return false;
    }
    m_impl->shared->window = window;
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(m_impl.get()));
    return true;
}

void PlatformWindow::Destroy()
{
    if (m_impl)
        m_impl->Destroy();
}

NativeWindowHandle PlatformWindow::GetNativeHandle() const noexcept
{
    return {m_impl ? m_impl->shared->window : nullptr};
}

bool PlatformWindow::IsNativeHandleValid(NativeWindowHandle handle) const noexcept
{
    return handle && IsWindow(static_cast<HWND>(handle.value)) != FALSE;
}

PollResult PlatformWindow::PollEvents()
{
    MSG message{};
    if (!PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE))
        return PollResult::Idle;
    const BOOL result = GetMessageW(&message, nullptr, 0, 0);
    if (result <= 0)
        return PollResult::Quit;
    TranslateMessage(&message);
    DispatchMessageW(&message);
    return PollResult::Dispatched;
}

bool PlatformWindow::HasPendingEvents() const
{
    MSG message{};
    return PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE) != FALSE;
}

void PlatformWindow::WaitForEvents() const
{
    WaitMessage();
}

void PlatformWindow::RequestQuit(int exitCode) const
{
    PostQuitMessage(exitCode);
}

bool PlatformWindow::RequestClose() const
{
    return m_impl && m_impl->shared->window &&
           PostMessageW(m_impl->shared->window, WM_CLOSE, 0, 0) != FALSE;
}

std::intptr_t PlatformWindow::DefaultWindowProcedure(const NativeMessage& message)
{
    return static_cast<std::intptr_t>(m_impl->DefaultProcedure(static_cast<HWND>(message.window.value),
                                                               message.id, static_cast<WPARAM>(message.wParam),
                                                               static_cast<LPARAM>(message.lParam)));
}

bool PlatformWindow::IsVisible() const noexcept { return m_impl->visible; }
bool PlatformWindow::IsActive() const noexcept { return m_impl->active; }
bool PlatformWindow::IsWindowMinimized() const noexcept { return m_impl->minimized; }

void PlatformWindow::Show(bool visible)
{
    m_impl->visible = visible;
    if (m_impl->shared->window)
        ShowWindow(m_impl->shared->window, visible ? SW_SHOW : SW_HIDE);
}

void PlatformWindow::SetFocus()
{
    if (m_impl->shared->window)
        ::SetFocus(m_impl->shared->window);
}

void PlatformWindow::Minimize()
{
    if (m_impl->shared->window)
        ShowWindow(m_impl->shared->window, SW_MINIMIZE);
}

void PlatformWindow::Restore()
{
    if (m_impl->shared->window)
        ShowWindow(m_impl->shared->window, SW_RESTORE);
}

void PlatformWindow::SetPosition(int x, int y)
{
    if (m_impl->shared->window)
        SetWindowPos(m_impl->shared->window, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void PlatformWindow::SetSize(int width, int height)
{
    if (m_impl->shared->window)
        SetWindowPos(m_impl->shared->window, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOMOVE);
}

void PlatformWindow::AdjustClientSize(int width, int height)
{
    if (!m_impl->shared->window)
        return;
    RECT rect{0, 0, width, height};
    AdjustWindowRectEx(&rect, static_cast<DWORD>(GetWindowLongPtrW(m_impl->shared->window, GWL_STYLE)),
                       GetMenu(m_impl->shared->window) != nullptr,
                       static_cast<DWORD>(GetWindowLongPtrW(m_impl->shared->window, GWL_EXSTYLE)));
    MoveWindow(m_impl->shared->window, 0, 0, rect.right - rect.left, rect.bottom - rect.top, FALSE);
}

void PlatformWindow::SetTitle(const char* title)
{
    if (m_impl->shared->window)
        SetWindowTextW(m_impl->shared->window, ToWide(title ? title : "").c_str());
}

Rect PlatformWindow::GetClientRect() const
{
    RECT rect{};
    if (m_impl->shared->window)
        ::GetClientRect(m_impl->shared->window, &rect);
    return {static_cast<int>(rect.left), static_cast<int>(rect.top),
            static_cast<int>(rect.right), static_cast<int>(rect.bottom)};
}

Rect PlatformWindow::GetWindowRect() const
{
    RECT rect{};
    if (m_impl->shared->window)
        ::GetWindowRect(m_impl->shared->window, &rect);
    return {static_cast<int>(rect.left), static_cast<int>(rect.top),
            static_cast<int>(rect.right), static_cast<int>(rect.bottom)};
}

Point PlatformWindow::GetMousePosition() const
{
    POINT point{};
    if (GetCursorPos(&point) && m_impl->shared->window)
        ::ScreenToClient(m_impl->shared->window, &point);
    return {static_cast<int>(point.x), static_cast<int>(point.y)};
}

int PlatformWindow::GetScreenWidth() const { return GetSystemMetrics(SM_CXSCREEN); }
int PlatformWindow::GetScreenHeight() const { return GetSystemMetrics(SM_CYSCREEN); }

bool PlatformWindow::GetCursorScreenPosition(Point& point) const
{
    POINT native{};
    if (!GetCursorPos(&native))
        return false;
    point = {static_cast<int>(native.x), static_cast<int>(native.y)};
    return true;
}

bool PlatformWindow::ScreenToClient(Point& point) const
{
    POINT native{point.x, point.y};
    if (!m_impl->shared->window || !::ScreenToClient(m_impl->shared->window, &native))
        return false;
    point = {static_cast<int>(native.x), static_cast<int>(native.y)};
    return true;
}

bool PlatformWindow::ClientToScreen(Point& point) const
{
    POINT native{point.x, point.y};
    if (!m_impl->shared->window || !::ClientToScreen(m_impl->shared->window, &native))
        return false;
    point = {static_cast<int>(native.x), static_cast<int>(native.y)};
    return true;
}

bool PlatformWindow::SetCursorScreenPosition(Point point) const
{
    return SetCursorPos(point.x, point.y) != FALSE;
}

void PlatformWindow::CaptureMouse()
{
    if (m_impl->shared->window)
        SetCapture(m_impl->shared->window);
}

void PlatformWindow::ReleaseMouseCapture() { ReleaseCapture(); }
bool PlatformWindow::HasMouseCapture() const { return GetCapture() == m_impl->shared->window; }

NativeCursorHandle PlatformWindow::LoadCursorResource(int resource, int width, int height) const
{
    return {LoadImageW(m_impl->instance, MAKEINTRESOURCEW(resource), IMAGE_CURSOR, width, height, LR_VGACOLOR)};
}

void PlatformWindow::DestroyCursorResource(NativeCursorHandle cursor) const
{
    if (cursor)
        DestroyCursor(static_cast<HCURSOR>(cursor.value));
}

void PlatformWindow::SetCursor(NativeCursorHandle cursor) const
{
    ::SetCursor(static_cast<HCURSOR>(cursor.value));
}

int PlatformWindow::AdjustCursorVisibility(bool visible) const
{
    return ShowCursor(visible ? TRUE : FALSE);
}

void PlatformWindow::SetCursorVisible(bool visible) const
{
    int counter;
    do
    {
        counter = AdjustCursorVisibility(visible);
    } while (visible ? counter < 0 : counter >= 0);
}

bool PlatformWindow::HasWindowWithTitle(const char* title) const
{
    return FindWindowW(nullptr, ToWide(title ? title : "").c_str()) != nullptr;
}

void PlatformWindow::EnterFullscreen(std::uint32_t width, std::uint32_t height, std::uint32_t bitsPerPixel)
{
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    mode.dmBitsPerPel = bitsPerPixel;
    mode.dmPelsWidth = width;
    mode.dmPelsHeight = height;
    mode.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;
    if (ChangeDisplaySettingsW(&mode, CDS_FULLSCREEN) == DISP_CHANGE_RESTART)
        ChangeDisplaySettingsW(nullptr, 0);
}

void PlatformWindow::MinimizeFullscreen(std::uint32_t width, std::uint32_t height)
{
    ChangeDisplaySettingsW(nullptr, 0);
    if (!m_impl->shared->window)
        return;
    SetWindowPos(m_impl->shared->window, nullptr, 0, 0, static_cast<int>(width), static_cast<int>(height), SWP_SHOWWINDOW);
    ShowWindow(m_impl->shared->window, SW_MINIMIZE);
}

std::unique_ptr<PlatformChildWindow> PlatformWindow::CreateChildRenderSurface(std::uint32_t width, std::uint32_t height)
{
    if (!m_impl->shared->window || !IsWindow(m_impl->shared->window) || !width || !height)
        return {};
    if (m_impl->shared->childCount == 0)
    {
        const LONG_PTR style = GetWindowLongPtrW(m_impl->shared->window, GWL_STYLE);
        m_impl->shared->restoreClipChildren = (style & WS_CLIPCHILDREN) == 0;
        if (m_impl->shared->restoreClipChildren)
            SetWindowLongPtrW(m_impl->shared->window, GWL_STYLE, style | WS_CLIPCHILDREN);
    }
    const HWND child = CreateWindowExW(0, L"STATIC", L"Metin2 Diligent terrain", WS_CHILD | WS_DISABLED,
                                       0, 0, static_cast<int>(width), static_cast<int>(height),
                                       m_impl->shared->window, nullptr, m_impl->instance, nullptr);
    if (!child)
    {
        if (m_impl->shared->childCount == 0 && m_impl->shared->restoreClipChildren)
        {
            const LONG_PTR style = GetWindowLongPtrW(m_impl->shared->window, GWL_STYLE);
            SetWindowLongPtrW(m_impl->shared->window, GWL_STYLE, style & ~static_cast<LONG_PTR>(WS_CLIPCHILDREN));
            m_impl->shared->restoreClipChildren = false;
        }
        return {};
    }
    ++m_impl->shared->childCount;
    auto impl = std::make_unique<PlatformChildWindow::Impl>();
    impl->parent = m_impl->shared;
    impl->window = child;
    return std::unique_ptr<PlatformChildWindow>(new PlatformChildWindow(std::move(impl)));
}
}
