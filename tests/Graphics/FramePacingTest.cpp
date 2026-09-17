#include "Graphics/FramePacing.h"
#include "Graphics/GraphicsSettingsFile.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace Graphics;
void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
int main()
{
    try
    {
        const auto old = LoadGraphicsSettings("VERSION 1\nSHADOWS 4\n# retain\nEXTRA yes\n");
        Check(old.settings.frameRateLimit == FrameRateLimit::FPS60 && old.settings.vsync == VSync::On, "old defaults");
        const auto bad = LoadGraphicsSettings("FRAME_RATE_LIMIT 3\nVSYNC -1\n");
        Check(bad.invalidValues == 2 && bad.settings.frameRateLimit == FrameRateLimit::FPS60 && bad.settings.vsync == VSync::On, "bad values");
        Store store;
        store.ApplyGraphicsPreset(GraphicsPreset::High);
        store.ConsumeChanges();
        for (auto limit : {FrameRateLimit::FPS60, FrameRateLimit::FPS120, FrameRateLimit::Unlimited})
            for (auto vsync : {VSync::Off, VSync::On})
            {
                auto s = store.GetGraphicsSettings(); s.frameRateLimit = limit; s.vsync = vsync;
                const auto saved = SaveGraphicsSettings(s, "# retain\nEXTRA yes\n");
                Check(LoadGraphicsSettings(saved).settings == s && saved.find("EXTRA yes") != saved.npos, "six combinations roundtrip");
                std::string error;
                Check(SaveGraphicsSettingsFile("pacing-test.cfg", s, error), "atomic save");
                Check(LoadGraphicsSettingsFile("pacing-test.cfg", {}, error).settings == s, "disk reload");
                Check(store.ApplyGraphicsSettings(s), "live apply");
                auto event = store.ConsumeChanges();
                Check(event.fields == (RendererChanged | FramePacingChanged), "only pacing notification");
                Check(event.category == ApplyCategory::Live && event.runtime.frameRateLimit == limit && event.runtime.vsync == vsync, "live runtime independent values");
                Check(store.GetGraphicsSettings().preset == GraphicsPreset::High, "pacing leaves quality preset");
                store.ApplyGraphicsPreset(GraphicsPreset::Low);
                Check(store.GetGraphicsSettings().frameRateLimit == limit && store.GetGraphicsSettings().vsync == vsync, "quality leaves pacing");
                store.ApplyGraphicsPreset(GraphicsPreset::High); store.ConsumeChanges();
            }
        std::filesystem::remove("pacing-test.cfg");
        FramePacer pacer;
        constexpr std::uint64_t start = 1'000'000'000;
        pacer.Begin(start, FrameRateLimit::FPS60);
        Check(pacer.Deadline() == start + NanosecondsPerSecond / 60, "60 deadline");
        const auto deadline = pacer.Deadline(); pacer.End(deadline + 100'000);
        Check(pacer.Deadline() == deadline + NanosecondsPerSecond / 60, "overshoot does not accumulate");
        pacer.Begin(start, FrameRateLimit::FPS120);
        Check(pacer.Deadline() == start + NanosecondsPerSecond / 120, "live 120 reset");
        pacer.End(start + NanosecondsPerSecond);
        Check(pacer.Deadline() > start + NanosecondsPerSecond, "no late catch-up burst");
        pacer.Begin(start, FrameRateLimit::Unlimited);
        Check(!pacer.Deadline(), "unlimited never waits");
        for (unsigned renderRate : {30, 60, 120, 165, 500, 1000})
        {
            SimulationClock clock;
            unsigned ticks = 0;
            for (unsigned frame = 0; frame <= renderRate * 10; ++frame)
            {
                const auto batch = clock.Poll(start + std::uint64_t(frame) * NanosecondsPerSecond / renderRate);
                Check(!batch.skippedMilliseconds && batch.steps <= SimulationClock::MaxCatchUpSteps, "ordinary ticks are not skipped");
                ticks += batch.steps;
            }
            Check(ticks == 607, "ten seconds have identical simulation count at every render rate");
        }
        SimulationClock stalled;
        stalled.Poll(start);
        const auto recovered = stalled.Poll(start + 2 * NanosecondsPerSecond);
        Check(recovered.skippedMilliseconds > 1900 && recovered.steps <= 2, "bounded long stall recovery");
        std::cout << "PASS six settings combinations, persistence, live apply, deadlines, simulation invariance, stall recovery\n";
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
