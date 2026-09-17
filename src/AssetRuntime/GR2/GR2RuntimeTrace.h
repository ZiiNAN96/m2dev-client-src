#pragma once
#include "EterBase/MapLoadTrace.h"
#include <array>
#include <algorithm>

// Opt-in P0-L8 diagnostics. No persistent cache and no per-key trace records.
// Fine timers deliberately run only in a separate diagnostic capture.
namespace AssetRuntime::GR2::RuntimeTrace {
inline bool Enabled() {
    // Elevated Windows launches do not reliably inherit the probe environment.
    // The explicit marker belongs only to the private diagnostic testclient.
    static const bool enabled=[] { return std::ifstream("animation-preparation-trace.enabled").good(); }();
    return enabled && MapLoadTrace::state.active;
}
inline thread_local unsigned prewarmDepth{};
struct PrewarmScope {
    MapLoadTrace::Scope wall{"Actors","Production animation prewarm"};
    PrewarmScope() { ++prewarmDepth; }
    ~PrewarmScope() { --prewarmDepth; }
};
enum Stage { Mapping, Timeline, Allocation, Sampling, Refinement, Store, Validation, TrackStorage, StageCount };
inline constexpr std::array<const char*,StageCount> names{
    "track binding","timeline temporary buffer","key allocation relocation","source curve evaluation",
    "refinement error checks","key count and store","clip validation binding","track storage"};
struct Stats {
    std::string id;
    std::array<double,StageCount> ms{};
    std::array<std::uint64_t,StageCount> calls{};
    std::uint64_t sourceTracks{},sourceAllKeys{},tracks{},sourceKeys{},runtimeKeys{},allocations{},reallocations{},allocatedBytes{},relocatedBytes{};
    std::uint64_t reusedSamples{};
    std::uint64_t temporaryAllocated{},temporaryPeak{},temporaryLive{},runtimeBytes{},sampleCalls{},splitNodes{},leafNodes{},maxDepth{};
    std::uint64_t identityChannels{},constantChannels{},staticTracks{},potentialMidpointReuse{},constantControlChannels{};
    std::array<std::uint64_t,3> sourceByChannel{},runtimeByChannel{},constantByChannel{};
};
inline thread_local Stats* current{};
struct Timer {
    Stats* stats=current; Stage stage; MapLoadTrace::Clock::time_point start{};
    explicit Timer(Stage value):stage(value) { if(stats) start=MapLoadTrace::Clock::now(); }
    ~Timer() { if(stats) { stats->ms[stage]+=MapLoadTrace::Ms(MapLoadTrace::Clock::now()-start); ++stats->calls[stage]; } }
};
struct ClipScope {
    Stats stats; Stats* previous=current; bool active=Enabled();
    MapLoadTrace::Clock::time_point start{};
    ClipScope(std::string_view name,std::uint64_t skeleton,unsigned boundary) {
        if(active) {
            start=MapLoadTrace::Clock::now();
            stats.id=MapLoadTrace::state.gr2Path+"|"+std::string(name)+"|"+std::to_string(skeleton)+"|"+std::to_string(boundary);
            current=&stats;
            MapLoadTrace::Count("runtime-preparation",stats.id,prewarmDepth?1:0);
        }
    }
    ~ClipScope() {
        current=previous;
        if(!active) return;
        auto count=[&](const char* kind,std::uint64_t value) { MapLoadTrace::Count(kind,stats.id,value); };
        count("runtime-total-ns",static_cast<std::uint64_t>(MapLoadTrace::Ms(MapLoadTrace::Clock::now()-start)*1e6));
        count("runtime-source-tracks",stats.sourceTracks); count("runtime-tracks",stats.tracks);
        count("runtime-all-source-keys",stats.sourceAllKeys); count("runtime-reused-samples",stats.reusedSamples);
        count("runtime-source-keys",stats.sourceKeys); count("runtime-keys",stats.runtimeKeys);
        count("runtime-allocations",stats.allocations); count("runtime-reallocations",stats.reallocations);
        count("runtime-allocated-bytes",stats.allocatedBytes); count("runtime-relocated-bytes",stats.relocatedBytes);
        count("runtime-temporary-allocated",stats.temporaryAllocated); count("runtime-temporary-peak",stats.temporaryPeak);
        count("runtime-owned-bytes",stats.runtimeBytes); count("runtime-sample-calls",stats.sampleCalls);
        count("runtime-split-nodes",stats.splitNodes); count("runtime-leaf-nodes",stats.leafNodes); count("runtime-max-depth",stats.maxDepth);
        count("runtime-identity-channels",stats.identityChannels); count("runtime-constant-channels",stats.constantChannels);
        count("runtime-constant-control-channels",stats.constantControlChannels); count("runtime-static-tracks",stats.staticTracks);
        count("runtime-midpoint-reuse",stats.potentialMidpointReuse);
        for(unsigned c=0;c<3;++c) {
            MapLoadTrace::Count("runtime-source-channel-"+std::to_string(c),stats.id,stats.sourceByChannel[c]);
            MapLoadTrace::Count("runtime-key-channel-"+std::to_string(c),stats.id,stats.runtimeByChannel[c]);
            MapLoadTrace::Count("runtime-constant-channel-"+std::to_string(c),stats.id,stats.constantByChannel[c]);
        }
        for(unsigned i=0;i<StageCount;++i) {
            MapLoadTrace::Count("runtime-stage-ns-"+std::string(names[i]),stats.id,static_cast<std::uint64_t>(stats.ms[i]*1e6));
            MapLoadTrace::Count("runtime-stage-calls-"+std::string(names[i]),stats.id,stats.calls[i]);
        }
    }
};
}
