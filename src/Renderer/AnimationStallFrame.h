#pragma once
#include "AssetRuntime/AnimationStallAudit.h"
#include "SkinningBenchmark.h"

namespace Renderer
{
struct AnimationStallFrame {
    bool active = AssetRuntime::AnimationStallAudit::enabled;
    std::uint64_t observedPresents = AssetRuntime::AnimationStallAudit::swapchainPresents;
    static void External() noexcept {
        AssetRuntime::AnimationStallAudit::External(skinningCpuCalls, skinningFallbacks, prototypePrepareUs);
    }
    AnimationStallFrame(bool game, bool minimized, bool foreground) noexcept {
        if (!active) return;
        External();
        AssetRuntime::AnimationStallAudit::BeginProcess(game, minimized, foreground);
    }
    void Presented() noexcept {
        if (!active || observedPresents == AssetRuntime::AnimationStallAudit::swapchainPresents) return;
        observedPresents = AssetRuntime::AnimationStallAudit::swapchainPresents;
        External(); AssetRuntime::AnimationStallAudit::Presented();
    }
    ~AnimationStallFrame() {
        if (!active) return;
        External(); AssetRuntime::AnimationStallAudit::EndProcess();
    }
};
}
