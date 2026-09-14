#include "GR2Fixtures.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include "AssetRuntime/GR2/GR2Reader.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include <functional>
#include <iostream>

using namespace AssetRuntime;
static void Check(bool value,const std::string& message) { if(!value) throw std::runtime_error(message); }
static std::string bindingDiagnostic;
int main()
{
    try {
        std::size_t checks=0;
        auto expect=[&](std::string name,std::vector<std::byte> bytes) {
            auto result=GetGR2AssetProvider().Load("synthetic.gr2",bytes);
            Check(!result && !result.diagnostic.empty(),"malformed accepted: "+name);
            Check(!GR2::liveReaderDocuments && !liveDocuments,"failure ownership: "+name);
            ++checks;
        };
        {
            GR2Fixtures::Builder fixture; auto bytes=fixture.Bytes(); auto result=GetGR2AssetProvider().Load("synthetic.gr2",bytes);
            Check(bool(result),"valid deterministic fixture: "+result.diagnostic);
            auto model=result.asset.Model(0); auto instance=model.GetDocument()->CreateAnimationInstance(model);
            Check(instance && instance->BoneWorldMatrix(0).size()==16 && instance->CompositePose().Valid(),"bind pose available before first frame/collision");
            Check(instance&&instance->SetAnimation(result.asset.Animation(0),.5f)==AssetError::None,"own decoded animation binding");
            const auto decodes=GR2::nativeAnimationDecodes.load();
            auto second=model.GetDocument()->CreateAnimationInstance(model);
            Check(second->SetAnimation(result.asset.Animation(0),.25f)==AssetError::None && GR2::nativeAnimationDecodes==decodes,"instances share the document-owned decoded clip");
            bytes.clear(); Check(bool(instance->Evaluate({}).pose.Valid()),"no borrowed input storage");
            result.asset.ReleaseUploadData(); Check(instance->Evaluate({}).pose.Valid(),"pose survives upload data release");
            const auto before=instance->BoneWorldMatrix(0)[12];
            Check(instance->SetMotion(result.asset.Animation(0),.5f,.2f,1,1)==AssetError::None && instance->BoneWorldMatrix(0).size()==16 && instance->CompositePose().Valid() && instance->BoneWorldMatrix(0)[12]==before,"successful motion change preserves pose for collision before next frame");
            Check(instance->SetAnimation({},0)!=AssetError::None && !instance->Evaluate({}).pose.Valid(),"failed binding invalidates stale pose");
        }
        {
            GR2Fixtures::Builder f;
            const auto animation=f.fixups.at(f.fixups.at(f.root+12));
            const auto firstGroup=f.fixups.at(f.fixups.at(animation+16));
            const auto secondGroup=f.Allocate(84), secondTrack=f.Allocate(64), knot=f.Allocate(4), controls=f.Allocate(12);
            f.Name(secondGroup,"second"); f.Identity(secondGroup+12); f.Array(secondGroup,4,1,secondTrack);
            f.Name(secondTrack,"root"); f.Array(secondTrack+4,4,1,knot); f.Array(secondTrack+4,12,3,controls); f.Float(controls,7);
            const auto groups=f.Allocate(8); f.Pointer(groups,firstGroup); f.Pointer(groups+4,secondGroup); f.Array(animation,12,2,groups);
            auto first=GetGR2AssetProvider().Load("two-groups.gr2",f.Bytes());
            GR2Fixtures::Builder other; other.Name(other.model,"second");
            auto second=GetGR2AssetProvider().Load("second-model.gr2",other.Bytes());
            Check(first && second,"two groups with identical skeleton layouts");
            auto a=first.asset.Get()->CreateAnimationInstance(first.asset.Model(0));
            auto b=second.asset.Get()->CreateAnimationInstance(second.asset.Model(0));
            Check(a->SetAnimation(first.asset.Animation(0),.5f)==AssetError::None && b->SetAnimation(first.asset.Animation(0),.5f)==AssetError::None,"distinct track groups bind");
            Check(a->Evaluate({}).pose.Valid() && b->Evaluate({}).pose.Valid() && a->BoneWorldMatrix(0)[12]==0 && b->BoneWorldMatrix(0)[12]==7,"document sharing isolates track groups with matching skeleton layouts");
        }
        auto valid=GR2Fixtures::Builder().Bytes();
        {
            auto model=GetGR2AssetProvider().Load("model.gr2",valid);
            GR2Fixtures::Builder f; const auto rotation=f.curve+20, knots=f.Allocate(8), controls=f.Allocate(32);
            f.Float(knots+4,1); f.Float(controls+12,1); f.Array(rotation,4,2,knots); f.Array(rotation,12,8,controls);
            auto clip=GetGR2AssetProvider().Load("end-failure.gr2",f.Bytes()); Check(bool(clip),"end-boundary fixture");
            auto instance=model.asset.Get()->CreateAnimationInstance(model.asset.Model(0));
            Check(instance->SetAnimation(clip.asset.Animation(0),.5f)==AssetError::None,"valid looping boundary");
            clip.asset.Reset();
            animationRuntimeErrorSink=[](const char* message){bindingDiagnostic=message;};
            instance->SetMotionAtEnd(); animationRuntimeErrorSink=nullptr;
            Check(!instance->CompositePose().Valid() && bindingDiagnostic.find("end-failure.gr2")!=std::string::npos,"self-owned clip retained through failed end-boundary diagnostic"); ++checks;
        }
        for(std::size_t size:{0u,15u,31u,35u,55u,87u,131u}) expect("truncation",{valid.begin(),valid.begin()+size});
        auto mutate=[&](std::string name,std::size_t at,std::uint32_t value) { auto bytes=valid; GR2Fixtures::Put(bytes,at,value); GR2Fixtures::Builder::Checksum(bytes); expect(name,std::move(bytes)); };
        mutate("magic",0,0); mutate("version",32,99); mutate("section count",48,UINT32_MAX); mutate("offset",92,UINT32_MAX);
        mutate("overlap",92,88); mutate("alignment",104,3); mutate("huge allocation",100,UINT32_MAX); mutate("unsupported compression",88,4);
        mutate("invalid Oodle1 model",88,2);
        auto corrupt=valid; corrupt.back()^=std::byte(1); expect("checksum",corrupt);
        {
            auto bytes=valid; const auto relocation=GR2::U32(bytes,116); GR2Fixtures::Put(bytes,relocation,UINT32_MAX);
            GR2Fixtures::Builder::Checksum(bytes); expect("corrupt relocation",bytes);
        }
        const std::vector<std::pair<std::string,std::function<void(GR2Fixtures::Builder&)>>> mutations{
            {"huge array",[](auto& f){f.Value(f.root,UINT32_MAX);}},
            {"bad mesh index",[](auto& f){f.Value(f.indices,3);}},
            {"invalid parent",[](auto& f){f.Value(f.bone+4,5);}},
            {"cyclic hierarchy",[](auto& f){f.Value(f.bone+4,0);}},
            {"invalid weight",[](auto& f){f.data[f.vertices+12]=std::byte(254);}},
            {"invalid bone index",[](auto& f){f.data[f.vertices+16]=std::byte(1);}},
            {"malformed curve",[](auto& f){f.Value(f.curve+4,1);}},
            {"unsupported degree",[](auto& f){f.Value(f.curve,3);}},
            {"invalid string",[](auto& f){const auto at=f.Allocate(4100);std::fill(f.data.begin()+at,f.data.end(),std::byte('x'));f.Pointer(f.bone,at);}},
            {"nonfinite vertex",[](auto& f){f.Value(f.vertices,0x7fc00000);}}
        };
        for(const auto& [name,mutateFixture]:mutations) { GR2Fixtures::Builder fixture; mutateFixture(fixture); expect(name,fixture.Bytes()); }
        {
            GR2::File file(valid); GR2::Types types(file); bool rejected=false;
            try { types.Charge(256u*1024u*1024u); types.Array(types.Root(),"Models"); } catch(const GR2::Error&) { rejected=true; }
            Check(rejected,"cumulative repeated-reference allocation budget"); ++checks;
        }
        Check(!liveDocuments&&!liveAnimationInstances&&!liveMeshBindings&&!GR2::liveReaderDocuments,"all owners released");
        std::cout<<"PASS "<<checks<<" deterministic malformed cases, owned model/animation, upload release, resources=0\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}
