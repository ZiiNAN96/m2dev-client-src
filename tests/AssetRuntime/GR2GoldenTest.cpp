#include "GR2Golden.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AssetRuntime/GR2ReaderMode.h"
#include "AnimationRuntime/AnimationRuntime.h"
int main(int argc,char** argv)
{
    try {
        GR2Golden::Check(argc==4,"asset-root golden-file static|animation required");
        std::ifstream input(argv[2]); input.imbue(std::locale::classic());
        GR2Golden::Check(bool(input),"golden file missing");
        GR2Golden::Records records{&input,nullptr};
        const std::string_view mode=argv[3]; GR2Golden::Check(mode=="static"||mode=="animation","golden mode");
        if(mode=="static") GR2Golden::Static(records,AssetRuntime::GetGR2AssetProvider(),argv[1]);
        else GR2Golden::Animation(records,AssetRuntime::GetGR2AssetProvider(),argv[1]);
        records.Finish();
        const auto counts=AnimationRuntime::GetLifetimeCounts();
        GR2Golden::Check(!AssetRuntime::liveDocuments&&!AssetRuntime::liveAnimationInstances&&
            !AssetRuntime::liveIndependentAnimationInstances&&!AssetRuntime::GR2::liveReaderDocuments&&
            !counts.skeletons&&!counts.clips&&!AssetRuntime::grannyFileReads&&
            !AssetRuntime::referencePoseSamples&&!AssetRuntime::importPoseSamples,"golden resources/reference counters");
        std::cout<<"PASS "<<mode<<" golden records="<<records.count<<" SDK-free resources=0\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
