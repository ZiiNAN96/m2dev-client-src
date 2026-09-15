#pragma once
namespace Renderer {
inline unsigned liveWaterRenderers{};
// A negative value selects the production monotonic clock.
inline double waterTestSeconds=-1;
inline unsigned waterTestView=0;
inline bool waterTestReadback=false;
inline float waterTestInputPeak{},waterTestOutputPeak{};
inline bool waterTestFinite=true;
}
