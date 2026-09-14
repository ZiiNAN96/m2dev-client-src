#pragma once

#include <cstdint>

namespace Platform::Android
{
// ZiiNAN: Cross-platform bootstrap
class Lifecycle final
{
public:
    void SetResumed(bool resumed) noexcept { m_resumed = resumed; }
    void SetFocused(bool focused) noexcept { m_focused = focused; }
    void SetSurfaceSize(std::uint32_t width, std::uint32_t height) noexcept
    {
        m_width = width;
        m_height = height;
    }
    void SetRendererReady(bool ready) noexcept { m_rendererReady = ready; }
    void DestroySurface() noexcept
    {
        m_rendererReady = false;
        m_width = m_height = 0;
    }
    void Destroy() noexcept
    {
        DestroySurface();
        m_destroyed = true;
    }
    [[nodiscard]] bool CanRender() const noexcept
    {
        return !m_destroyed && m_resumed && m_focused && m_rendererReady && m_width != 0 && m_height != 0;
    }

private:
    bool m_resumed = false;
    bool m_focused = false;
    bool m_rendererReady = false;
    bool m_destroyed = false;
    std::uint32_t m_width = 0;
    std::uint32_t m_height = 0;
};
}
