#pragma once

#include <string_view>
#include "GpuSkinningPrototype.h"
#include "Diagnostics.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AssetRuntime/GR2ReaderMode.h"
#include "Vegetation/VegetationRuntime.h"

namespace Renderer
{
enum class BackendKind { DiligentD3D11 };

// Unknown renderer values and conflicting selections are errors, never silent fallback.
struct StartupOptions
{
    // ZiiNAN: Removed final D3D9 compile-time dependency. One production API.
    BackendKind backend=BackendKind::DiligentD3D11;
    bool selected = false;
    bool smokeTest = false;
    bool valid = true;
    PrototypeSkinningMode skinning=defaultStartupSkinningMode;
    bool skinningSelected=false;
    bool diagnostics=defaultVerboseDiagnostics;
    AssetRuntime::AnimationRuntimeMode animationRuntime=AssetRuntime::AnimationRuntimeMode::ZiiNAN;
    bool animationRuntimeSelected=false;
    bool animationStallAudit=false;
    bool loadWarmupAudit=false;
    bool gr2Prewarm=true,gr2PrewarmSelected=false;
    AssetRuntime::GR2ReaderMode gr2Reader=AssetRuntime::GR2ReaderMode::ZiiNAN;
    bool gr2ReaderSelected=false;
    Vegetation::Mode vegetation=Vegetation::Mode::Reference;bool vegetationSelected=false;

    void ParseArgument(std::wstring_view argument)
    {
        if(argument.starts_with(L"--vegetation=")) {
            const auto value=argument.substr(13);if(value!=L"reference"&&value!=L"ziinan"){valid=false;return;}
            const auto requested=value==L"ziinan"?Vegetation::Mode::ZiiNAN:Vegetation::Mode::Reference;
            if(vegetationSelected&&vegetation!=requested)valid=false;vegetation=requested;vegetationSelected=true;return;
        }
        if(argument.starts_with(L"--gr2-reader=")) {
            const auto value=argument.substr(13);
            if(value!=L"ziinan") { valid=false; return; }
            gr2ReaderSelected=true;
            return;
        }
        if (argument == L"--animation-stall-audit") { animationStallAudit=true; return; }
        if (argument == L"--load-warmup-audit") { loadWarmupAudit=true; return; }
        if (argument.starts_with(L"--gr2-prewarm=")) {
            const auto value=argument.substr(14);
            if(value!=L"on" && value!=L"off") { valid=false; return; }
            const bool requested=value==L"on";
            if(gr2PrewarmSelected && gr2Prewarm!=requested) valid=false;
            gr2Prewarm=requested; gr2PrewarmSelected=true; return;
        }
        if (argument.starts_with(L"--animation-runtime=")) {
            const auto value=argument.substr(20);
            if(value!=L"ziinan") { valid=false; return; }
            animationRuntimeSelected=true; return;
        }
        if (argument == L"--renderer-diagnostics") { diagnostics = true; return; }
        // ZiiNAN: GPU skinning production path — old CLI spelling is an explicit compatibility alias.
        if(argument.starts_with(L"--skinning=")) {
            const auto value=argument.substr(11);
            if(value!=L"cpu" && value!=L"gpu" && value!=L"gpu-prototype") { valid=false; return; }
            const auto requested=value==L"cpu" ? PrototypeSkinningMode::CPU : PrototypeSkinningMode::GPU;
            if(skinningSelected && requested!=skinning) valid=false;
            skinning=requested; skinningSelected=true; return;
        }
        if (argument == L"--renderer-smoke-test")
            smokeTest = true;
        constexpr std::wstring_view prefix = L"--renderer=";
        if (!argument.starts_with(prefix))
            return; // Preserve all existing, non-renderer command-line arguments.
        const auto value = argument.substr(prefix.size());
        BackendKind requested;
        if (value == L"d3d11" || value == L"diligent-d3d11")
            requested = BackendKind::DiligentD3D11;
        else
        {
            valid = false;
            return;
        }
        if (selected && requested != backend)
            valid = false;
        backend = requested;
        selected = true;
    }
};
}
