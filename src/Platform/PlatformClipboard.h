#pragma once

#include "NativeTypes.h"

#include <string>
#include <string_view>

namespace Platform::Clipboard
{
// Text crosses the platform boundary as UTF-8.
[[nodiscard]] bool SetText(std::string_view text, NativeWindowHandle owner = {}) noexcept;
[[nodiscard]] bool GetText(std::string& text, NativeWindowHandle owner = {}) noexcept;
}
