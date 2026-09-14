#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <vector>

namespace AssetRuntime::AnimationStallAudit
{
// ZiiNAN: Optional, bounded stall capture. No file I/O in the measured frame.
using Clock = std::chrono::steady_clock;
using Digest = std::array<std::uint8_t, 32>;
inline bool enabled = false, fullCapture = false;
enum class Work { Fingerprint, Import, Pose, Palette, Submission, Presentation, PresentWait, Update, Sleep,
    GR2Read, Decompress, Container, Parse, AnimationDecode, ClipBind, Skeleton, Mesh, Material, GPUCreate, Prewarm, ReferencePose, GPUUpload, Count };
inline constexpr std::array<const char*,static_cast<std::size_t>(Work::Count)> workNames{
    "fingerprint", "import", "pose", "palette", "submission", "presentation", "present_wait", "update", "sleep",
    "gr2_read", "decompress", "container", "parse", "animation_decode", "clip_bind", "skeleton", "mesh", "material", "gpu_create", "prewarm", "reference_pose", "gpu_upload"};
struct Stats {
    std::uint64_t imports{}, fingerprints{}, hits{}, misses{}, unkeyed{}, instanceHits{}, poses{}, granny{}, cpu{}, fallbacks{};
    double importUs{}, maxImportUs{}, fingerprintUs{}, poseUs{}, maxPoseUs{}, paletteUs{};
    double submissionUs{}, presentationUs{}, presentWaitUs{}, updateUs{}, sleepUs{}, gpuPrepareUs{}, processUs{}, outsideUs{};
    std::array<std::uint64_t,workNames.size()> workCalls{};
    std::array<double,workNames.size()> workUs{};
};
struct Frame {
    Stats stats;
    std::uint64_t process{}, display{}, updates{};
    double elapsedUs{}, frameUs{};
    bool displayRow{}, presented{}, game{}, minimized{}, inactive{};
    int phase{};
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
inline constexpr std::size_t fullFrameLimit = 100000, diagnosticByteLimit = 4*1024*1024;
inline int capturePhase = 0;
inline bool explicitPhase = false;
inline std::vector<char> diagnostics;
inline std::uint64_t droppedDiagnosticBytes{};
inline std::mutex diagnosticMutex;
inline std::vector<Frame> frames;
inline std::vector<ContentEvent> events;
inline Stats processStats, displayStats, totals;
// Local-spawn-only breakdown, using the existing timers. Written at shutdown.
struct LocalPlayerSample {
    const char* stage{};
    double elapsedUs{}, wallUs{};
    std::array<double,workNames.size()> workUs{};
};
inline std::array<LocalPlayerSample,64> localPlayerSamples{};
inline std::size_t localPlayerSampleCount{};
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
class LocalPlayerScope {
    const char* stage;
    bool active;
    Clock::time_point start{};
    std::array<double,workNames.size()> before{};
public:
    LocalPlayerScope(const char* name,bool local) : stage(name),active(fullCapture && local) {
        if(active) { start=Clock::now(); before=totals.workUs; }
    }
    ~LocalPlayerScope() { Stop(); }
    void Stop() {
        if(!active) return;
        active=false;
        if(localPlayerSampleCount==localPlayerSamples.size()) return;
        auto& sample=localPlayerSamples[localPlayerSampleCount++];
        sample.stage=stage; sample.elapsedUs=Micros(origin,start); sample.wallUs=Micros(start,Clock::now());
        for(std::size_t i=0;i<before.size();++i) sample.workUs[i]=totals.workUs[i]-before[i];
    }
};
template<class F> inline void Record(F apply) noexcept
{
    if (!enabled) return;
    if (inProcess) apply(processStats);
    apply(displayStats); apply(totals);
}
inline void Enable(bool allFrames = false)
{
    fullCapture=allFrames;
    frames.reserve(fullCapture?fullFrameLimit:frameLimit); events.reserve(eventLimit);
    if(fullCapture) diagnostics.reserve(diagnosticByteLimit);
    origin = previousEnd = lastDisplay = Clock::now();
    enabled = true;
}
inline bool BufferDiagnostic(const char* text,std::size_t size)
{
    if(!fullCapture) return false;
    if(!text || !size) return true;
    std::lock_guard lock(diagnosticMutex);
    const auto count=std::min(size,diagnosticByteLimit-diagnostics.size());
    diagnostics.insert(diagnostics.end(),text,text+count); droppedDiagnosticBytes+=size-count;
    return true;
}
inline void ImportStarted() noexcept { Record([](Stats& s) { ++s.imports; }); }
inline void InstanceHit() noexcept { Record([](Stats& s) { ++s.instanceHits; }); }
inline void ReferencePose() noexcept { Record([](Stats& s) { ++s.granny; }); }
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
    explicit WorkScope(Work value,bool measure=true) noexcept : work(value) {
        active=active && measure;
        if (!active) return;
        start = Clock::now();
        if (work == Work::Pose) Record([](Stats& s) { ++s.poses; });
        if (work == Work::Fingerprint) Record([](Stats& s) { ++s.fingerprints; });
        if(fullCapture) Record([&](Stats& s){++s.workCalls[static_cast<std::size_t>(work)];});
    }
    ~WorkScope() { Stop(); }
    void Stop() noexcept {
        if (!active) return;
        active = false;
        const double us = Micros(start, Clock::now());
        Record([&](Stats& s) {
            if(fullCapture) s.workUs[static_cast<std::size_t>(work)]+=us;
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
            default: break;
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
    if(fullCapture) { if(!row.displayRow) return; }
    else if (row.frameUs <= 20000) return;
    if (frames.size() == (fullCapture?fullFrameLimit:frameLimit)) { ++droppedFrames; return; }
    row.phase=capturePhase;
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
    std::ofstream csv(fullCapture?"load-warmup-frames.csv":"animation-stalls.csv"); csv << std::setprecision(12);
    csv << "kind,process,display,updates,elapsed_ms,frame_ms,presented,game,minimized,inactive,imports,import_ms,max_import_ms,fingerprints,fingerprint_ms,content_hits,content_misses,unkeyed_misses,instance_hits,ziinan_poses,pose_ms,max_pose_ms,palette_ms,submission_ms,presentation_ms,present_wait_ms,update_ms,sleep_ms,gpu_prepare_ms,process_ms,outside_process_ms,reference_poses,cpu_deforms,gpu_fallbacks";
    if(fullCapture) { csv<<",phase"; for(auto name:workNames) csv<<','<<name<<"_calls,"<<name<<"_work_ms"; }
    csv<<'\n';
    for (const auto& row : frames) {
        const auto& s = row.stats;
        csv << (row.displayRow ? "display" : "process") << ',' << row.process << ',' << row.display << ',' << row.updates << ',' << row.elapsedUs/1000 << ',' << row.frameUs/1000 << ',' << row.presented << ',' << row.game << ',' << row.minimized << ',' << row.inactive << ','
            << s.imports << ',' << s.importUs/1000 << ',' << s.maxImportUs/1000 << ',' << s.fingerprints << ',' << s.fingerprintUs/1000 << ',' << s.hits << ',' << s.misses << ',' << s.unkeyed << ',' << s.instanceHits << ','
            << s.poses << ',' << s.poseUs/1000 << ',' << s.maxPoseUs/1000 << ',' << s.paletteUs/1000 << ',' << s.submissionUs/1000 << ',' << s.presentationUs/1000 << ',' << s.presentWaitUs/1000 << ',' << s.updateUs/1000 << ',' << s.sleepUs/1000 << ','
            << s.gpuPrepareUs/1000 << ',' << s.processUs/1000 << ',' << s.outsideUs/1000 << ',' << s.granny << ',' << s.cpu << ',' << s.fallbacks;
        if(fullCapture) { csv<<','<<row.phase; for(std::size_t i=0;i<workNames.size();++i) csv<<','<<s.workCalls[i]<<','<<s.workUs[i]/1000; }
        csv<<'\n';
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
    meta << "ThresholdMs="<<(fullCapture?0:20)<<" Processes=" << processSerial << " PresentedFrames=" << displaySerial
        << " DroppedFrames=" << droppedFrames << " DroppedContentEvents=" << droppedEvents
        << " Imports=" << totals.imports << " ImportMs=" << totals.importUs/1000 << " MaxImportMs=" << totals.maxImportUs/1000
        << " ZiiNANPoses=" << totals.poses << " PoseMs=" << totals.poseUs/1000 << " MaxSinglePoseMs=" << totals.maxPoseUs/1000
        << " MaxGamePoseFrameMs=" << peakGamePoseFrameUs/1000 << " MaxGameSinglePoseMs=" << peakGameSinglePoseUs/1000
        << " MaxGameDisplayMs=" << peakGameDisplayUs/1000 << " MaxDisplayMs=" << peakDisplayUs/1000 << '\n';
    if(fullCapture) {
        meta<<"FullFrameCapture=1 FrameCapacity="<<fullFrameLimit<<" FrameStorageBytes="<<frames.capacity()*sizeof(Frame)
            <<" BufferedDiagnosticBytes="<<diagnostics.size()<<" DroppedDiagnosticBytes="<<droppedDiagnosticBytes<<'\n';
        for(std::size_t i=0;i<workNames.size();++i) meta<<workNames[i]<<" calls="<<totals.workCalls[i]<<" ms="<<totals.workUs[i]/1000<<'\n';
        std::ofstream messages("load-warmup-diagnostics.log",std::ios::binary);
        messages.write(diagnostics.data(),static_cast<std::streamsize>(diagnostics.size()));
        std::ofstream local("local-player-spawn.csv");
        local<<"stage,elapsed_ms,wall_ms";
        for(const auto* name:workNames) local<<','<<name<<"_ms";
        local<<'\n'<<std::fixed<<std::setprecision(3);
        for(std::size_t row=0;row<localPlayerSampleCount;++row) {
            const auto& sample=localPlayerSamples[row];
            local<<sample.stage<<','<<sample.elapsedUs/1000<<','<<sample.wallUs/1000;
            for(const auto value:sample.workUs) local<<','<<value/1000;
            local<<'\n';
        }
    }
    meta << "CPU wall timings; nested scopes are not additive. Pose includes hierarchy and palette; palette is only BuildPalette math, not renderer capture/upload. Full capture times reference_pose separately; legacy capture retains reference counts only. GPU prepare includes existing preparation/upload work, not GPU execution. Presentation includes Present wait and renderer diagnostic tail. Display rows aggregate updates between actual swapchain presentations; legacy process rows overlap display rows and must not be summed with them. The audit writes only after capture. Full capture buffers game diagnostic sinks and disables verbose renderer file logs; legacy capture leaves them unchanged.\n";
}
}
