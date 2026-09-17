#pragma once
#include "Diagnostics.h"
#include <cstdint>

namespace Renderer {
// Diagnostic totals only; sampled once per second by the native smoke probe.
struct WorldResidencyDiagnostics {
    std::uint64_t terrainLoaded{}, terrainUnloaded{}, areasLoaded{}, areasUnloaded{};
    std::uint64_t terrainResident{}, areasResident{}, terrainAssignments{}, treeBlockerDraws{};
    std::uint64_t instanceBufferCreates{}, terrainVisible{}, terrainDrawn{};
};
inline WorldResidencyDiagnostics worldResidency;
}
