#pragma once
#include "Graphics/GraphicsSettings.h"

namespace Renderer
{
// Main/render-thread value snapshot. Published at the frame boundary, never by asset workers.
// No allocations, file access, Python, string lookups or change comparisons in draw paths.
inline Graphics::GraphicsRuntimeConfig graphicsRuntimeConfig;
inline const Graphics::GraphicsRuntimeConfig& GetGraphicsRuntimeConfig() { return graphicsRuntimeConfig; }
inline void ApplyGraphicsRuntimeConfig(const Graphics::GraphicsRuntimeConfig& config) { graphicsRuntimeConfig = config; }
}
