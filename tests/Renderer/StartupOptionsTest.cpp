#include "Renderer/StartupOptions.h"
#include <iostream>
int main()
{
    using namespace Renderer;
    if(StartupOptions{}.gr2Reader!=AssetRuntime::GR2ReaderMode::Granny) return 30;
    StartupOptions reader; reader.ParseArgument(L"--gr2-reader=ziinan");
    if(!reader.valid || reader.animationRuntime!=AssetRuntime::AnimationRuntimeMode::ZiiNAN) return 31;
    reader.ParseArgument(L"--animation-runtime=granny"); if(reader.valid) return 32;
    StartupOptions conflict; conflict.ParseArgument(L"--animation-runtime=granny"); conflict.ParseArgument(L"--gr2-reader=ziinan"); if(conflict.valid) return 33;
    for(auto value:{L"--gr2-reader=",L"--gr2-reader=auto",L"--gr2-reader=ZiiNAN"}) { StartupOptions bad; bad.ParseArgument(value); if(bad.valid) return 34; }
    if (StartupOptions{}.animationStallAudit) return 24;
    StartupOptions stall;
    stall.ParseArgument(L"--animation-stall-audit");
    if (!stall.valid || !stall.animationStallAudit || stall.animationRuntimeSelected ||
        stall.animationRuntime != AssetRuntime::AnimationRuntimeMode::Granny) return 25;
    if (StartupOptions{}.animationRuntime!=AssetRuntime::AnimationRuntimeMode::Granny ||
        StartupOptions{}.animationRuntimeSelected) return 20;
    StartupOptions animation;
    animation.ParseArgument(L"--animation-runtime=ziinan");
    animation.ParseArgument(L"--animation-runtime=ziinan");
    if (!animation.valid || !animation.animationRuntimeSelected ||
        animation.animationRuntime!=AssetRuntime::AnimationRuntimeMode::ZiiNAN) return 21;
    animation.ParseArgument(L"--animation-runtime=granny");
    if (animation.valid) return 22;
    for (auto value : {L"--animation-runtime=", L"--animation-runtime=auto", L"--animation-runtime=ZiiNAN"}) {
        StartupOptions bad; bad.ParseArgument(value); if (bad.valid) return 23;
    }
    if(StartupOptions{}.diagnostics != defaultVerboseDiagnostics) return 18;
    StartupOptions diagnostics; diagnostics.ParseArgument(L"--renderer-diagnostics");
    if(!diagnostics.valid || !diagnostics.diagnostics || diagnostics.selected || diagnostics.skinningSelected) return 19;
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
