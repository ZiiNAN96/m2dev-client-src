#pragma once
#include <atomic>
namespace AssetRuntime
{
enum class GR2ReaderMode { ZiiNAN };
inline GR2ReaderMode startupGR2Reader=GR2ReaderMode::ZiiNAN;
inline bool nativeGR2Prewarm=true;
inline std::atomic_size_t grannyFileReads{};
}
