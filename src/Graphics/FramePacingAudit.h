#pragma once
#include <cstdint>
#include <fstream>
#include <vector>

namespace Graphics::FramePacingAudit
{
// Explicit, bounded diagnostic capture. No file I/O during measured frames;
// ordinary sessions allocate nothing and take no extra Present timestamps.
struct Sample
{
    std::uint64_t start{}, presentStart{}, presentEnd{}, wait{};
    unsigned gameMilliseconds{}, steps{}, skippedMilliseconds{}, limit{}, vsync{};
};
class Capture
{
public:
    void Enable()
    {
        samples.reserve(MaxSamples);
        enabled = true;
    }
    ~Capture()
    {
        if (!enabled) return;
        std::ofstream out("frame-pacing.csv");
        out << "start_ns,present_start_ns,present_end_ns,wait_ns,game_ms,steps,skipped_ms,limit,vsync\n";
        for (const auto& s : samples)
            out << s.start << ',' << s.presentStart << ',' << s.presentEnd << ',' << s.wait << ','
                << s.gameMilliseconds << ',' << s.steps << ',' << s.skippedMilliseconds << ','
                << s.limit << ',' << s.vsync << '\n';
        std::ofstream("frame-pacing-meta.txt") << "samples=" << samples.size() << " dropped=" << dropped << '\n';
    }
    void Finish()
    {
        if (samples.size() < MaxSamples) samples.push_back(current); else ++dropped;
    }
    bool enabled{};
    Sample current;
private:
    static constexpr unsigned MaxSamples = 400'000;
    std::vector<Sample> samples;
    unsigned dropped{};
};
inline Capture capture;
}
