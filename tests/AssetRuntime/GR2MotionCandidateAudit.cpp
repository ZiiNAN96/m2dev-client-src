// Generic, SDK-free comparison of an authored motion and its source skeleton
// against a proposed destination skeleton. This tool never repairs assets.
#include "AssetRuntime/GR2/GR2Reader.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <sstream>

namespace GR=AssetRuntime::GR2;
namespace AR=AnimationRuntime;
namespace fs=std::filesystem;
static GR::Contents Read(const fs::path& path)
{
    std::ifstream input(path,std::ios::binary|std::ios::ate);GR::Require(bool(input),"missing file");
    const auto size=input.tellg();GR::Require(size>0&&size<=std::streamoff(GR::MaximumFileBytes),"file size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),size);
    GR::Require(bool(input),"file read");GR::File file(bytes);return GR::Read(file);
}
static void SkeletonInventory(const fs::path& root,const AssetRuntime::SkeletonAsset& target,const fs::path& output)
{
    std::ofstream report(output);GR::Require(bool(report),"skeleton report");
    report<<"file\tmodel\tbones\tnames_order\tparents\tbind_exact\troots\tstatus\n";
    std::size_t files=0,models=0,rejected=0,matching=0;
    for(const auto& entry:fs::recursive_directory_iterator(root)) if(entry.is_regular_file()&&entry.path().extension()==".gr2") {
        ++files;const auto path=fs::relative(entry.path(),root).generic_string();
        try {
            const auto content=Read(entry.path());
            for(const auto& model:content.models) if(model.skeleton) {
                ++models;const auto& bones=model.skeleton->bones;
                bool names=bones.size()==target.bones.size(),parents=names,bind=names;
                std::ostringstream roots;
                for(std::size_t i=0;i<bones.size();++i) {
                    const auto& a=bones[i];if(a.parentIndex<0) roots<<i<<':'<<a.name<<';';
                    if(i>=target.bones.size()) continue;const auto& b=target.bones[i];
                    names&=a.name==b.name;parents&=a.parentIndex==b.parentIndex;
                    bind&=a.localBind.flags==b.localBind.flags&&a.localBind.position==b.localBind.position&&a.localBind.orientation==b.localBind.orientation&&a.localBind.scaleShear==b.localBind.scaleShear&&a.inverseBind==b.inverseBind;
                }
                matching+=names&&parents;
                report<<path<<'\t'<<model.name<<'\t'<<bones.size()<<'\t'<<names<<'\t'<<parents<<'\t'<<bind<<'\t'<<roots.str()<<"\tparsed\n";
            }
        } catch(const std::exception& e) {++rejected;report<<path<<"\tNA\tNA\t0\t0\t0\tNA\t"<<e.what()<<'\n';}
    }
    report.flush();GR::Require(bool(report),"write skeleton report");
    std::cout<<"INVENTORY files="<<files<<" models="<<models<<" rejected="<<rejected<<" matching_structure="<<matching<<'\n';
}
int main(int argc,char**argv)
{
    try {
        GR::Require(argc==5,"asset-root target-model candidates.tsv|--skeletons output.tsv required");
        const fs::path root=argv[1];auto target=Read(root/argv[2]);GR::Require(target.models.size()==1&&target.modelData[0].skeleton,"target model");
        const auto& destination=*target.models[0].skeleton;
        if(std::string_view(argv[3])=="--skeletons") {SkeletonInventory(root,destination,argv[4]);return 0;}
        std::cout<<"TARGET "<<argv[2]<<" model="<<target.models[0].name<<" bones="<<destination.bones.size()<<'\n';
        for(const auto& bone:destination.bones) std::cout<<"BONE "<<bone.id<<' '<<bone.parentIndex<<' '<<std::quoted(bone.name)<<'\n';
        std::ifstream manifest(argv[3]);std::ofstream report(argv[4]);GR::Require(bool(manifest)&&bool(report),"audit files");
        report<<std::setprecision(9)<<"model\tclip\tbones\tnames_order\tparents\tbind_exact\troots\tduration\tgroups\ttracks\tmatched_bones\tloop_translation\tperiodic\tfinite_clamp_poses\tstatus\tdiagnostic\n";
        std::string line;std::map<std::string,GR::Contents> models;
        while(std::getline(manifest,line)) {
            if(!line.empty()&&line.back()=='\r') line.pop_back();if(line.empty()) continue;
            const auto separator=line.find('\t');GR::Require(separator!=std::string::npos,"manifest columns");
            const auto modelPath=line.substr(0,separator),clipPath=line.substr(separator+1);
            report<<modelPath<<'\t'<<clipPath;
            try {
                auto found=models.find(modelPath);if(found==models.end()) found=models.emplace(modelPath,Read(root/modelPath)).first;
                const auto& model=found->second;GR::Require(model.models.size()==1&&model.models[0].skeleton,"candidate model");
                const auto& bones=model.models[0].skeleton->bones;
                bool names=bones.size()==destination.bones.size(),parents=names,bind=names;
                for(std::size_t i=0;i<std::min(bones.size(),destination.bones.size());++i) {
                    const auto& a=bones[i];const auto& b=destination.bones[i];
                    names&=a.name==b.name;parents&=a.parentIndex==b.parentIndex;
                    bind&=a.localBind.flags==b.localBind.flags&&a.localBind.position==b.localBind.position&&a.localBind.orientation==b.localBind.orientation&&a.localBind.scaleShear==b.localBind.scaleShear&&a.inverseBind==b.inverseBind;
                }
                report<<'\t'<<bones.size()<<'\t'<<names<<'\t'<<parents<<'\t'<<bind<<'\t';
                for(std::size_t i=0;i<bones.size();++i) if(bones[i].parentIndex<0) report<<i<<':'<<bones[i].name<<';';
                try {
                    std::ostringstream metadata;metadata<<std::setprecision(9);
                    auto clip=Read(root/clipPath);GR::Require(clip.animations.size()==1,"one clip required");
                    const auto& animation=clip.animations[0];const GR::TrackGroup* group=nullptr;
                    for(const auto& g:clip.animationData[0].groups) if(g.name==target.models[0].name) {GR::Require(!group,"ambiguous group");group=&g;}
                    GR::Require(group!=nullptr,"no matching model group");
                    std::size_t matched=0;for(const auto& bone:destination.bones) if(GR::FindTransformTrack(*group,bone.name)) ++matched;
                    metadata<<'\t'<<animation.duration<<'\t'<<animation.trackGroupCount<<'\t'<<group->tracks.size()<<'\t'<<matched<<'\t'<<group->loopTranslation[0]<<' '<<group->loopTranslation[1]<<' '<<group->loopTranslation[2]<<'\t'<<bool(group->periodicLoop);
                    std::string error;auto bound=GR::BindAnimation(animation,clip.animationData[0],*target.modelData[0].skeleton,error,0,target.models[0].name);
                    GR::Require(bool(bound),error);AR::AnimationPose pose;pose.Prepare(destination.bones.size());std::vector<AR::Matrix> world(destination.bones.size()),palette(world.size());
                    for(double fraction:{0.,.25,.5,.75,1.}) GR::Require(AR::Sample(*target.modelData[0].skeleton,*bound,animation.duration*fraction,AR::TimeMode::Clamp,pose)&&AR::Evaluate(*target.modelData[0].skeleton,pose,world)&&AR::BuildPalette(*target.modelData[0].skeleton,world,palette),"nonfinite/evaluation failure");
                    report<<metadata.str()<<"\t5\t"<<(names&&parents?"compatible structure":"different skeleton")<<"\t\n";
                } catch(const std::exception& e) {report<<"\tNA\tNA\tNA\tNA\tNA\tNA\t0\trejected\t"<<e.what()<<'\n';}
            } catch(const std::exception& e) { report<<"\tNA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\tNA\t0\trejected model\t"<<e.what()<<'\n'; }
        }
        report.flush();GR::Require(bool(report),"report write");return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
