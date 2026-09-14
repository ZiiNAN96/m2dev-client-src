#pragma once

#include "NativeTypes.h"

#include <string_view>

namespace Platform::Shell
{
// ZiiNAN: Platform abstraction
[[nodiscard]] bool Open(std::string_view target, NativeWindowHandle owner = {}) noexcept;
[[nodiscard]] bool OpenUrl(std::string_view url, NativeWindowHandle owner = {}) noexcept;
}
