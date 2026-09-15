#pragma once
#include "GpuSkinningPrototype.h"
#include <fstream>
#include <iomanip>
#include <map>

namespace Renderer {
// ZiiNAN: GPU skinning production path — explicitly enabled, bounded diagnostic capture.
inline bool skinningBenchmarkEnabled=false;
inline uint64_t skinningCpuCalls=0,skinningCpuVertices=0,skinningFallbacks=0;
struct SkinningBenchmarkFrame {
    uint64_t serial=0,cpuCalls=0,cpuVertices=0,gpuCalls=0,boneBytes=0,vertexBytes=0,uploads=0,draws=0,visible=0,fallbacks=0;
    int stage=-1,sample=-1;
    double skinUs=0,prepUs=0,deformUs=0,renderUs=0,worldUs=0,processUs=0,presentUs=0,wallUs=0;
    std::uint64_t shadowDraws{},shadowCasters{},shadowBytes{},aoBytes{};
    double shadowCpuUs{},aoCpuUs{};
};
inline SkinningBenchmarkFrame skinningBenchmarkCurrent;
inline std::vector<SkinningBenchmarkFrame> skinningBenchmarkFrames;
inline std::map<uint64_t,double> skinningBenchmarkGpuTimes;
inline std::map<uint64_t,double> ambientBenchmarkGpuTimes;
inline uint64_t skinningBenchmarkSerial=0,skinningBenchmarkDropped=0;
inline constexpr size_t skinningBenchmarkLimit=100000;
struct SkinningBenchmarkProcessScope {
    bool active=skinningBenchmarkEnabled;
    PrototypeClock::time_point start{};
    uint64_t cpu=0,vertices=0,gpu=0,bones=0,fallback=0;
    double skin=0,prep=0;
    SkinningBenchmarkProcessScope() {
        if(!active) return;
        start=PrototypeClock::now();cpu=skinningCpuCalls;vertices=skinningCpuVertices;gpu=prototypeFrames;bones=prototypeBoneBytes;
        fallback=skinningFallbacks;skin=prototypeCpuSkinUs;prep=prototypePrepareUs;
        skinningBenchmarkCurrent={};skinningBenchmarkCurrent.serial=++skinningBenchmarkSerial;
    }
    ~SkinningBenchmarkProcessScope() {
        if(!active) return;
        auto& s=skinningBenchmarkCurrent;s.wallUs=PrototypeMicroseconds(start);
        s.cpuCalls=skinningCpuCalls-cpu;s.cpuVertices=skinningCpuVertices-vertices;s.gpuCalls=prototypeFrames-gpu;
        s.boneBytes=prototypeBoneBytes-bones;s.fallbacks=skinningFallbacks-fallback;
        s.skinUs=prototypeCpuSkinUs-skin;s.prepUs=prototypePrepareUs-prep;
        if(skinningBenchmarkFrames.size()<skinningBenchmarkLimit) skinningBenchmarkFrames.push_back(s);
        else ++skinningBenchmarkDropped;
    }
    void BeforeFrameLimit() {
        if(active) skinningBenchmarkCurrent.processUs=PrototypeMicroseconds(start)-skinningBenchmarkCurrent.presentUs;
    }
};
inline void WriteSkinningBenchmark()
{
    if(!skinningBenchmarkEnabled) return;
    std::ofstream csv("skinning-benchmark.csv");csv<<std::setprecision(12);
    csv<<"serial,stage,sample,cpu_calls,cpu_vertices,gpu_deforms,bone_bytes,vertex_bytes,vertex_updates,draws,visible,fallbacks,cpu_skin_us,gpu_prep_us,deform_us,render_cpu_us,world_cpu_us,process_cpu_us,present_us,wall_frame_us,gpu_frame_us,shadow_draws,shadow_casters,shadow_bytes,ao_bytes,shadow_cpu_us,ao_cpu_us,ao_gpu_us\n";
    for(const auto& s:skinningBenchmarkFrames) {
        csv<<s.serial<<','<<s.stage<<','<<s.sample<<','<<s.cpuCalls<<','<<s.cpuVertices<<','<<s.gpuCalls<<','<<s.boneBytes<<','<<s.vertexBytes<<','<<s.uploads<<','<<s.draws<<','<<s.visible<<','<<s.fallbacks<<','
            <<s.skinUs<<','<<s.prepUs<<','<<s.deformUs<<','<<s.renderUs<<','<<s.worldUs<<','<<s.processUs<<','<<s.presentUs<<','<<s.wallUs<<',';
        auto found=skinningBenchmarkGpuTimes.find(s.serial);if(found!=skinningBenchmarkGpuTimes.end()) csv<<found->second;
        csv<<','<<s.shadowDraws<<','<<s.shadowCasters<<','<<s.shadowBytes<<','<<s.aoBytes<<','<<s.shadowCpuUs<<','<<s.aoCpuUs<<',';
        auto ao=ambientBenchmarkGpuTimes.find(s.serial);if(ao!=ambientBenchmarkGpuTimes.end())csv<<ao->second;
        csv<<'\n';
    }
    std::ofstream meta("skinning-benchmark-meta.txt");
    meta<<"VSync=1 fixedNativeClock=16/17ms capacity="<<skinningBenchmarkLimit<<" frames="<<skinningBenchmarkFrames.size()<<" dropped="<<skinningBenchmarkDropped<<'\n';
}
}
