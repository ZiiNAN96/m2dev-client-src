#pragma once

#include "NativeTypes.h"

#include <cstdint>
#include <functional>
#include <memory>

namespace Platform
{
// ZiiNAN: Platform abstraction
using ScanCode = std::uint16_t;
using KeyEventHandler = std::function<void(ScanCode)>;

class PlatformInput
{
public:
    PlatformInput();
    ~PlatformInput();
    PlatformInput(PlatformInput&&) noexcept;
    PlatformInput& operator=(PlatformInput&&) noexcept;

    PlatformInput(const PlatformInput&) = delete;
    PlatformInput& operator=(const PlatformInput&) = delete;

    bool Initialize(NativeWindowHandle window, KeyEventHandler keyDown, KeyEventHandler keyUp);
    void Update();
    void Reset();
    bool IsPressed(ScanCode code) const;

    void DisableAccessibilityShortcuts();
    void RestoreAccessibilityShortcuts();

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
