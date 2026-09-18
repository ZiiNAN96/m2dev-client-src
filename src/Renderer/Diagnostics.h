#pragma once
#include <fstream>
#include <atomic>
#include <cstdint>

namespace Renderer
{
// ZiiNAN: Platform abstraction
#if defined(_DEBUG)
inline constexpr bool defaultVerboseDiagnostics = true;
#else
inline constexpr bool defaultVerboseDiagnostics = false;
#endif
inline bool verboseDiagnostics = defaultVerboseDiagnostics;
inline bool auditDiligentDiagnostics=false;
inline std::atomic<unsigned> diligentErrorCount{},diligentFatalCount{};
// Read-only evidence of the actual swapchain and submitted UI viewport.
struct DisplayLayoutAudit
{
    uint32_t backbufferWidth=0, backbufferHeight=0;
    uint32_t uiViewportWidth=0, uiViewportHeight=0;
    uint64_t uiDraws=0;
};
inline DisplayLayoutAudit displayLayoutAudit;

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
