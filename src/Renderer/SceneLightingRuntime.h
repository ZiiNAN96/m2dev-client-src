#pragma once
#include "Graphics/SceneLighting.h"
#include <atomic>

namespace Renderer
{
// Same main-thread ownership as GraphicsConfig. No asset format or backend handles.
inline Graphics::SceneLightingState sceneLighting;
inline std::atomic_uint64_t liveSceneLightingResources{}, liveLightBuffers{}, lightBufferUpdates{};
inline std::atomic_uint64_t liveLightingPipelines{};
inline std::atomic_uint64_t modernTerrainDraws{}, modernVegetationDraws{};
}
