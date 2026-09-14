#pragma once
#include <atomic>
namespace AssetRuntime
{
enum class GR2ReaderMode { Granny, ZiiNAN };
inline GR2ReaderMode startupGR2Reader=GR2ReaderMode::Granny;
inline std::atomic_size_t grannyFileReads{};
}
