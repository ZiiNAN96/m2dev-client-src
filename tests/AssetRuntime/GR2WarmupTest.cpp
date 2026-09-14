#include "GR2Fixtures.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include "AnimationRuntime/AnimationRuntime.h"
#include <cstdlib>
#include <iostream>
#include <new>

namespace { bool countAllocations=false; std::size_t allocations=0; }
void* operator new(std::size_t bytes) { if(countAllocations) ++allocations; if(auto* p=std::malloc(bytes?bytes:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t bytes) { return ::operator new(bytes); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using namespace AssetRuntime;
static void Check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main()
{
    try {
        {
            auto loaded=GetGR2AssetProvider().Load("shared.gr2",GR2Fixtures::Builder().Bytes());
            Check(bool(loaded),"valid fixture");
            const auto model=loaded.asset.Model(0);
            const auto animation=loaded.asset.Animation(0);
            const auto before=GR2::nativeAnimationDecodes.load();
            Check(PrepareGR2Animation(model,animation,8)==AssetError::None,"prepare loop variant");
            Check(GR2::nativeAnimationDecodes==before+1,"one decode for first shared variant");
            std::vector<std::unique_ptr<AnimationInstance>> instances;
            for(unsigned i=0;i<20;++i) {
                auto instance=model.GetDocument()->CreateAnimationInstance(model);
                Check(instance->SetMotion(animation,0,0,0,1)==AssetError::None,"shared instance binding");
                instances.push_back(std::move(instance));
            }
            Check(GR2::nativeAnimationDecodes==before+1,"20 actors do not decode again");
            const auto pose=instances[0]->BoneWorldMatrix(0)[12];
            Check(PrepareGR2Animation(model,animation,9)==AssetError::None,"prepare finite and loop variants");
            Check(instances[0]->BoneWorldMatrix(0)[12]==pose,"prewarm does not mutate playback/pose");
            const auto readyDecodes=GR2::nativeAnimationDecodes.load();
            AnimationStallAudit::Enable(true);
            allocations=0; countAllocations=true;
            { std::string emptyDiagnostic; }
            countAllocations=false;
            const auto emptyStringAllocations=allocations;
            std::size_t motionAllocations=0,poseAllocations=0;
            allocations=0; bool valid=true; countAllocations=true;
            for(unsigned frame=0;frame<120;++frame) for(auto& instance:instances) {
                const auto beforeMotion=allocations;
                valid &= instance->SetMotion(animation,float(frame)*.008f,.05f,frame%2?0:1,1)==AssetError::None;
                motionAllocations+=allocations-beforeMotion;
                const auto beforePose=allocations;
                instance->SetClock(float(frame)*.008f+.004f);
                valid &= instance->Evaluate({}).pose.Valid();
                poseAllocations+=allocations-beforePose;
            }
            countAllocations=false;
            Check(valid && GR2::nativeAnimationDecodes==readyDecodes,"warm sample/blend/switch does not decode");
            Check(poseAllocations==0,"warm pose/sample/blend/palette makes no C++ scalar/array heap allocations");
#if defined(_ITERATOR_DEBUG_LEVEL) && _ITERATOR_DEBUG_LEVEL > 0
            // MSVC's checked STL allocates a container proxy for SetMotion's
            // empty diagnostic string, even though its content uses SSO.
            Check(motionAllocations==2400*emptyStringAllocations,"only measured checked-STL string proxies during motion changes");
#else
            Check(motionAllocations==0,"production warm motion switches make no C++ scalar/array heap allocations");
#endif
            std::cout<<"ALLOCATIONS motion="<<motionAllocations<<" pose="<<poseAllocations<<" empty-string-control="<<emptyStringAllocations<<'\n';
            instances.clear();
            Check(PrepareGR2Animation(model,animation,9)==AssetError::None && GR2::nativeAnimationDecodes==readyDecodes,"document retains prepared clips after actors die");
            Check(PrepareGR2Animation(model,animation,16)==AssetError::InvalidInput,"invalid boundary rejected");
        }
        const auto lifetime=AnimationRuntime::GetLifetimeCounts();
        Check(!GR2::liveReaderDocuments && !liveDocuments && !lifetime.clips && !lifetime.skeletons,"last document releases prepared immutable clips");
        AnimationStallAudit::Frame row{}; row.displayRow=true; row.frameUs=1000;
        AnimationStallAudit::capturePhase=4;
        AnimationStallAudit::Save(row);
        Check(AnimationStallAudit::frames.size()==1 && AnimationStallAudit::frames[0].phase==4,"full capture includes sub-budget frames with phase");
        row.displayRow=false; AnimationStallAudit::Save(row);
        Check(AnimationStallAudit::frames.size()==1,"full capture does not double-count process rows");
        Check(AnimationStallAudit::BufferDiagnostic("retained",8) && AnimationStallAudit::diagnostics.size()==8,"diagnostics retained until shutdown");
        std::cout<<"PASS: one decode / 20 actors, immutable prewarm, retained document lifetime, verified warm allocations, complete frame capture\n";
        return 0;
    } catch(const std::exception& e) { countAllocations=false; std::cerr<<e.what()<<'\n'; return 1; }
}
