#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include "AnimationRuntime/AnimationRuntime.h"
#include "GlTFCharacterFixtures.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace AR=AnimationRuntime;
using namespace AssetRuntime;
static void Check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
static void Near(float a,float b,const char* message) {Check(std::abs(a-b)<.002f,message);}
static LoadResult Load(const GlTFFixtures::Builder& fixture) {return GetGlTFAssetProvider().Load("fixture.glb",fixture.Bytes());}
static void Replace(std::string& text,std::string_view from,std::string_view to)
{
    const auto pos=text.find(from);Check(pos!=std::string::npos,"test mutation matches");text.replace(pos,from.size(),to);
}
static void Reject(const GlTFFixtures::Builder& fixture,const char* message)
{
    const auto result=Load(fixture);Check(!result && !result.diagnostic.empty(),message);
}
static void Errors()
{
    auto f=SkinAnimationFixture();Replace(f.nodes,R"("skin":0,)","");Reject(f,"missing skin rejects attributes");
    f=SkinAnimationFixture();f.binary[96]=std::byte{2};Reject(f,"joint range rejected");
    f=SkinAnimationFixture();Replace(f.extra,R"("joints":[1,2])",R"("joints":[1,1])");Reject(f,"duplicate joint ID rejected");
    f=SkinAnimationFixture();Replace(f.extra,R"("joints":[1,2])",R"("joints":[1,99])");Reject(f,"missing joint node rejected");
    f=SkinAnimationFixture();Replace(f.accessors[5],R"("count":2)",R"("count":1)");Reject(f,"inverse count rejected");
    f=SkinAnimationFixture();Replace(f.extra,R"(,"inverseBindMatrices":5)","");Check(bool(Load(f)),"absent inverse binds use specified identity default");
    f=SkinAnimationFixture();f.binary[108]=f.binary[109]=std::byte{};Reject(f,"zero weights rejected");
    f=SkinAnimationFixture();f.binary[108]=f.binary[109]=std::byte{255};Reject(f,"grossly invalid weight sum not repaired");
    f=SkinAnimationFixture();f.nodes=R"({"mesh":0,"skin":0,"children":[1],"translation":[1e39,0,0]},{"children":[2]}, {})";Reject(f,"nonfinite node rejected");
    f=SkinAnimationFixture();Replace(f.nodes,R"("children":[2])",R"("children":[0])");Reject(f,"cycle rejected");
    f=SkinAnimationFixture();Replace(f.extra,R"("output":7)",R"("output":1000)");Reject(f,"bad animation accessor rejected");
    f=SkinAnimationFixture();Replace(f.extra,R"("node":2)",R"("node":99)");Reject(f,"unknown target rejected");
    f=SkinAnimationFixture();Replace(f.extra,R"("STEP")",R"("CUBICSPLINE")");Reject(f,"cubic fails without linear fallback");
    f=SkinAnimationFixture();f.extra+=R"(,"extensionsRequired":["unsupported_required"] )";Reject(f,"required extension rejected");
    f=SkinAnimationFixture();f.attributes+=R"(,"JOINTS_1":3,"WEIGHTS_1":4)";Reject(f,"extra influences explicitly rejected");
    f=SkinAnimationFixture();Replace(f.extra,R"("path":"translation")",R"("path":"weights")");Reject(f,"morph animation rejected");
    for(const auto times:{std::array<float,2>{1,0},std::array<float,2>{1,1},std::array<float,2>{-1,1}}) {
        f=SkinAnimationFixture();std::memcpy(f.binary.data()+248,times.data(),8);Reject(f,"bad timestamps rejected");
    }
    f=SkinAnimationFixture();const float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(f.binary.data()+120,&nan,4);Reject(f,"nonfinite inverse bind rejected");
    f=SkinAnimationFixture();Replace(f.accessors[6],R"("count":2)",R"("count":0)");Reject(f,"empty channel rejected");
    // A constant clip at time zero is valid and has a defined bind-independent pose.
    f=SkinAnimationFixture();Replace(f.accessors[6],R"("count":2)",R"("count":1)");Replace(f.accessors[7],R"("count":2)",R"("count":1)");
    auto zero=Load(f);Check(bool(zero),"zero-duration clip accepted");auto instance=zero.asset.Get()->CreateAnimationInstance(zero.asset.Model(0));
    Check(instance->SetMotion(zero.asset.Animation(0),0,0,1,1)==AssetError::None,"zero duration motion");
    instance->SetClock(100);Check(instance->Evaluate({}).pose.Valid()&&!instance->IsPlaying(),"zero duration one-shot clamps");
}
static void Conversion()
{
    auto f=SkinAnimationFixture();
    f.nodes=R"({"mesh":0,"skin":0,"rotation":[0,0.70710678,0,0.70710678],"scale":[2,3,4],"translation":[1,2,3],"children":[1]},{"name":"same","children":[2]},{"name":"same","translation":[0,1,0]})";
    auto result=Load(f);Check(bool(result),result.diagnostic.c_str());
    const auto* skeleton=result.asset.Get()->RuntimeSkeleton(0);Check(skeleton&&skeleton->Bones().size()==3,"translated runtime skeleton");
    const auto& metadata=*result.asset.Model(0).Get()->skeleton;
    Check(metadata.bones[0].sourceNodeIndex==1 && metadata.bones[1].sourceNodeIndex==2 && metadata.bones[0].parentIndex==2,"joint order and real ancestor retained");
    Check(metadata.FindBone("same")==0,"legacy name lookup deterministically returns first source ID");
    Check(ResolveAttachment(metadata,1,AttachmentKind::Weapon).bone==1,"explicit attachment ID works with duplicate names");
    auto instance=result.asset.Get()->CreateAnimationInstance(result.asset.Model(0));Check(instance->Evaluate({}).pose.Valid(),"bind pose evaluates");
    const auto joint=instance->BoneWorldMatrix(1);
    Near(joint[12],100,"root translation x");Near(joint[13],-300,"root translation y");Near(joint[14],500,"ancestor scale and joint translation");
    Near(joint[0],0,"root quaternion orientation");Near(joint[1],2,"root quaternion axis conversion");
    std::array<float,24> vertices{};
    Check(result.asset.Get()->CopyVertices(0,0,VertexLayout::PositionNormalUV,std::as_writable_bytes(std::span(vertices)))==AssetError::None,"positions decode");
    Near(vertices[0],0,"mesh transform is not baked twice");Near(vertices[8],100,"meter scale once");
    // Root is now an animated non-joint ancestor. No special evaluator is needed.
    Replace(f.extra,R"("node":2)",R"("node":0)");result=Load(f);Check(bool(result),"animated ancestor accepted");
    // Forest with two actual roots, no synthetic hierarchy.
    f=SkinAnimationFixture();f.nodes=R"({"mesh":0,"skin":0},{"name":"root"},{"name":"tip","translation":[0,1,0]})";
    Replace(f.extra,R"(,"skeleton":1)","");
    auto json=f.Json();Replace(json,R"("nodes":[0])",R"("nodes":[0,1,2])");
    auto forest=GetGlTFAssetProvider().Load("forest.glb",GlTFFixtures::GLB(json,f.binary));Check(bool(forest),"multiple roots supported");
    Check(forest.asset.Model(0).Get()->skeleton->bones[1].parentIndex==-1,"forest parent remains root");
    // glTF LINEAR quaternion needs constant angular speed, in the existing sampler.
    f=SkinAnimationFixture();auto view=f.View(std::array<float,8>{0,0,0,1,0,1,0,0});auto accessor=f.Accessor(view,5126,2,"VEC4");
    Replace(f.extra,R"("output":7)",R"("output":)"+std::to_string(accessor));Replace(f.extra,"translation","rotation");Replace(f.extra,"STEP","LINEAR");
    result=Load(f);Check(bool(result),"quaternion clip imports");AR::AnimationPose pose;pose.Prepare(3);
    Check(AR::Sample(*result.asset.Get()->RuntimeSkeleton(0),*result.asset.Get()->RuntimeClip(0),.5,AR::TimeMode::Clamp,pose),"rotation samples");
    Near(pose.localTransforms[1].rotation[2],std::sin(float(3.141592653589793/8)),"spherical linear quaternion quarter time");
}
static void Character(const char* path)
{
    std::ifstream file(path,std::ios::binary);Check(bool(file),"character fixture opens");
    std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});
    auto result=GetGlTFAssetProvider().Load(path,{reinterpret_cast<const std::byte*>(bytes.data()),bytes.size()});Check(bool(result),result.diagnostic.c_str());
    bytes.clear();bytes.shrink_to_fit();
    const auto asset=result.asset;const auto model=asset.Model(0);const auto* skeleton=asset.Get()->RuntimeSkeleton(0);
    Check(model.Get()->renderable&&model.Get()->meshes.size()==2&&skeleton&&skeleton->Bones().size()==21,"real articulated two-primitive character");
    Check(asset.AnimationCount()==6&&model.Get()->animations.size()==6,"six runtime clips");
    Check(model.Get()->materials[0].embeddedImages[0]==model.Get()->materials[1].embeddedImages[0],"shared texture ownership");
    const auto hand=ResolveAttachment(*model.Get()->skeleton,12,AttachmentKind::Weapon);Check(bool(hand),"right hand attachment binding");
    std::vector<std::unique_ptr<AnimationInstance>> actors;
    const auto lifetime=AR::GetLifetimeCounts();Check(lifetime.skeletons==1&&lifetime.clips==6,"one skeleton and six shared clips");
    for(int i=0;i<20;++i) {
        auto actor=asset.Get()->CreateAnimationInstance(model);Check(bool(actor),"actor instance");
        const int clip=i%6;
        Check(actor->SetMotion(asset.Animation(clip),0,0,clip<3?0:1,1)==AssetError::None,"motion ID and loop mapping");
        actor->SetClock(float(i)*.031f);Check(actor->Evaluate({}).pose.Valid(),"independent instance pose");actors.push_back(std::move(actor));
    }
    Check(actors[0]->BoneWorldMatrix(0).data()!=actors[1]->BoneWorldMatrix(0).data(),"poses owned per actor");
    auto bind=asset.Get()->CreateAnimationInstance(model);Check(bind->Evaluate({}).pose.Valid(),"bind pose");
    for(std::size_t b=0;b<19;++b) for(unsigned k=0;k<16;++k) Near(bind->CompositePose().values[b*16+k],AR::IdentityMatrix()[k],"inverse bind times bind model is identity");
    for(const auto& mesh:model.Get()->meshes) {
        Check(mesh.skin.meshToSkeleton.size()==19&&mesh.skin.influencesPerVertex==4,"same indexed skinning contract");
        std::vector<std::byte> weighted(mesh.vertexCount*40);
        const auto index=std::size_t(&mesh-model.Get()->meshes.data());
        Check(asset.Get()->CopyVertices(0,index,VertexLayout::WeightedPositionNormalUV,weighted)==AssetError::None,"production weighted stream");
        for(std::size_t v=0;v<mesh.vertexCount;++v) {
            unsigned sum=0;for(unsigned k=0;k<4;++k) sum+=std::to_integer<unsigned>(weighted[v*40+12+k]);Check(sum==255,"exact byte normalization");
        }
    }
    for(int clip=0;clip<6;++clip) for(float speed:{.5f,1.f,2.f}) {
        auto& actor=*actors[0];const auto* runtime=asset.Get()->RuntimeClip(clip);Check(runtime&&!runtime->Tracks().empty()&&!runtime->Looping(),"owned runtime tracks; loop policy external");
        Check(actor.SetMotion(asset.Animation(clip),0,0,clip<3?0:1,speed)==AssetError::None,"speed set");
        actor.SetClock(float(runtime->Duration())*.4f/speed);Check(actor.Evaluate({}).pose.Valid(),"speed pose");
        AR::AnimationPose pose;pose.Prepare(21);std::vector<AR::Matrix> matrices(21),palette(21);
        Check(AR::Sample(*skeleton,*runtime,runtime->Duration()*.4,AR::TimeMode::Clamp,pose)&&AR::Evaluate(*skeleton,pose,matrices)&&AR::BuildPalette(*skeleton,matrices,palette),"same core sampling math");
        for(unsigned b=0;b<21;++b) for(unsigned k=0;k<16;++k) Near(actor.CompositePose().values[b*16+k],palette[b][k],"playback speed parity");
        actor.SetClock(float(runtime->Duration())*2/speed);Check(actor.Evaluate({}).pose.Valid(),"clip boundary");
        Check(actor.IsPlaying()==(clip<3),"one-shot death/damage/attack stop; locomotion loops");
    }
    auto& actor=*actors[0];float clock=0;
    for(int clip:{0,1,2,0,3,0,4,5}) {
        actor.SetClock(clock);Check(actor.Evaluate({}).pose.Valid(),"before transition");
        const std::vector<float> before(actor.CompositePose().values.begin(),actor.CompositePose().values.end());
        Check(actor.SetMotion(asset.Animation(clip),clock,.2f,clip<3?0:1,1)==AssetError::None,"crossfade control");
        Check(actor.Evaluate({}).pose.Valid(),"crossfade starts");
        for(unsigned k=0;k<before.size();++k) Near(actor.CompositePose().values[k],before[k],"crossfade has no start jump");
        clock+=.3f;actor.SetClock(clock);Check(actor.Evaluate({}).pose.Valid(),"crossfade finishes");actor.FreeCompletedControls();
    }
    auto world=AR::IdentityMatrix();actor.UpdateTransform(1,world);Check(world==AR::IdentityMatrix(),"root motion never drives gameplay");
    const auto start=std::chrono::steady_clock::now();
    for(int frame=0;frame<120;++frame) for(int i=0;i<20;++i) {actors[i]->SetClock(frame/60.f+i*.03f);Check(actors[i]->Evaluate({}).pose.Valid(),"warm pose");}
    const auto end=std::chrono::steady_clock::now();
    Check(AR::GetLifetimeCounts().skeletons==lifetime.skeletons&&AR::GetLifetimeCounts().clips==lifetime.clips,"no clip/skeleton construction for 20 instances or warm frames");
    asset.ReleaseUploadData();Check(actors[1]->Evaluate({}).pose.Valid(),"clips survive upload release");
    std::cout<<"Warm 2400 poses: "<<std::chrono::duration<double,std::milli>(end-start).count()<<" ms; shared skeletons=1 clips=6 actors=20\n";
}
int main(int argc,char** argv)
{
    try {Check(argc==2,"fixture path required");Errors();Conversion();Character(argv[1]);
        Check(liveDocuments==0&&liveAnimationInstances==0&&liveMeshBindings==0&&AR::GetLifetimeCounts().skeletons==0&&AR::GetLifetimeCounts().clips==0,"all asset and animation owners released");
        std::cout<<"PASS F5-X skeleton, clips, weights, transforms, playback, attachments, sharing, errors, resources=0\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
