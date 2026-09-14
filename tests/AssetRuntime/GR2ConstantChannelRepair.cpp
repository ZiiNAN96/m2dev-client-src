// Offline asset authoring utility. Copies explicitly selected, already authored
// constant channels; never relaxes runtime validation or substitutes a backend.
#include "AssetRuntime/GR2/GR2Reader.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace GR=AssetRuntime::GR2;
namespace fs=std::filesystem;
static std::vector<std::byte> Read(const fs::path& path)
{
    std::ifstream stream(path,std::ios::binary|std::ios::ate);GR::Require(bool(stream),"open input");
    const auto size=stream.tellg();GR::Require(size>0&&size<=std::streamoff(GR::MaximumFileBytes),"input size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));stream.seekg(0);stream.read(reinterpret_cast<char*>(bytes.data()),size);
    GR::Require(bool(stream),"read input");return bytes;
}
static void Put(std::vector<std::byte>& bytes,std::size_t offset,std::uint32_t value)
{
    GR::Range(offset,4,bytes.size());for(unsigned i=0;i<4;++i)bytes[offset+i]=std::byte((value>>(i*8))&255);
}
static std::uint32_t Crc(std::span<const std::byte> bytes)
{
    std::uint32_t crc=0xffffffffu;for(auto b:bytes){crc^=std::to_integer<unsigned>(b);for(unsigned i=0;i<8;++i)crc=(crc>>1)^((crc&1)?0xedb88320u:0);}return crc^0xffffffffu;
}
int main(int argc,char**argv)
{
    try {
        GR::Require(argc==5,"broken.gr2 donor.gr2 selected-channels.tsv new-output.gr2 required");
        const fs::path output=argv[4];GR::Require(!fs::exists(output),"output must be new; source overwrite forbidden");
        const auto original=Read(argv[1]);GR::File source(original);GR::Types types(source);
        GR::File donorFile(Read(argv[2]));const auto donor=GR::Read(donorFile);
        GR::Require(donor.animations.size()==1,"one donor animation required");
        const auto animations=types.Array(types.Root(),"Animations");GR::Require(animations.size()==1,"one source animation required");
        std::vector<std::vector<std::byte>> sections;
        for(std::uint32_t i=0;i<source.header.sections.size();++i){const auto bytes=source.Bytes({i,0},source.header.sections[i].expanded);sections.emplace_back(bytes.begin(),bytes.end());}
        std::map<GR::Ref,std::uint32_t> replacements;std::set<std::string> selected;
        std::ifstream plan(argv[3]);GR::Require(bool(plan),"open selection");std::string line;
        while(std::getline(plan,line)) {
            if(!line.empty()&&line.back()=='\r')line.pop_back();if(line.empty())continue;
            GR::Require(selected.insert(line).second,"duplicate channel selection");
            std::istringstream row(line);std::string groupName,trackName,channel,extra;
            GR::Require(bool(std::getline(row,groupName,'\t'))&&bool(std::getline(row,trackName,'\t'))&&bool(std::getline(row,channel))&&channel.find('\t')==std::string::npos,"selection columns");
            const auto donorGroup=std::find_if(donor.animationData[0].groups.begin(),donor.animationData[0].groups.end(),[&](const auto& g){return g.name==groupName;});
            GR::Require(donorGroup!=donor.animationData[0].groups.end(),"donor group");
            const auto* donorTrack=GR::FindTransformTrack(*donorGroup,trackName);GR::Require(donorTrack,"donor track");
            GR::Require(channel=="PositionCurve"||channel=="OrientationCurve"||channel=="ScaleShearCurve","unsupported channel");
            const auto& curve=channel=="PositionCurve"?donorTrack->translation:channel=="OrientationCurve"?donorTrack->rotation:donorTrack->scale;
            std::vector<float> value(curve.dimension,0);if(curve.dimension==4)value[3]=1;if(curve.dimension==9)value[0]=value[4]=value[8]=1;
            if(!curve.controls.empty())std::copy_n(curve.controls.begin(),curve.dimension,value.begin());
            for(std::size_t i=0;i<curve.controls.size();++i)GR::Require(curve.controls[i]==value[i%curve.dimension],"donor must be exactly constant across all controls");
            std::size_t found=0;
            for(auto group:types.Array(animations[0],"TrackGroups"))if(types.Text(group,"Name")==groupName)
                for(auto track:types.Array(group,"TransformTracks"))if(types.Text(track,"Name")==trackName) {
                    ++found;auto srcCurve=types.Child(track,channel);if(types.Find(srcCurve,"CurveData"))srcCurve=types.Child(srcCurve,"CurveData");
                    GR::Require(types.Find(srcCurve,"Degree"),"repair supports audited legacy float curves only");
                    const auto controls=types.Array(srcCurve,"Controls");GR::Require(!controls.empty()&&controls.size()%curve.dimension==0,"source controls");
                    for(std::size_t i=0;i<controls.size();++i) {
                        const auto at=controls[i].data;GR::Require(types.Get(controls[i].type).size==4,"scalar float control");
                        const auto old=source.Uint(at),replacement=std::bit_cast<std::uint32_t>(value[i%curve.dimension]);
                        GR::Require(!std::isfinite(std::bit_cast<float>(old)),"source channel is not entirely corrupt; refusing to overwrite valid values");
                        GR::Require(replacements.emplace(at,replacement).second,"overlapping selected controls");
                        Put(sections.at(at.section),at.offset,replacement);
                        std::cout<<"PATCH "<<at.section<<':'<<at.offset<<" old="<<std::hex<<old<<" new="<<replacement<<std::dec<<' '<<trackName<<' '<<channel<<'\n';
                    }
                }
            GR::Require(found==1,"source group/track must be unique");
        }
        GR::Require(!selected.empty(),"empty selection");
        // Keep logical section data, references, type definitions and metadata.
        // Only physical storage changes to standard uncompressed GR2 sections.
        std::vector<std::byte> bytes(original.begin(),original.begin()+source.header.headerSize);
        const auto append=[&](std::span<const std::byte> data,std::size_t alignment) {
            const auto offset=(bytes.size()+alignment-1)&~(alignment-1);GR::Range(offset,data.size(),GR::MaximumFileBytes);
            bytes.resize(offset);bytes.insert(bytes.end(),data.begin(),data.end());return static_cast<std::uint32_t>(offset);
        };
        const std::size_t table=32+(source.header.version==6?56:72);
        for(std::size_t i=0;i<sections.size();++i) {
            const auto& s=source.header.sections[i];const auto at=table+i*44;
            Put(bytes,at,0);Put(bytes,at+4,append(sections[i],s.alignment));Put(bytes,at+8,s.expanded);
            Put(bytes,at+28,append(std::span(original).subspan(s.fixupOffset,GR::Product(s.fixupCount,12)),4));
            Put(bytes,at+36,append(std::span(original).subspan(s.marshalOffset,GR::Product(s.marshalCount,16)),4));
        }
        Put(bytes,36,static_cast<std::uint32_t>(bytes.size()));Put(bytes,40,Crc(std::span(bytes).subspan(table)));
        GR::File verified(bytes);const auto repaired=GR::Read(verified);GR::Require(repaired.animations.size()==1,"repaired animation");
        for(std::uint32_t i=0;i<sections.size();++i) {
            GR::Require(std::ranges::equal(sections[i],verified.Bytes({i,0},sections[i].size())),"section round trip");
            const auto old=source.Bytes({i,0},sections[i].size());
            for(std::size_t j=0;j<old.size();++j)if(old[j]!=sections[i][j]) {
                bool planned=false;for(unsigned k=0;k<4&&k<=j;++k)planned|=replacements.contains({i,static_cast<std::uint32_t>(j-k)});
                GR::Require(planned,"unexpected logical data modification");
            }
        }
        std::ofstream stream(output,std::ios::binary);GR::Require(bool(stream),"create output");stream.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());stream.flush();GR::Require(bool(stream),"write output");
        std::cout<<"PASS channels="<<selected.size()<<" scalar_replacements="<<replacements.size()<<" sections="<<sections.size()<<" bytes="<<bytes.size()<<" other_logical_bytes_unchanged=1 native_read=1\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
