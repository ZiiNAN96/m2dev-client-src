#pragma once

#include "Platform/NativeTypes.h"
#include <cstdint>
#include <memory>

namespace Renderer
{
class VulkanBootstrap final
{
public:
    VulkanBootstrap();
    ~VulkanBootstrap();
    VulkanBootstrap(const VulkanBootstrap&) = delete;
    VulkanBootstrap& operator=(const VulkanBootstrap&) = delete;
    [[nodiscard]] bool Initialize(Platform::NativeWindowHandle window, std::uint32_t width, std::uint32_t height);
    [[nodiscard]] bool Resize(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] bool ClearAndPresent();
    void Shutdown() noexcept;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
