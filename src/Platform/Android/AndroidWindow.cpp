#include "AndroidWindow.h"
#include <android/native_window.h>

namespace Platform::Android
{
Window::~Window() { Reset(); }

void Window::Attach(ANativeWindow* window) noexcept
{
    if (m_window == window)
        return;
    if (window)
        ANativeWindow_acquire(window);
    Reset();
    m_window = window;
}

void Window::Reset() noexcept
{
    if (m_window)
        ANativeWindow_release(m_window);
    m_window = nullptr;
}

NativeWindowHandle Window::GetNativeHandle() const noexcept { return {m_window}; }

std::uint32_t Window::Width() const noexcept
{
    const auto width = m_window ? ANativeWindow_getWidth(m_window) : 0;
    return width > 0 ? static_cast<std::uint32_t>(width) : 0;
}

std::uint32_t Window::Height() const noexcept
{
    const auto height = m_window ? ANativeWindow_getHeight(m_window) : 0;
    return height > 0 ? static_cast<std::uint32_t>(height) : 0;
}
}
