#pragma once
#include "GraphicsSettings.h"

namespace Graphics
{
inline constexpr std::uint64_t NanosecondsPerSecond = 1'000'000'000;
inline constexpr std::uint64_t NanosecondsPerMillisecond = 1'000'000;

// Absolute deadlines avoid accumulating the sleep API's overshoot. No catch-up
// render burst after a missed deadline, and no wait at all in Unlimited mode.
class FramePacer
{
public:
    void Begin(std::uint64_t now, FrameRateLimit limit)
    {
        const auto rate = FrameRate(limit);
        const auto period = rate ? NanosecondsPerSecond / rate : 0;
        if (period != period_ || !deadline_) { period_ = period; deadline_ = now + period; }
    }
    std::uint64_t Deadline() const { return period_ ? deadline_ : 0; }
    void End(std::uint64_t now)
    {
        if (!period_) { deadline_ = 0; return; }
        deadline_ += period_;
        if (deadline_ <= now) deadline_ = now + period_;
    }
private:
    std::uint64_t period_{}, deadline_{};
};

// Retain the legacy alternating 17/16 ms simulation steps independently of
// presentation. Existing per-update camera/UI/particle increments keep their
// authored speed. This does not claim render interpolation between game ticks.
class SimulationClock
{
public:
    struct Batch { unsigned steps{}, skippedMilliseconds{}; };
    Batch Poll(std::uint64_t now)
    {
        if (!started_) { started_ = true; next_ = now; }
        Batch batch;
        constexpr auto pair = 33 * NanosecondsPerMillisecond;
        // Match the old 500 ms stall escape, retaining step parity. Bounded
        // catch-up prevents a stalled/minimized client from spiralling forever.
        if (now > next_ && now - next_ >= 500 * NanosecondsPerMillisecond)
        {
            const auto skipped = (now - next_) / pair * pair;
            next_ += skipped;
            batch.skippedMilliseconds = static_cast<unsigned>(skipped / NanosecondsPerMillisecond);
        }
        while (now >= next_ && batch.steps < MaxCatchUpSteps)
        {
            ++batch.steps;
            next_ += (longStep_ ? 17 : 16) * NanosecondsPerMillisecond;
            longStep_ = !longStep_;
        }
        return batch;
    }
    static constexpr unsigned MaxCatchUpSteps = 8;
private:
    std::uint64_t next_{};
    bool started_{}, longStep_{true};
};
}
