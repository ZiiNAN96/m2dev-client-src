#include "Platform/PlatformTime.h"
#include <chrono>
#include <thread>

namespace Platform::Time
{
bool Initialize() noexcept { return std::chrono::steady_clock::is_steady; }

std::uint64_t MonotonicNanoseconds() noexcept
{
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

std::uint32_t TickMilliseconds() noexcept { return static_cast<std::uint32_t>(MonotonicNanoseconds() / 1000000); }
std::uint32_t UptimeMilliseconds() noexcept { return TickMilliseconds(); }
void SleepMilliseconds(std::uint32_t milliseconds) noexcept { std::this_thread::sleep_for(std::chrono::milliseconds{milliseconds}); }
bool BeginTimerPeriod() noexcept { return false; }
void EndTimerPeriod() noexcept {}
}
