#include "Platform/PlatformTime.h"

#include <windows.h>
#include <mmsystem.h>

#include <algorithm>
#include <mutex>

namespace Platform::Time
{
namespace
{
std::once_flag g_frequencyOnce;
LARGE_INTEGER g_frequency{};
}

bool Initialize() noexcept
{
    std::call_once(g_frequencyOnce, []
    {
        if (!QueryPerformanceFrequency(&g_frequency))
            g_frequency.QuadPart = 0;
    });
    return g_frequency.QuadPart > 0;
}

std::uint32_t TickMilliseconds() noexcept
{
    return timeGetTime();
}

std::uint32_t UptimeMilliseconds() noexcept
{
    return GetTickCount();
}

std::uint64_t MonotonicNanoseconds() noexcept
{
    if (!Initialize())
        return static_cast<std::uint64_t>(TickMilliseconds()) * 1'000'000ULL;

    LARGE_INTEGER counter{};
    if (!QueryPerformanceCounter(&counter) || counter.QuadPart < 0)
        return static_cast<std::uint64_t>(TickMilliseconds()) * 1'000'000ULL;

    const std::uint64_t ticks = static_cast<std::uint64_t>(counter.QuadPart);
    const std::uint64_t frequency = static_cast<std::uint64_t>(g_frequency.QuadPart);
    const std::uint64_t seconds = ticks / frequency;
    const std::uint64_t remainder = ticks % frequency;
    return seconds * 1'000'000'000ULL + remainder * 1'000'000'000ULL / frequency;
}

void SleepMilliseconds(std::uint32_t milliseconds) noexcept
{
    Sleep(milliseconds);
}

bool BeginTimerPeriod() noexcept
{
    TIMECAPS capabilities{};
    if (timeGetDevCaps(&capabilities, sizeof(capabilities)) != TIMERR_NOERROR)
        return false;

    const UINT resolution = std::clamp<UINT>(1, capabilities.wPeriodMin, capabilities.wPeriodMax);
    return timeBeginPeriod(resolution) == TIMERR_NOERROR;
}

void EndTimerPeriod() noexcept
{
    // Preserve the existing client shutdown contract paired with its requested 1 ms period.
    timeEndPeriod(1);
}
}
