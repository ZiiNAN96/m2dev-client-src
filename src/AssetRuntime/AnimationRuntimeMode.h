#pragma once

#include <atomic>
#include <cstdint>

namespace AssetRuntime
{
// ZiiNAN: Animation Runtime boundary
enum class AnimationRuntimeMode { Granny, ZiiNAN };
inline AnimationRuntimeMode startupAnimationRuntime = AnimationRuntimeMode::Granny;
inline std::atomic_uint64_t referencePoseSamples{}, independentPoseSamples{}, animationRuntimeFailures{};
inline std::atomic_size_t liveIndependentAnimationInstances{};
inline std::atomic_uint64_t importPoseSamples{}, importedAnimationClips{};
inline std::atomic_uint64_t importedBindingWarnings{};
inline std::atomic_uint64_t importTotalMicroseconds{}, importPeakMicroseconds{};
inline std::atomic_size_t retainedImportKeyBytes{}, peakRetainedImportKeyBytes{};
inline std::atomic_uint64_t importCacheHits{}, importCacheEvictions{};
inline void (*animationRuntimeErrorSink)(const char*) = nullptr;
inline void (*animationRuntimeCacheClearSink)() noexcept = nullptr;
inline void ClearAnimationRuntimeCaches() noexcept
{
    if (animationRuntimeCacheClearSink) animationRuntimeCacheClearSink();
}
}
