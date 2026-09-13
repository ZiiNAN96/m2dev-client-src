#include "Renderer/StartupOptions.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    StartupOptions defaults;
    defaults.ParseArgument(L"--existing-client-option");
    if(!defaults.valid || defaults.selected || defaults.backend!=BackendKind::DiligentD3D11) return 1;
    defaults.ParseArgument(L"--renderer-smoke-test");
    if(!defaults.smokeTest || defaults.selected) return 2;
    StartupOptions explicitDiligent;
    explicitDiligent.ParseArgument(L"--renderer=d3d11");
    explicitDiligent.ParseArgument(L"--renderer=diligent-d3d11");
    if(!explicitDiligent.valid || !explicitDiligent.selected) return 3;
    for(auto value : {L"--renderer=legacy-d3d9",L"--renderer=",L"--renderer=vulkan",
        L"--renderer=dx9",L"--renderer=dx12",L"--renderer=Diligent-d3d11"}) {
        StartupOptions bad; bad.ParseArgument(value);
        if(bad.valid) return 4;
        bad.ParseArgument(L"--renderer=diligent-d3d11");
        if(bad.valid) return 5;
    }
    explicitDiligent.ParseArgument(L"--renderer=legacy-d3d9");
    if(explicitDiligent.valid) return 7;
    std::cout << "Diligent-only default/explicit, removed Legacy, invalid/no fallback: PASS\n";
}
