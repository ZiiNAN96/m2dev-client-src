#pragma once
#include "AssetRuntime/AssetRuntime.h"

namespace AssetRuntime
{
AssetProvider& GetGR2AssetProvider();
namespace GR2 {
inline std::atomic_size_t liveReaderDocuments{}, nativeFileReads{};
inline std::atomic_size_t nativeAnimationDecodes{}, boundClipHits{}, boundClipBypasses{};
}
}
