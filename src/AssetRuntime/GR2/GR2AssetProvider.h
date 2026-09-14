#pragma once
#include "AssetRuntime/AssetRuntime.h"

namespace AssetRuntime
{
AssetProvider& GetGR2AssetProvider();
// Prepare immutable document-owned variants without creating/changing playback state.
AssetError PrepareGR2Animation(const ModelHandle&,const AnimationHandle&,unsigned boundaryMask=9);
namespace GR2 {
inline std::atomic_size_t liveReaderDocuments{}, nativeFileReads{};
inline std::atomic_size_t nativeAnimationDecodes{}, boundClipHits{}, boundClipBypasses{};
inline std::atomic_size_t prewarmRequests{}, prewarmFailures{}, prewarmLimited{};
}
}
