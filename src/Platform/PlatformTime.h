#pragma once

#include <cstdint>

namespace Platform::Time
{
// ZiiNAN: Platform abstraction
[[nodiscard]] bool Initialize() noexcept;
[[nodiscard]] std::uint32_t TickMilliseconds() noexcept;
[[nodiscard]] std::uint32_t UptimeMilliseconds() noexcept;
[[nodiscard]] std::uint64_t MonotonicNanoseconds() noexcept;
void SleepMilliseconds(std::uint32_t milliseconds) noexcept;

[[nodiscard]] bool BeginTimerPeriod() noexcept;
void EndTimerPeriod() noexcept;
}
