#pragma once

namespace Platform
{
// ZiiNAN: Platform abstraction
struct NativeWindowHandle
{
    void* value = nullptr;
    explicit operator bool() const noexcept { return value != nullptr; }
    bool operator==(const NativeWindowHandle&) const = default;
};

struct Point { int x = 0, y = 0; };
struct Rect { int left = 0, top = 0, right = 0, bottom = 0; };
}
