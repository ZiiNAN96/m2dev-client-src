#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <vector>

namespace AssetRuntime::AnimationStallAudit
{
// ZiiNAN: Optional, bounded stall capture. No file I/O in the measured frame.
using Clock = std::chrono::steady_clock;
using Digest = std::array<std::uint8_t, 32>;
inline bool enabled = false;
enum class Work { Fingerprint, Import, Pose, Palette, Submission, Presentation, PresentWait, Update, Sleep };
struct Stats {
    std::uint64_t imports{}, fingerprints{}, hits{}, misses{}, unkeyed{}, instanceHits{}, poses{}, granny{}, cpu{}, fallbacks{};
    double importUs{}, maxImportUs{}, fingerprintUs{}, poseUs{}, maxPoseUs{}, paletteUs{};
    double submissionUs{}, presentationUs{}, presentWaitUs{}, updateUs{}, sleepUs{}, gpuPrepareUs{}, processUs{}, outsideUs{};
};
struct Frame {
    Stats stats;
    std::uint64_t process{}, display{}, updates{};
    double elapsedUs{}, frameUs{};
    bool displayRow{}, presented{}, game{}, minimized{}, inactive{};
};
struct ContentEvent {
    Digest model{}, animation{};
    std::uint64_t process{}, display{}, binding{};
    std::size_t modelIndex{}, animationIndex{};
    double elapsedUs{};
    char kind{}; // H=content hit, M=content miss, U=missing digest, I=actual import.
    bool keyed{};
};
inline constexpr std::size_t frameLimit = 20000, eventLimit = 100000;
inline std::vector<Frame> frames;
inline std::vector<ContentEvent> events;
inline Stats processStats, displayStats, totals;
inline Clock::time_point origin{}, processStart{}, accountedUntil{}, previousEnd{}, lastDisplay{};
inline std::uint64_t processSerial{}, displaySerial{}, displayUpdates{}, droppedFrames{}, droppedEvents{};
inline std::uint64_t swapchainPresents{};
inline std::uint64_t previousCpu{}, previousFallbacks{};
inline double previousGpuPrepare{};
inline bool inProcess{}, processPresented{}, processGame{}, processMinimized{}, displayGame{}, displayMinimized{};
inline bool processInactive{}, displayInactive{};
inline double peakDisplayUs{}, peakGameDisplayUs{}, peakGamePoseFrameUs{}, peakGameSinglePoseUs{};

inline double Micros(Clock::time_point from, Clock::time_point to) noexcept
{ return std::chrono::duration<double, std::micro>(to - from).count(); }
template<class F> inline void Record(F apply) noexcept
{
    if (!enabled) return;
    if (inProcess) apply(processStats);
    apply(displayStats); apply(totals);
}
inline void Enable()
{
    frames.reserve(frameLimit); events.reserve(eventLimit);
    origin = previousEnd = lastDisplay = Clock::now();
    enabled = true;
}
inline void ImportStarted() noexcept { Record([](Stats& s) { ++s.imports; }); }
inline void InstanceHit() noexcept { Record([](Stats& s) { ++s.instanceHits; }); }
inline void GrannyPose() noexcept { Record([](Stats& s) { ++s.granny; }); }
inline void Content(char kind, const Digest* model, const Digest* animation,
    std::size_t modelIndex, std::size_t animationIndex, std::uint64_t binding) noexcept
{
    if (!enabled) return;
    if (events.size() == eventLimit) { ++droppedEvents; return; }
    ContentEvent event;
    event.kind = kind; event.keyed = model && animation;
    if (event.keyed) { event.model = *model; event.animation = *animation; }
    event.modelIndex = modelIndex; event.animationIndex = animationIndex; event.binding = binding;
    event.process = inProcess ? processSerial : 0; event.display = displaySerial + 1;
    event.elapsedUs = Micros(origin, Clock::now());
    events.push_back(event);
}
inline void CacheLookup(const Digest* model, const Digest* animation, std::size_t modelIndex,
    std::size_t animationIndex, std::uint64_t binding, bool hit) noexcept
{
    Record([&](Stats& s) { if (hit) ++s.hits; else ++s.misses; if (!model || !animation) ++s.unkeyed; });
    Content(hit ? 'H' : (model && animation ? 'M' : 'U'), model, animation, modelIndex, animationIndex, binding);
}
inline void ImportKey(const Digest* model, const Digest* animation, std::size_t modelIndex,
    std::size_t animationIndex, std::uint64_t binding) noexcept
{ Content('I', model, animation, modelIndex, animationIndex, binding); }
class WorkScope {
    Work work;
    bool active = enabled;
    Clock::time_point start{};
public:
    explicit WorkScope(Work value) noexcept : work(value) {
        if (!active) return;
        start = Clock::now();
        if (work == Work::Pose) Record([](Stats& s) { ++s.poses; });
        if (work == Work::Fingerprint) Record([](Stats& s) { ++s.fingerprints; });
    }
    ~WorkScope() { Stop(); }
    void Stop() noexcept {
        if (!active) return;
        active = false;
        const double us = Micros(start, Clock::now());
        Record([&](Stats& s) {
            switch (work) {
            case Work::Fingerprint: s.fingerprintUs += us; break;
            case Work::Import: s.importUs += us; s.maxImportUs = std::max(s.maxImportUs, us); break;
            case Work::Pose: s.poseUs += us; s.maxPoseUs = std::max(s.maxPoseUs, us); break;
            case Work::Palette: s.paletteUs += us; break;
            case Work::Submission: s.submissionUs += us; break;
            case Work::Presentation: s.presentationUs += us; break;
            case Work::PresentWait: s.presentWaitUs += us; break;
            case Work::Update: s.updateUs += us; break;
            case Work::Sleep: s.sleepUs += us; break;
            }
        });
    }
};
inline void External(std::uint64_t cpu, std::uint64_t fallbacks, double gpuPrepare) noexcept
{
    if (!enabled) return;
    Record([&](Stats& s) { s.cpu += cpu - previousCpu; s.fallbacks += fallbacks - previousFallbacks;
        s.gpuPrepareUs += gpuPrepare - previousGpuPrepare; });
    previousCpu = cpu; previousFallbacks = fallbacks; previousGpuPrepare = gpuPrepare;
}
inline void Save(Frame row) noexcept
{
    if (row.frameUs <= 20000) return;
    if (frames.size() == frameLimit) { ++droppedFrames; return; }
    frames.push_back(row);
}
inline void BeginProcess(bool game, bool minimized, bool active = true) noexcept
{
    if (!enabled) return;
    processStart = accountedUntil = Clock::now();
    displayStats.outsideUs += Micros(previousEnd, processStart);
    processStats = {}; inProcess = true; processPresented = false;
    processGame = game; processMinimized = minimized;
    processInactive = !active; displayInactive = displayInactive || !active;
    displayGame = displayGame || game; displayMinimized = displayMinimized || minimized;
    ++processSerial; ++displayUpdates;
}
inline void Presented() noexcept
{
    if (!enabled || !inProcess) return;
    const auto now = Clock::now();
    displayStats.processUs += Micros(accountedUntil, now);
    const double wall = Micros(lastDisplay, now);
    ++displaySerial;
    peakDisplayUs = std::max(peakDisplayUs, wall);
    if (displayGame && !displayMinimized && !displayInactive) {
        peakGameDisplayUs = std::max(peakGameDisplayUs, wall);
        peakGamePoseFrameUs = std::max(peakGamePoseFrameUs, displayStats.poseUs);
        peakGameSinglePoseUs = std::max(peakGameSinglePoseUs, displayStats.maxPoseUs);
    }
    Save({displayStats, processSerial, displaySerial, displayUpdates, Micros(origin, now), wall,
        true, true, displayGame, displayMinimized, displayInactive});
    displayStats = {}; displayUpdates = 0; displayGame = displayMinimized = false;
    displayInactive = false;
    lastDisplay = accountedUntil = now; processPresented = true;
}
inline void EndProcess() noexcept
{
    if (!enabled || !inProcess) return;
    const auto now = Clock::now();
    processStats.processUs = Micros(processStart, now);
    displayStats.processUs += Micros(accountedUntil, now);
    Save({processStats, processSerial, displaySerial + (processPresented ? 0 : 1), 1,
        Micros(origin, now), processStats.processUs, false, processPresented, processGame, processMinimized, processInactive});
    previousEnd = now; inProcess = false;
}
inline void Write()
{
    if (!enabled) return;
    std::ofstream csv("animation-stalls.csv"); csv << std::setprecision(12);
    csv << "kind,process,display,updates,elapsed_ms,frame_ms,presented,game,minimized,inactive,imports,import_ms,max_import_ms,fingerprints,fingerprint_ms,content_hits,content_misses,unkeyed_misses,instance_hits,ziinan_poses,pose_ms,max_pose_ms,palette_ms,submission_ms,presentation_ms,present_wait_ms,update_ms,sleep_ms,gpu_prepare_ms,process_ms,outside_process_ms,reference_poses,cpu_deforms,gpu_fallbacks\n";
    for (const auto& row : frames) {
        const auto& s = row.stats;
        csv << (row.displayRow ? "display" : "process") << ',' << row.process << ',' << row.display << ',' << row.updates << ',' << row.elapsedUs/1000 << ',' << row.frameUs/1000 << ',' << row.presented << ',' << row.game << ',' << row.minimized << ',' << row.inactive << ','
            << s.imports << ',' << s.importUs/1000 << ',' << s.maxImportUs/1000 << ',' << s.fingerprints << ',' << s.fingerprintUs/1000 << ',' << s.hits << ',' << s.misses << ',' << s.unkeyed << ',' << s.instanceHits << ','
            << s.poses << ',' << s.poseUs/1000 << ',' << s.maxPoseUs/1000 << ',' << s.paletteUs/1000 << ',' << s.submissionUs/1000 << ',' << s.presentationUs/1000 << ',' << s.presentWaitUs/1000 << ',' << s.updateUs/1000 << ',' << s.sleepUs/1000 << ','
            << s.gpuPrepareUs/1000 << ',' << s.processUs/1000 << ',' << s.outsideUs/1000 << ',' << s.granny << ',' << s.cpu << ',' << s.fallbacks << '\n';
    }
    std::ofstream content("animation-stall-content.csv"); content << std::setprecision(12);
    content << "kind,process,display,elapsed_ms,keyed,model_sha256,animation_sha256,model_index,animation_index,binding\n";
    for (const auto& e : events) {
        content << e.kind << ',' << e.process << ',' << e.display << ',' << e.elapsedUs/1000 << ',' << e.keyed << ',';
        if (e.keyed) for (auto byte : e.model) content << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
        content << ',';
        if (e.keyed) for (auto byte : e.animation) content << std::hex << std::setw(2) << std::setfill('0') << unsigned(byte);
        content << std::dec << ',' << e.modelIndex << ',' << e.animationIndex << ',' << e.binding << '\n';
    }
    std::ofstream meta("animation-stall-summary.txt");
    meta << "ThresholdMs=20 Processes=" << processSerial << " PresentedFrames=" << displaySerial
        << " DroppedFrames=" << droppedFrames << " DroppedContentEvents=" << droppedEvents
        << " Imports=" << totals.imports << " ImportMs=" << totals.importUs/1000 << " MaxImportMs=" << totals.maxImportUs/1000
        << " ZiiNANPoses=" << totals.poses << " PoseMs=" << totals.poseUs/1000 << " MaxSinglePoseMs=" << totals.maxPoseUs/1000
        << " MaxGamePoseFrameMs=" << peakGamePoseFrameUs/1000 << " MaxGameSinglePoseMs=" << peakGameSinglePoseUs/1000
        << " MaxGameDisplayMs=" << peakGameDisplayUs/1000 << " MaxDisplayMs=" << peakDisplayUs/1000 << '\n';
    meta << "CPU wall timings; nested scopes are not additive. Pose includes hierarchy and palette; palette is only BuildPalette math, not renderer capture/upload. Reference poses are counts without a reference-time measurement. GPU prepare includes existing preparation/upload work, not GPU execution. Presentation includes Present wait and renderer diagnostic tail. Display rows aggregate updates between actual swapchain presentations; process rows can overlap display rows and must not be summed with them. This audit performs no file writes during capture; existing diagnostic logs are unchanged.\n";
}
}
