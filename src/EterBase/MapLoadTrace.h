#pragma once
// Opt-in, main-thread wall-clock attribution. Exclusive costs never double-count
// nested work. Mapped pack reads are logical reads, not physical disk reads.
#include <chrono>
#include <cstdlib>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace MapLoadTrace {
using Clock = std::chrono::steady_clock;
inline double Ms(Clock::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }
struct Cost { double inclusive{}, exclusive{}; std::uint64_t calls{}; double maximum{}; };
struct Event { std::uint64_t count{}, bytes{}; };
struct Frame { std::string key; Clock::time_point start; double children{}; };
struct State {
    bool active{}, world{};
    std::string label;
    Clock::time_point start{}, lastPresent{};
    double firstPresent{};
    unsigned frames{}, stable{};
    std::uint64_t activity{}, lastActivity{};
    std::vector<Frame> stack;
    std::map<std::string, Cost> costs;
    std::map<std::string, Event> events;
};
// Background threads deliberately do not contribute to main-thread attribution.
inline thread_local State state;
inline bool explicitlyEnabled{};
inline bool Enabled() { static const bool value = [] { const auto* e=std::getenv("M2_MAP_LOAD_TRACE"); return e && std::string_view(e)=="1"; }(); return explicitlyEnabled || value; }
inline std::string Clean(std::string_view s) {
    std::string r(s); for(auto& c:r) { if(c=='\t'||c=='\r'||c=='\n')c=' '; if(c=='\\')c='/'; }
    return r;
}
inline void Count(std::string_view kind, std::string_view name={}, std::uint64_t bytes=0, bool activity=false) {
    if(!state.active)return;
    auto& e=state.events[Clean(kind)+"\t"+Clean(name)]; ++e.count; e.bytes+=bytes;
    if(activity)++state.activity;
}
class Scope {
    bool active_{};
public:
    Scope(std::string_view phase, std::string_view name, std::string_view kind="cpu", bool enabled=true) : active_(enabled&&state.active) {
        if(active_)state.stack.push_back({Clean(phase)+"\t"+Clean(kind)+"\t"+Clean(name),Clock::now(),0});
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
    void Stop() {
        if(!active_)return; active_=false;
        auto f=std::move(state.stack.back()); state.stack.pop_back();
        const auto elapsed=Ms(Clock::now()-f.start);
        auto& c=state.costs[f.key]; c.inclusive+=elapsed; c.exclusive+=elapsed-f.children; ++c.calls;
        if(elapsed>c.maximum)c.maximum=elapsed;
        if(!state.stack.empty())state.stack.back().children+=elapsed;
    }
    ~Scope(){Stop();}
};
inline bool Begin(std::string_view label) {
    if(!Enabled()||state.active)return false;
    state=State{}; state.label=Clean(label); state.start=Clock::now(); state.active=true; return true;
}
inline bool End(std::string_view reason) {
    if(!state.active||!state.stack.empty())return false;
    const auto total=Ms(Clock::now()-state.start); state.active=false;
    std::ofstream out("map-load-trace.tsv",std::ios::app); out<<std::fixed<<std::setprecision(6);
    out<<"LOAD\t"<<state.label<<"\t"<<total<<"\t"<<state.firstPresent<<"\t"<<state.frames<<"\t"<<reason<<'\n';
    double accounted=0;
    for(const auto& [key,c]:state.costs){out<<"COST\t"<<key<<"\t"<<c.inclusive<<"\t"<<c.exclusive<<"\t"<<c.calls<<"\t"<<c.maximum<<'\n';accounted+=c.exclusive;}
    out<<"COST\tOther\tunknown\tunattributed / frame pacing\t"<<total-accounted<<"\t"<<total-accounted<<"\t1\n";
    for(const auto& [key,e]:state.events)out<<"EVENT\t"<<key<<"\t"<<e.count<<"\t"<<e.bytes<<'\n';
    out<<"END\n"; return bool(out);
}
inline void WorldRendered() { if(state.active)state.world=true; }
inline void Presented() {
    if(!state.active||!state.world)return;
    state.world=false; const auto now=Clock::now(); ++state.frames;
    if(state.frames==1)state.firstPresent=Ms(now-state.start);
    const bool stable=state.frames>1&&Ms(now-state.lastPresent)<=50.0&&state.activity==state.lastActivity;
    state.stable=stable?state.stable+1:0; state.lastPresent=now; state.lastActivity=state.activity;
    // Three consecutive presented frames <=50ms with no new load/upload/compile.
    if(state.stable>=3)End("stable-present");
}
}
