#pragma once

#include "Platform/NativeTypes.h"
#include <cstdint>

struct ANativeWindow;

namespace Platform::Android
{
class Window final
{
public:
    ~Window();
    Window() = default;
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    void Attach(ANativeWindow* window) noexcept;
    void Reset() noexcept;
    [[nodiscard]] NativeWindowHandle GetNativeHandle() const noexcept;
    [[nodiscard]] std::uint32_t Width() const noexcept;
    [[nodiscard]] std::uint32_t Height() const noexcept;

private:
    ANativeWindow* m_window = nullptr;
};
}
