#pragma once

#include <string_view>
#include "GpuSkinningPrototype.h"

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
    PrototypeSkinningMode skinning=PrototypeSkinningMode::CPU;
    bool skinningSelected=false;

    void ParseArgument(std::wstring_view argument)
    {
        // ZiiNAN: Diligent GPU skinning prototype
        if(argument.starts_with(L"--skinning=")) {
            const auto value=argument.substr(11);
            if(value!=L"cpu" && value!=L"gpu-prototype") { valid=false; return; }
            const auto requested=value==L"cpu" ? PrototypeSkinningMode::CPU : PrototypeSkinningMode::GPUPrototype;
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
