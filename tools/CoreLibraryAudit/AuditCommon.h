#pragma once
#include "AnimationRuntime/AnimationRuntime.h"
#include "AssetRuntime/GR2/GR2Reader.h"
#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>

namespace CLib {
namespace AR = AnimationRuntime;
namespace GR = AssetRuntime::GR2;
inline void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
inline std::vector<std::byte> Read(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary | std::ios::ate); Check(bool(in), "Cannot open " + path.string());
    const auto size = in.tellg(); Check(size > 0 && size <= 256 * 1024 * 1024, "Input size bound");
    std::vector<std::byte> bytes(static_cast<size_t>(size)); in.seekg(0);
    Check(bool(in.read(reinterpret_cast<char*>(bytes.data()), size)), "Short input"); return bytes;
}
using Clock = std::chrono::steady_clock;
inline double Micros(Clock::time_point start) { return std::chrono::duration<double, std::micro>(Clock::now()-start).count(); }
struct Stats { double median{}, p95{}, maximum{}; };
inline Stats Summarize(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    return {values[values.size()/2], values[static_cast<size_t>(std::ceil(values.size()*.95))-1], values.back()};
}
inline void PrintStats(const char* name, Stats s) {
    std::cout << ",\"" << name << "\":{\"median_us\":" << s.median << ",\"p95_us\":" << s.p95 << ",\"max_us\":" << s.maximum << '}';
}
template<class F> Stats Bench(F function, int batch=32) {
    for(int i=0;i<64;++i) function(i);
    std::vector<double> values; values.reserve(31);
    for(int r=0;r<31;++r) { auto begin=Clock::now(); for(int i=0;i<batch;++i) function(r*batch+i); values.push_back(Micros(begin)/batch); }
    return Summarize(std::move(values));
}
inline size_t ClipBytes(const AR::RuntimeAnimationClip& clip) {
    size_t size=sizeof(clip)+clip.Name().capacity()+1+clip.Tracks().capacity()*sizeof(AR::AnimationTrack);
    for(const auto& t:clip.Tracks()) size+=t.translation.keys.capacity()*sizeof(AR::Keyframe<AR::Vector3>)+
        t.rotation.keys.capacity()*sizeof(AR::Keyframe<AR::Quaternion>)+t.scaleShear.keys.capacity()*sizeof(AR::Keyframe<AR::ScaleShear>);
    return size;
}
}
