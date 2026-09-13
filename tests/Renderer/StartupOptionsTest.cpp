#include "Renderer/StartupOptions.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    if(StartupOptions{}.skinning!=PrototypeSkinningMode::GPU || startupSkinningMode!=PrototypeSkinningMode::GPU ||
       productionSkinningMode!=SkinningMode::GPU || StartupOptions{}.skinningSelected) return 10;
    StartupOptions production;production.ParseArgument(L"--skinning=gpu");production.ParseArgument(L"--skinning=gpu-prototype");
    if(!production.valid || !production.skinningSelected || production.skinning!=PrototypeSkinningMode::GPU) return 15;
    StartupOptions cpu;cpu.ParseArgument(L"--skinning=cpu");cpu.ParseArgument(L"--skinning=cpu");
    if(!cpu.valid || !cpu.skinningSelected || cpu.skinning!=PrototypeSkinningMode::CPU) return 16;
    cpu.ParseArgument(L"--skinning=gpu");if(cpu.valid) return 17;
    if(!IsReferenceSkinningAsset("D:\\ymir work\\pc\\warrior\\warrior_novice.gr2") ||
        IsReferenceSkinningAsset("d:/ymir work/pc/warrior/warrior_4-1.gr2")) return 14;
    StartupOptions gpu; gpu.ParseArgument(L"--skinning=gpu-prototype");
    if(!gpu.valid || gpu.skinning!=PrototypeSkinningMode::GPUPrototype) return 11;
    gpu.ParseArgument(L"--skinning=cpu"); if(gpu.valid) return 12;
    for(auto value:{L"--skinning=",L"--skinning=auto",L"--skinning=GPU",L"--skinning=GPU-prototype"}) {
        StartupOptions bad; bad.ParseArgument(value); if(bad.valid) return 13;
    }
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
    std::cout << "GPU skinning default/explicit/compatibility alias, explicit CPU, conflicts invalid; Diligent-only renderer: PASS\n";
}
