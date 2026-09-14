#pragma once

namespace Renderer
{
// ZiiNAN: Platform abstraction
#if defined(_DEBUG)
inline constexpr bool defaultVerboseDiagnostics = true;
#else
inline constexpr bool defaultVerboseDiagnostics = false;
#endif
inline bool verboseDiagnostics = defaultVerboseDiagnostics;
}
