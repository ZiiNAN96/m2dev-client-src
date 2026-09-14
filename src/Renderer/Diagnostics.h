#pragma once
#include <fstream>

namespace Renderer
{
// ZiiNAN: Platform abstraction
#if defined(_DEBUG)
inline constexpr bool defaultVerboseDiagnostics = true;
#else
inline constexpr bool defaultVerboseDiagnostics = false;
#endif
inline bool verboseDiagnostics = defaultVerboseDiagnostics;

// Called only at the first existing failure of a renderer instance. Diagnostics
// must never turn a recoverable logging problem into another render failure.
inline void LogRendererFailure(const char* implementation, const void* instance,
                               const char* reason, int line) noexcept
{
    if (!verboseDiagnostics) return;
    try {
        std::ofstream log("renderer-failure.log", std::ios::app);
        log << "ERROR implementation=" << implementation << " line=" << line
            << " instance=" << instance << " reason=" << reason << '\n';
    } catch (...) {}
}
}
