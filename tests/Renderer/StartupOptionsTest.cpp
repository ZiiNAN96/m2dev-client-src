#include "Renderer/StartupOptions.h"
#include <iostream>

int main()
{
    using namespace Renderer;
    // ZiiNAN: Both compiled-availability defaults are tested in every build configuration.
    for (bool available : {false, true})
    {
        StartupOptions defaults(available);
        defaults.ParseArgument(L"--existing-client-option");
        const auto expected = available ? BackendKind::DiligentD3D11 : BackendKind::LegacyD3D9;
        if (!defaults.valid || defaults.backend != expected || defaults.selected || defaults.smokeTest)
            return 1;
        StartupOptions sameExplicit(available);
        sameExplicit.ParseArgument(available ? L"--renderer=diligent-d3d11" : L"--renderer=legacy-d3d9");
        if (!sameExplicit.valid || !sameExplicit.selected || sameExplicit.backend != defaults.backend ||
            sameExplicit.smokeTest != defaults.smokeTest) return 7;
        defaults.ParseArgument(L"--renderer-smoke-test");
        if (!defaults.smokeTest || defaults.backend != expected || defaults.selected) return 8;
    }
    StartupOptions explicitLegacy(true);
    explicitLegacy.ParseArgument(L"--renderer=legacy-d3d9");
    if (!explicitLegacy.valid || explicitLegacy.backend != BackendKind::LegacyD3D9)
        return 2;
    StartupOptions diligent(false); // Explicit Diligent remains explicit; availability is rejected by WinMain.
    diligent.ParseArgument(L"--renderer=diligent-d3d11");
    diligent.ParseArgument(L"--renderer-smoke-test");
    if (!diligent.valid || !diligent.smokeTest || diligent.backend != BackendKind::DiligentD3D11)
        return 3;
    diligent.ParseArgument(L"--renderer=legacy-d3d9");
    if (diligent.valid)
        return 4;
    StartupOptions invalid(true);
    invalid.ParseArgument(L"--renderer=vulkan");
    if (invalid.valid)
        return 5;
    invalid.ParseArgument(L"--renderer=legacy-d3d9");
    if (invalid.valid)
        return 6;
    StartupOptions repeated(true);
    repeated.ParseArgument(L"--renderer=legacy-d3d9");
    repeated.ParseArgument(L"--renderer=legacy-d3d9");
    if (!repeated.valid || repeated.backend != BackendKind::LegacyD3D9) return 9;
    repeated.ParseArgument(L"--renderer=diligent-d3d11");
    repeated.ParseArgument(L"--renderer=diligent-d3d11");
    if (repeated.valid) return 10;
    for (auto value : {L"--renderer=", L"--renderer=Diligent-d3d11", L"--renderer=dx12"})
    {
        StartupOptions bad(false);
        bad.ParseArgument(value);
        if (bad.valid) return 11;
    }
    std::cout << "ON/OFF defaults, default/explicit equivalence, explicit fallback, repeated/invalid/conflicting selection: PASS\n";
    return 0;
}
