#include "Renderer/StartupOptions.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    StartupOptions defaults(true);
    defaults.ParseArgument(L"--existing-client-option");
    if(!defaults.valid || defaults.selected || defaults.backend!=BackendKind::DiligentD3D11) return 1;
    defaults.ParseArgument(L"--renderer-smoke-test");
    if(!defaults.smokeTest || defaults.selected) return 2;
    StartupOptions explicitDiligent(true);
    explicitDiligent.ParseArgument(L"--renderer=diligent-d3d11");
    explicitDiligent.ParseArgument(L"--renderer=diligent-d3d11");
    if(!explicitDiligent.valid || !explicitDiligent.selected) return 3;
    for(auto value : {L"--renderer=legacy-d3d9",L"--renderer=",L"--renderer=vulkan",
        L"--renderer=dx9",L"--renderer=dx12",L"--renderer=Diligent-d3d11"}) {
        StartupOptions bad(true); bad.ParseArgument(value);
        if(bad.valid) return 4;
        bad.ParseArgument(L"--renderer=diligent-d3d11");
        if(bad.valid) return 5;
    }
    StartupOptions unavailable(false);
    unavailable.ParseArgument(L"--renderer=diligent-d3d11");
    if(unavailable.valid) return 6;
    explicitDiligent.ParseArgument(L"--renderer=legacy-d3d9");
    if(explicitDiligent.valid) return 7;
    std::cout << "Diligent-only default/explicit, removed Legacy, invalid/unavailable/no fallback: PASS\n";
}
