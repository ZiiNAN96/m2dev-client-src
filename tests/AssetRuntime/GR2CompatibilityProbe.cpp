// Read-only F3-A field inspection. The probe reports unusual source values;
// accepting them remains the reader's responsibility, with separate parity.
#include "EterGrnLib/StdAfx.h"
#include "AssetRuntime/GR2/GR2Reader.h"
#include "AssetRuntime/Granny/Native.h"
#include <filesystem>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <set>
#include <cmath>

using namespace AssetRuntime::GR2;
static std::string Address(Ref r) { return std::to_string(r.section)+":"+std::to_string(r.offset); }
static void Reference(const std::filesystem::path& modelPath,const std::filesystem::path& clipPath)
{
    if(!std::filesystem::exists(modelPath)) { std::cout<<"REFERENCE_MODEL_MISSING "<<modelPath.filename().string()<<'\n';return; }
    auto* mf=GrannyReadEntireFile(modelPath.string().c_str());auto* cf=GrannyReadEntireFile(clipPath.string().c_str());
    Require(mf&&cf,"reference read");
    auto* mi=GrannyGetFileInfo(mf);auto* ci=GrannyGetFileInfo(cf);
    if(mi->ModelCount && ci->AnimationCount) {
        auto* m=mi->Models[0];auto* a=ci->Animations[0];auto* instance=GrannyInstantiateModel(m);
        auto* pose=GrannyNewLocalPose(m->Skeleton->BoneCount);auto* world=GrannyNewWorldPose(m->Skeleton->BoneCount);
        auto* control=GrannyPlayControlledAnimation(0,a,instance);GrannySetControlLoopCount(control,0);
        GrannySetControlEaseIn(control,false);GrannySetControlEaseOut(control,false);
        std::cout<<"REFERENCE model="<<m->Name<<" duration="<<a->Duration<<'\n';
        int groupIndex=-1;GrannyFindTrackGroupForModel(a,m->Name,&groupIndex);
        if(groupIndex>=0) {
            auto* group=a->TrackGroups[groupIndex];
            std::cout<<"REF_GROUP "<<groupIndex<<" loop="<<group->LoopTranslation[0]<<' '<<group->LoopTranslation[1]<<' '<<group->LoopTranslation[2]<<'\n';
            std::map<std::string,int> names;
            for(int b=0;b<m->Skeleton->BoneCount;++b) {
                auto name=m->Skeleton->Bones[b].Name;int found=-1,track=-1;
                GrannyFindBoneByName(m->Skeleton,name,&found);GrannyFindTrackByName(group,name,&track);
                if(!names.emplace(name,b).second || found!=b)
                    std::cout<<"REF_BIND bone="<<b<<" name="<<std::quoted(name)<<" find_bone="<<found<<" track="<<track<<'\n';
            }
        }
        for(float fraction:{0.f,.25f,.5f,1.1f}) {
            GrannySetModelClock(instance,a->Duration*fraction);
            GrannySampleModelAnimationsAccelerated(instance,m->Skeleton->BoneCount,nullptr,pose,world);
            std::size_t nonfinite=0;
            for(int b=0;b<m->Skeleton->BoneCount;++b) {
                const auto* matrix=GrannyGetWorldPoseComposite4x4(world,b);
                if(std::any_of(matrix,matrix+16,[](float x){return !std::isfinite(x);})) { ++nonfinite;std::cout<<"REF_NAN_BONE "<<b<<' '<<m->Skeleton->Bones[b].Name<<'\n'; }
            }
            float translation[3],rotation[3];GrannyGetRootMotionVectors(instance,.1f,translation,rotation,false);
            std::cout<<"REF_SAMPLE fraction="<<fraction<<" nan_bones="<<nonfinite<<" translation="<<translation[0]<<' '<<translation[1]<<' '<<translation[2]<<" rotation="<<rotation[0]<<' '<<rotation[1]<<' '<<rotation[2]<<'\n';
        }
        GrannyFreeControl(control);GrannyFreeModelInstance(instance);GrannyFreeLocalPose(pose);GrannyFreeWorldPose(world);
    }
    GrannyFreeFile(mf);GrannyFreeFile(cf);
}
static void Floats(Types& t,Object object,const std::string& path,std::set<std::pair<Ref,Ref>>& visited,unsigned depth=0)
{
    Require(depth<64,"probe depth");
    if(!object || !visited.emplace(object.data,object.type).second) return;
    Require(visited.size()<2000000,"probe object limit");
    for(const auto& member:t.Get(object.type).members) {
        auto field=t.file.Add(object.data,member.offset);
        auto label=path+"."+member.name;
        if(member.kind==10 || member.kind==9) {
            auto count=member.kind==9?16u:std::max(1u,member.width);
            if(member.kind==9) field=t.file.Add(field,4);
            for(unsigned i=0;i<count;++i) {
                const auto at=t.file.Add(field,i*4); const auto bits=t.file.Uint(at);
                if(!std::isfinite(std::bit_cast<float>(bits)))
                    std::cout<<"NONFINITE "<<label<<"["<<i<<"] section:offset="<<Address(at)<<" bits="<<std::hex<<bits<<std::dec<<" type="<<Address(object.type)<<'\n';
            }
        } else if(member.kind==1||member.kind==2||member.kind==5) Floats(t,t.Child(object,member.name),label,visited,depth+1);
        else if(member.kind==3||member.kind==4||member.kind==7) {
            const auto values=t.Array(object,member.name);
            for(std::size_t i=0;i<values.size();++i) Floats(t,values[i],label+"["+std::to_string(i)+"]",visited,depth+1);
        }
    }
}
int main(int argc,char**argv)
{
    if(argc!=3) return 1;
    std::ifstream manifest(argv[2]); std::string line;
    while(std::getline(manifest,line)) {
        if(!line.starts_with("gr2\t")) continue;
        const auto path=line.substr(4,line.find('\t',4)-4);
        std::cout<<"FILE "<<path<<'\n';
        try {
            std::ifstream input(std::filesystem::path(argv[1])/path,std::ios::binary|std::ios::ate);
            Require(bool(input),"missing file"); const auto size=input.tellg();
            Require(size>0&&size<256*1024*1024,"size"); std::vector<std::byte> bytes(static_cast<std::size_t>(size));
            input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),size);Require(bool(input),"read");
            File file(bytes);Types t(file);auto root=t.Root();
            std::set<std::pair<Ref,Ref>> visited;
            try { Floats(t,root,"Root",visited); } catch(const std::exception& e) { std::cout<<"UNCONSUMED_METADATA "<<e.what()<<'\n'; }
            for(auto model:t.Array(root,"Models")) {
                if(auto skeleton=t.Child(model,"Skeleton")) {
                    auto bones=t.Array(skeleton,"Bones");std::map<std::string,std::vector<std::size_t>> names;
                    for(std::size_t i=0;i<bones.size();++i) names[t.Text(bones[i],"Name")].push_back(i);
                    std::cout<<"MODEL "<<t.Text(model,"Name")<<" bones="<<bones.size()<<'\n';
                    for(std::size_t i=0;i<bones.size();++i) {
                        auto name=t.Text(bones[i],"Name");auto parent=t.Integer(bones[i],"ParentIndex");
                        if(parent==-1||name.empty()||names[name].size()>1)
                            std::cout<<"BONE index="<<i<<" parent="<<parent<<" name="<<std::quoted(name)<<" section:offset="<<Address(bones[i].data)<<" type="<<Address(bones[i].type)<<'\n';
                    }
                }
                for(auto binding:t.Array(model,"MeshBindings")) {
                    auto mesh=t.Child(binding,"Mesh");auto data=t.Child(mesh,"PrimaryVertexData");auto topology=t.Child(mesh,"PrimaryTopology");
                    std::cout<<"MESH "<<t.Text(mesh,"Name")<<" vertex_count="<<t.Array(data,"Vertices").size()<<" indices="<<t.Array(topology,"Indices").size()<<" index16="<<t.Array(topology,"Indices16").size()<<" groups="<<t.Array(topology,"Groups").size()<<" section:offset="<<Address(mesh.data)<<" type="<<Address(mesh.type)<<'\n';
                }
            }
            for(auto animation:t.Array(root,"Animations")) for(auto group:t.Array(animation,"TrackGroups")) {
                std::cout<<"GROUP "<<t.Text(group,"Name")<<" flags="<<t.Integer(group,"AccumulationFlags")<<" section:offset="<<Address(group.data)<<" type="<<Address(group.type)<<'\n';
                for(auto track:t.Array(group,"TextTracks")) for(auto entry:t.Array(track,"Entries")) {
                    auto time=t.Real(entry,"TimeStamp");
                    if(time<0||time>t.Real(animation,"Duration")) std::cout<<"TEXT_OUTSIDE time="<<std::setprecision(9)<<time<<" duration="<<t.Real(animation,"Duration")<<" label="<<t.Text(entry,"Text")<<" section:offset="<<Address(entry.data)<<" type="<<Address(entry.type)<<'\n';
                }
                for(const char* field:{"PeriodicLoop","RootMotion"}) if(auto optional=t.Child(group,field)) {
                    std::cout<<field<<" section:offset="<<Address(optional.data)<<" type_address="<<Address(optional.type)<<" type="<<t.Describe(optional.type)<<'\n';
                    for(const auto& member:t.Get(optional.type).members) if(member.kind==10) {
                        std::cout<<"VALUE "<<member.name;for(auto value:t.Reals(optional,member.name))std::cout<<' '<<value;std::cout<<'\n';
                    }
                }
                std::map<std::string,std::size_t> names;std::size_t i=0;std::string last;
                for(auto track:t.Array(group,"TransformTracks")) {
                    auto name=t.Text(track,"Name");auto [first,added]=names.emplace(name,i);
                    if(name<last) std::cout<<"LEXICAL_INVERSION index="<<i<<" previous="<<std::quoted(last)<<" next="<<std::quoted(name)<<'\n';last=name;
                    if(path.find("redthief_general/")!=std::string::npos) std::cout<<"TRACK_NAME "<<i<<' '<<name<<'\n';
                    if(name.empty()||!added) std::cout<<"TRACK index="<<i<<" first="<<first->second<<" name="<<std::quoted(name)<<" section:offset="<<Address(track.data)<<" type="<<Address(track.type)<<'\n';
                    ++i;
                }
            }
            if(!t.Array(root,"Animations").empty()) {
                auto clipPath=std::filesystem::path(argv[1])/path;
                auto directory=clipPath.parent_path();auto name=directory.filename().string();
                auto modelPath=directory/(name=="ch_officer"?"skipia_officer.gr2":name+".gr2");
                if(name=="general") {directory=directory.parent_path();modelPath=directory/(directory.filename().string()+"_novice.gr2");}
                Reference(modelPath,clipPath);
            }
        } catch(const std::exception& error) { std::cout<<"ERROR "<<error.what()<<'\n'; }
    }
}
