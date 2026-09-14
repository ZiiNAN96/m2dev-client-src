#pragma once

#include "Platform/PlatformNetworking.h"

#include <winsock.h>

namespace Platform::Networking::Windows
{
[[nodiscard]] inline SOCKET ToSocket(NativeSocket socket) noexcept
{
    return static_cast<SOCKET>(socket.value);
}

[[nodiscard]] inline NativeSocket FromSocket(SOCKET socket) noexcept
{
    return NativeSocket{static_cast<std::uintptr_t>(socket)};
}
}
