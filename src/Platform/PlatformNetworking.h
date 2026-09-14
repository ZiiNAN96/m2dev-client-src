#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace Platform::Networking
{
struct NativeSocket
{
    std::uintptr_t value = std::numeric_limits<std::uintptr_t>::max();

    explicit operator bool() const noexcept
    {
        return value != std::numeric_limits<std::uintptr_t>::max();
    }

    bool operator==(const NativeSocket&) const = default;
};

// ZiiNAN: Platform abstraction
[[nodiscard]] bool Startup() noexcept;
void Shutdown() noexcept;
void Close(NativeSocket& socket) noexcept;
[[nodiscard]] int LastError() noexcept;
[[nodiscard]] bool IsWouldBlock(int error) noexcept;
[[nodiscard]] bool GetLocalHostName(char* destination, std::size_t size) noexcept;
[[nodiscard]] bool ResolveIPv4(std::string_view address, std::uint32_t& hostOrderAddress) noexcept;
}
