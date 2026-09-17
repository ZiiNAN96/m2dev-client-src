#include "GR2Preparation.h"
#include "GR2AssetProvider.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include "EterBase/MapLoadTrace.h"
#include <algorithm>
#include <atomic>
#include <thread>

namespace AssetRuntime::GR2 {
namespace {
std::string Key(std::string_view id) {
    std::string key(id);
    for(auto& c:key) { if(c=='\\') c='/'; if(c>='A' && c<='Z') c+='a'-'A'; }
    return key;
}
thread_local Preparation* current{};
struct TraceState {
    MapLoadTrace::State saved=std::move(MapLoadTrace::state);
    explicit TraceState(bool active) { MapLoadTrace::state={}; MapLoadTrace::state.active=active; }
    ~TraceState() { MapLoadTrace::state=std::move(saved); }
};
}
struct Preparation::Impl {
    struct Item {
        std::string id;
        std::vector<std::byte> bytes;
        std::unique_ptr<Result> result;
        MapLoadTrace::State trace;
        MapLoadTrace::Clock::time_point start{}, end{};
        std::uint64_t cpuNs{};
    };
    Preparation* previous{};
    bool enabled{}, ran{}, tracing{}, animationBatch{};
    std::size_t bytes{}, joins{};
    std::vector<Item> items;
    std::map<std::string,std::size_t> index;
};
Preparation::Preparation(bool animationBatch) : impl_(std::make_unique<Impl>()) {
    impl_->animationBatch=animationBatch;
    impl_->previous=current;
    // The optional animation audit is owner-thread state. Keep its established
    // serial path instead of accessing its counters from worker threads.
    impl_->enabled=!current && !AnimationStallAudit::enabled;
    impl_->tracing=MapLoadTrace::state.active;
    current=this;
}
Preparation::~Preparation() { current=impl_->previous; }
bool Preparation::Enabled() const { return impl_->enabled; }
void Preparation::Add(std::string id,const Loader& load) {
    if(!impl_->enabled || impl_->ran) return;
    id=Key(id);
    if(!id.ends_with(".gr2")) return;
    if(impl_->index.contains(id)) { ++impl_->joins; return; }
    // Bound retained inputs and task/results count as well as concurrency.
    if(impl_->items.size()>=32 || impl_->bytes>=16u*1024u*1024u) return;
    MapLoadTrace::GR2Context context(id);
    auto bytes=load();
    if(bytes.empty()) return;
    impl_->bytes+=bytes.size();
    impl_->index.emplace(id,impl_->items.size());
    impl_->items.push_back({std::move(id),std::move(bytes)});
}
void Preparation::Run(unsigned workers,const Executor& execute,const std::function<std::uint64_t()>& cpuClock) {
    if(!impl_->enabled || impl_->ran) return;
    impl_->ran=true;
    if(impl_->items.empty()) return;
    const auto hardware=std::thread::hardware_concurrency();
    workers=std::min({workers,4u,hardware>2?hardware-2:1u,static_cast<unsigned>(impl_->items.size())});
    workers=std::max(workers,1u);
    MapLoadTrace::Scope wall("Assets","GR2 parallel preparation wall");
    const auto start=MapLoadTrace::Clock::now();
    std::atomic_size_t next{}, active{}, peak{};
    auto work=[&] {
        for(;;) {
            const auto index=next.fetch_add(1);
            if(index>=impl_->items.size()) return;
            auto& item=impl_->items[index];
            auto result=std::make_unique<Result>();
            // A pool worker's trace is private. Restore it even on exceptions.
            TraceState trace(impl_->tracing);
            item.start=MapLoadTrace::Clock::now();
            const auto cpuStart=impl_->tracing && cpuClock?cpuClock():0;
            const auto count=active.fetch_add(1)+1;
            auto old=peak.load(); while(old<count && !peak.compare_exchange_weak(old,count)) {}
            {
                MapLoadTrace::GR2Context context(item.id);
                MapLoadTrace::Scope scope("Assets","GR2 parse");
                MapLoadTrace::Count("gr2-parse",item.id,item.bytes.size(),true);
                ++nativeFileReads;
                try {
                    MapLoadTrace::Scope container("Assets","GR2 container");
                    File file(item.bytes); container.Stop();
                    result->contents=Read(file);
                } catch(...) { result->error=std::current_exception(); }
            }
            item.end=MapLoadTrace::Clock::now();
            item.cpuNs=impl_->tracing && cpuClock?cpuClock()-cpuStart:0;
            active.fetch_sub(1);
            item.trace=std::move(MapLoadTrace::state);
            item.result=std::move(result);
        }
    };
    std::vector<std::future<void>> futures;
    std::exception_ptr failure;
    try {
        futures.reserve(workers);
        for(unsigned i=0;i<workers;++i) futures.push_back(execute(work));
    } catch(...) { failure=std::current_exception(); }
    // Always join every submitted job before any captured state can die.
    for(auto& future:futures) {
        try { future.get(); } catch(...) { if(!failure) failure=std::current_exception(); }
    }
    if(failure) std::rethrow_exception(failure);
    // Trace allocation must not throw while futures still reference this scope.
    for(std::size_t i=0;i<futures.size();++i) MapLoadTrace::Count("gr2-completion-waits");
    const auto elapsed=MapLoadTrace::Ms(MapLoadTrace::Clock::now()-start);
    if(impl_->tracing) {
        double busy=0;
        double workerCost=0,decodeCpu=0,parseCpu=0;
        std::uint64_t cpuNs=0;
        std::vector<std::pair<MapLoadTrace::Clock::time_point,MapLoadTrace::Clock::time_point>> intervals;
        decltype(intervals) parseIntervals;
        for(auto& item:impl_->items) {
            busy+=MapLoadTrace::Ms(item.end-item.start);
            cpuNs+=item.cpuNs;
            intervals.insert(intervals.end(),item.trace.gr2DecodeIntervals.begin(),item.trace.gr2DecodeIntervals.end());
            parseIntervals.insert(parseIntervals.end(),item.trace.animationParseIntervals.begin(),item.trace.animationParseIntervals.end());
            for(const auto& [key,cost]:item.trace.costs) workerCost+=cost.exclusive;
            const auto decode=item.trace.costs.find("Assets\tcpu\tGR2 section decompression");
            if(decode!=item.trace.costs.end()) decodeCpu+=decode->second.inclusive;
            const auto parse=item.trace.costs.find("Actors\tcpu\tGR2 animation curves");
            if(parse!=item.trace.costs.end()) parseCpu+=parse->second.inclusive;
            for(const auto& [key,event]:item.trace.events) {
                auto& target=MapLoadTrace::state.events[key]; target.count+=event.count; target.bytes+=event.bytes;
            }
            for(const auto& [key,cost]:item.trace.gr2Costs) {
                auto& target=MapLoadTrace::state.gr2Costs[key];
                target.inclusive+=cost.inclusive; target.exclusive+=cost.exclusive; target.calls+=cost.calls;
                target.maximum=std::max(target.maximum,cost.maximum);
            }
        }
        std::sort(intervals.begin(),intervals.end());
        double decodeWall=0;
        if(!intervals.empty()) {
            auto [begin,end]=intervals.front();
            for(const auto& interval:intervals) {
                if(interval.first>end) { decodeWall+=MapLoadTrace::Ms(end-begin); begin=interval.first; }
                end=std::max(end,interval.second);
            }
            decodeWall+=MapLoadTrace::Ms(end-begin);
        }
        // Worker CPU-duration attribution is separate from main-thread COST.
        // Nanoseconds are stored in event bytes to retain sub-ms resolution.
        MapLoadTrace::Count("gr2-jobs",{},impl_->items.size());
        MapLoadTrace::Count("gr2-files-completed",{},impl_->items.size());
        MapLoadTrace::Count("gr2-workers",std::to_string(workers),workers);
        MapLoadTrace::Count("gr2-peak",std::to_string(peak.load()),peak.load());
        MapLoadTrace::Count("gr2-busy-ns",{},static_cast<std::uint64_t>(busy*1e6));
        MapLoadTrace::Count("gr2-wall-ns",{},static_cast<std::uint64_t>(elapsed*1e6));
        MapLoadTrace::Count("gr2-worker-cost-ns",{},static_cast<std::uint64_t>(workerCost*1e6));
        MapLoadTrace::Count("gr2-worker-cpu-ns",{},cpuNs);
        MapLoadTrace::Count("gr2-worker-decode-cumulative-ns",{},static_cast<std::uint64_t>(decodeCpu*1e6));
        MapLoadTrace::Count("gr2-worker-decode-wall-ns",{},static_cast<std::uint64_t>(decodeWall*1e6));
        MapLoadTrace::Count("gr2-single-flight-joins",{},impl_->joins);
        if(impl_->animationBatch) {
            std::sort(parseIntervals.begin(),parseIntervals.end());
            double parseWall=0;
            if(!parseIntervals.empty()) {
                auto [begin,end]=parseIntervals.front();
                for(const auto& interval:parseIntervals) {
                    if(interval.first>end) { parseWall+=MapLoadTrace::Ms(end-begin); begin=interval.first; }
                    end=std::max(end,interval.second);
                }
                parseWall+=MapLoadTrace::Ms(end-begin);
            }
            MapLoadTrace::Count("animation-jobs",{},impl_->items.size());
            MapLoadTrace::Count("animation-worker-peak",std::to_string(peak.load()));
            MapLoadTrace::Count("animation-worker-busy-ns",{},static_cast<std::uint64_t>(busy*1e6));
            MapLoadTrace::Count("animation-worker-wall-ns",{},static_cast<std::uint64_t>(elapsed*1e6));
            MapLoadTrace::Count("animation-worker-cpu-ns",{},cpuNs);
            MapLoadTrace::Count("animation-worker-decode-cumulative-ns",{},static_cast<std::uint64_t>(decodeCpu*1e6));
            MapLoadTrace::Count("animation-worker-decode-wall-ns",{},static_cast<std::uint64_t>(decodeWall*1e6));
            MapLoadTrace::Count("animation-worker-parse-cumulative-ns",{},static_cast<std::uint64_t>(parseCpu*1e6));
            MapLoadTrace::Count("animation-worker-parse-wall-ns",{},static_cast<std::uint64_t>(parseWall*1e6));
            MapLoadTrace::Count("animation-single-flight-joins",{},impl_->joins);
        }
    }
}
std::span<const std::byte> Preparation::Payload(std::string_view id) {
    if(!current || !current->impl_->ran) return {};
    const auto found=current->impl_->index.find(Key(id));
    if(found==current->impl_->index.end()) return {};
    return current->impl_->items[found->second].bytes;
}
std::unique_ptr<Preparation::Result> Preparation::Take(std::string_view id) {
    if(!current || !current->impl_->ran) return {};
    const auto found=current->impl_->index.find(Key(id));
    if(found==current->impl_->index.end()) return {};
    return std::move(current->impl_->items[found->second].result);
}
}
