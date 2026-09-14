// Real-asset regression for the authored F3-B repair, without the Granny SDK.
#include "AssetRuntime/GR2/GR2Reader.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace GR=AssetRuntime::GR2;
namespace AR=AnimationRuntime;
namespace fs=std::filesystem;
static GR::Contents Read(const fs::path& path)
{
    std::ifstream file(path,std::ios::binary|std::ios::ate);GR::Require(bool(file),"asset missing");
    const auto size=file.tellg();GR::Require(size>0&&size<=std::streamoff(GR::MaximumFileBytes),"asset size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),size);GR::Require(bool(file),"asset read");
    GR::File decoded(bytes);return GR::Read(decoded);
}
int main(int argc,char**argv)
{
    try {
        GR::Require(argc==2,"asset root required");const fs::path root=fs::path(argv[1])/"metin2_patch_dragon_rock_mobs/ymir work/monster2";
        {
            const auto clip=Read(root/"redthief_general/back_damage.gr2"),donor=Read(root/"redthief_general/normal_attack1.gr2");
            GR::Require(clip.animations.size()==1&&clip.animations[0].duration==double(0.666666687f)&&clip.animationData[0].groups.size()==1,"hit metadata");
            const auto& group=clip.animationData[0].groups[0];GR::Require(group.name=="Bip01"&&group.tracks.size()==92&&!group.periodicLoop&&group.loopTranslation==std::array<float,3>{},"hit targets/root motion");
            unsigned repaired=0;
            for(const char* side:{"L","R"})for(const char* suffix:{"2","21","22","2Nub"}) {
                const std::string name=std::string("Bip01 ")+side+" Finger"+suffix;
                const auto* track=GR::FindTransformTrack(group,name);const auto* authored=GR::FindTransformTrack(donor.animationData[0].groups[0],name);GR::Require(track&&authored,"authored finger targets");
                const std::array<const GR::Curve*,3> curves{&track->translation,&track->rotation,&track->scale},values{&authored->translation,&authored->rotation,&authored->scale};
                for(unsigned c=0;c<3;++c) {
                    const auto& source=*values[c];const auto& fixed=*curves[c];std::vector<float> value(source.dimension,0);if(c==1)value[3]=1;if(c==2)value[0]=value[4]=value[8]=1;
                    if(!source.controls.empty())std::copy_n(source.controls.begin(),source.dimension,value.begin());
                    for(std::size_t i=0;i<source.controls.size();++i)GR::Require(source.controls[i]==value[i%source.dimension],"donor changed to animated channel");
                    GR::Require(fixed.controls.size()==(c==0?9u:c==1?12u:18u),"source control count changed");
                    for(std::size_t i=0;i<fixed.controls.size();++i)GR::Require(fixed.controls[i]==value[i%source.dimension],"repaired channel differs from authored donor");
                    ++repaired;
                }
            }
            for(const char* race:{"redthief_general","redthief2_general"})for(const char* lod:{"","_lod_01"}) {
                const auto model=Read(root/race/(std::string(race)+lod+".gr2"));GR::Require(model.models.size()==1&&model.modelData[0].skeleton,"model skeleton");
                const auto& skeleton=*model.modelData[0].skeleton;const std::size_t expected=std::string_view(race)=="redthief_general"?91:71;
                GR::Require(skeleton.Bones().size()==expected,"production skeleton changed");
                std::size_t bound=0;for(const auto& bone:skeleton.Bones())bound+=GR::FindTransformTrack(group,bone.name)!=nullptr;GR::Require(bound==expected-2,"production track binding changed");
                std::string error;const auto runtime=GR::BindAnimation(clip.animations[0],clip.animationData[0],skeleton,error,0,model.models[0].name);GR::Require(bool(runtime),error);
                AR::AnimationPose pose;pose.Prepare(expected);std::vector<AR::Matrix> world(expected),palette(expected);
                for(double fraction:{0.,.25,.5,.75,1.})GR::Require(AR::Sample(skeleton,*runtime,clip.animations[0].duration*fraction,AR::TimeMode::Clamp,pose)&&AR::Evaluate(skeleton,pose,world)&&AR::BuildPalette(skeleton,world,palette),"nonfinite pose/palette");
                std::cout<<"PASS model="<<race<<lod<<" bones="<<expected<<" bound="<<bound<<" finite_clamp_samples=5\n";
            }
            GR::Require(repaired==24,"repair coverage");
        }
        GR::Require(AR::GetLifetimeCounts().clips==0&&AR::GetLifetimeCounts().skeletons==0,"runtime lifetime leak");return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
