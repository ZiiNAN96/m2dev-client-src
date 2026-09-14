// Inspect invalid source channels without accepting them into the runtime.
// Constant-channel evidence alone does not establish motion compatibility.
#include "AssetRuntime/GR2/GR2Reader.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>

namespace GR=AssetRuntime::GR2;
static std::vector<std::byte> Bytes(const std::filesystem::path& path)
{
    std::ifstream stream(path,std::ios::binary|std::ios::ate);GR::Require(bool(stream),"open");const auto size=stream.tellg();
    GR::Require(size>0&&size<=std::streamoff(GR::MaximumFileBytes),"size");std::vector<std::byte> result(static_cast<std::size_t>(size));
    stream.seekg(0);stream.read(reinterpret_cast<char*>(result.data()),size);GR::Require(bool(stream),"read");return result;
}
struct Invalid {std::string group,track;unsigned channel,nonfinite,total;};
int main(int argc,char**argv)
{
    try {
        GR::Require(argc==4,"broken-clip donor-directory output.tsv");GR::File file(Bytes(argv[1]));GR::Types types(file);std::vector<Invalid> invalid;
        const std::array<const char*,3> channels{"PositionCurve","OrientationCurve","ScaleShearCurve"};
        for(auto animation:types.Array(types.Root(),"Animations")) for(auto group:types.Array(animation,"TrackGroups")) for(auto track:types.Array(group,"TransformTracks")) for(unsigned c=0;c<3;++c) {
            auto curve=types.Child(track,channels[c]);if(types.Find(curve,"CurveData")) curve=types.Child(curve,"CurveData");
            const auto controls=types.Array(curve,"Controls");unsigned nonfinite=0;
            for(auto control:controls) {GR::Require(types.Get(control.type).size==4,"scalar control required");if(!std::isfinite(std::bit_cast<float>(file.Uint(control.data)))) ++nonfinite;}
            if(nonfinite) invalid.push_back({types.Text(group,"Name"),types.Text(track,"Name"),c,nonfinite,static_cast<unsigned>(controls.size())});
        }
        std::ofstream report(argv[3]);GR::Require(bool(report),"report");report<<std::setprecision(9);
        report<<"donor\tgroup\ttrack\tchannel\tnonfinite_source\tsource_controls\tdonor_degree\tdonor_knots\texact_constant\tvalue\n";
        std::ofstream preserved(std::string(argv[3])+".preserved.tsv");GR::Require(bool(preserved),"preserved report");
        preserved<<std::setprecision(9)<<"file\tgroup\ttrack\tchannel\tdegree\tknots\tmax_component_spread\tfirst_control\n";
        const auto preservedChannels=[&](const std::string& name,GR::Types& t) {
            for(auto animation:t.Array(t.Root(),"Animations")) for(auto group:t.Array(animation,"TrackGroups")) for(auto track:t.Array(group,"TransformTracks")) {
                const auto target=t.Text(track,"Name");
                if(target.find("Finger0")==std::string::npos&&target.find("Finger1")==std::string::npos) continue;
                for(unsigned c=0;c<3;++c) {
                    auto curve=t.Child(track,channels[c]);if(t.Find(curve,"CurveData")) curve=t.Child(curve,"CurveData");
                    GR::Require(t.Find(curve,"Degree"),"preserved probe requires legacy float curve");
                    const unsigned dimension=c==0?3:c==1?4:9;const auto controls=t.Reals(curve,"Controls");
                    GR::Require(controls.size()%dimension==0,"preserved dimensions");
                    std::vector<float> first(dimension,0);if(c==1)first[3]=1;if(c==2)first[0]=first[4]=first[8]=1;
                    if(!controls.empty())std::copy_n(controls.begin(),dimension,first.begin());
                    float spread=0;for(unsigned axis=0;axis<dimension;++axis) {
                        float lo=first[axis],hi=lo;for(std::size_t i=axis;i<controls.size();i+=dimension){lo=std::min(lo,controls[i]);hi=std::max(hi,controls[i]);}spread=std::max(spread,hi-lo);
                    }
                    preserved<<name<<'\t'<<t.Text(group,"Name")<<'\t'<<target<<'\t'<<channels[c]<<'\t'<<t.Integer(curve,"Degree")<<'\t'<<t.Reals(curve,"Knots").size()<<'\t'<<spread<<'\t';
                    for(float x:first)preserved<<x<<' ';preserved<<'\n';
                }
            }
        };
        preservedChannels(std::filesystem::path(argv[1]).filename().string(),types);
        std::cout<<"INVALID_CHANNELS "<<invalid.size()<<'\n';
        for(const auto& item:invalid) std::cout<<item.track<<' '<<channels[item.channel]<<" nonfinite="<<item.nonfinite<<" total="<<item.total<<'\n';
        for(const auto& entry:std::filesystem::directory_iterator(argv[2])) if(entry.path().extension()==".gr2") {
            try {
                GR::File donorFile(Bytes(entry.path()));const auto donor=GR::Read(donorFile);if(donor.animations.empty()) continue;
                GR::Types donorTypes(donorFile);preservedChannels(entry.path().filename().string(),donorTypes);
                GR::Require(donor.animationData.size()==1,"donor animation count");std::size_t constantCount=0;
                for(const auto& item:invalid) {
                    const auto group=std::find_if(donor.animationData[0].groups.begin(),donor.animationData[0].groups.end(),[&](const auto& g){return g.name==item.group;});
                    GR::Require(group!=donor.animationData[0].groups.end(),"donor group missing");const auto* track=GR::FindTransformTrack(*group,item.track);GR::Require(track!=nullptr,"donor target missing");
                    const auto& curve=item.channel==0?track->translation:item.channel==1?track->rotation:track->scale;
                    std::vector<float> value(curve.dimension,0);bool constant=true;
                    if(curve.controls.empty()) {if(curve.dimension==4)value[3]=1;if(curve.dimension==9)value[0]=value[4]=value[8]=1;}
                    else {std::copy_n(curve.controls.begin(),curve.dimension,value.begin());for(std::size_t i=0;i<curve.controls.size();++i) constant&=curve.controls[i]==value[i%curve.dimension];}
                    constantCount+=constant;
                    report<<entry.path().filename().string()<<'\t'<<item.group<<'\t'<<item.track<<'\t'<<channels[item.channel]<<'\t'<<item.nonfinite<<'\t'<<item.total<<'\t'<<curve.degree<<'\t'<<curve.knots.size()<<'\t'<<constant<<'\t';
                    for(float x:value) report<<x<<' ';report<<'\n';
                }
                std::cout<<"DONOR "<<entry.path().filename().string()<<" exact_constant="<<constantCount<<'/'<<invalid.size()<<'\n';
            } catch(const std::exception& e) {std::cout<<"REJECT "<<entry.path().filename().string()<<' '<<e.what()<<'\n';}
        }
        report.flush();preserved.flush();GR::Require(bool(report)&&bool(preserved),"write");return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
