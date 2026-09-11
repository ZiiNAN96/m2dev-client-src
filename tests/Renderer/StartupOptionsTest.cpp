#include "Renderer/StartupOptions.h"
#include <iostream>

int main()
{
    using namespace Renderer;
    StartupOptions defaults;
    defaults.ParseArgument(L"--existing-client-option");
    if (!defaults.valid || defaults.backend != BackendKind::LegacyD3D9 || defaults.selected)
        return 1;
    StartupOptions explicitLegacy;
    explicitLegacy.ParseArgument(L"--renderer=legacy-d3d9");
    if (!explicitLegacy.valid || explicitLegacy.backend != BackendKind::LegacyD3D9)
        return 2;
    StartupOptions diligent;
    diligent.ParseArgument(L"--renderer=diligent-d3d11");
    diligent.ParseArgument(L"--renderer-smoke-test");
    if (!diligent.valid || !diligent.smokeTest || diligent.backend != BackendKind::DiligentD3D11)
        return 3;
    diligent.ParseArgument(L"--renderer=legacy-d3d9");
    if (diligent.valid)
        return 4;
    StartupOptions invalid;
    invalid.ParseArgument(L"--renderer=vulkan");
    if (invalid.valid)
        return 5;
    invalid.ParseArgument(L"--renderer=legacy-d3d9");
    if (invalid.valid)
        return 6;
    std::cout << "Default, explicit backend, invalid and conflicting selection: PASS\n";
    return 0;
}
