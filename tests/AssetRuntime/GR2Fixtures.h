#pragma once
#include "AssetRuntime/GR2/GR2File.h"
#include <map>
#include <cstring>

namespace GR2Fixtures
{
using namespace AssetRuntime::GR2;
inline void Put(std::vector<std::byte>& bytes,std::size_t at,std::uint32_t value)
{ Range(at,4,bytes.size()); for(unsigned i=0;i<4;++i) bytes[at+i]=std::byte((value>>(i*8))&255); }
struct Field { std::uint32_t kind; const char* name; std::uint32_t type=0,width=0; };
struct Builder
{
    std::vector<std::byte> data;
    std::map<std::uint32_t,std::uint32_t> fixups,sizes;
    std::map<std::uint32_t,std::map<std::string,std::uint32_t>> fields;
    std::uint32_t rootType{},root{},model{},skeleton{},bone{},vertices{},indices{},curve{},track{};
    std::uint32_t Allocate(std::size_t count)
    { const auto at=static_cast<std::uint32_t>(data.size()); data.resize(data.size()+((count+3)&~std::size_t(3))); return at; }
    std::uint32_t String(std::string_view text)
    { const auto at=Allocate(text.size()+1); std::memcpy(data.data()+at,text.data(),text.size()); return at; }
    void Pointer(std::uint32_t source,std::uint32_t destination) { fixups[source]=destination; }
    void Value(std::uint32_t source,std::uint32_t value) { Put(data,source,value); }
    void Float(std::uint32_t source,float value) { Value(source,std::bit_cast<std::uint32_t>(value)); }
    std::uint32_t Type(std::initializer_list<Field> members)
    {
        const auto type=Allocate((members.size()+1)*32); std::uint32_t offset=0,index=0;
        for(const auto& member:members) {
            const auto at=type+index++*32; Value(at,member.kind); Pointer(at+4,String(member.name));
            if(member.type) Pointer(at+8,member.type); Value(at+12,member.width);
            fields[type][member.name]=offset;
            std::uint32_t size=4;
            if(member.kind==1) size=sizes.at(member.type);
            if(member.kind==3||member.kind==4||member.kind==5) size=8;
            if(member.kind==7) size=12;
            if(member.kind==9) size=68;
            if(member.kind==12||member.kind==14) size=1;
            offset+=size*std::max(1u,member.width);
        }
        sizes[type]=offset; return type;
    }
    std::uint32_t Object(std::uint32_t type) { return Allocate(sizes.at(type)); }
    void Name(std::uint32_t object,std::string_view name) { Pointer(object,String(name)); }
    void Array(std::uint32_t object,std::uint32_t offset,std::uint32_t count,std::uint32_t target)
    { Value(object+offset,count); if(count) Pointer(object+offset+4,target); }
    void Identity(std::uint32_t at)
    { Value(at,7); Float(at+28,1); Float(at+32,1); Float(at+48,1); Float(at+64,1); }
    Builder()
    {
        Allocate(4); // Zero remains the fixture's null type sentinel.
        const auto integer=Type({{19,"Int32"}}),real=Type({{10,"Real32"}});
        const auto boneType=Type({{8,"Name"},{19,"ParentIndex"},{9,"Transform"},{10,"InverseWorldTransform",0,16}});
        const auto skeletonType=Type({{8,"Name"},{3,"Bones",boneType}});
        const auto vertexType=Type({{10,"Position",0,3},{14,"BoneWeights",0,4},{12,"BoneIndices",0,4},{10,"Normal",0,3},{10,"TextureCoordinates0",0,2}});
        const auto vertexData=Type({{7,"Vertices"}});
        const auto groupType=Type({{19,"MaterialIndex"},{19,"TriFirst"},{19,"TriCount"}});
        const auto topologyType=Type({{3,"Groups",groupType},{3,"Indices",integer},{3,"Indices16",integer}});
        const auto materialType=Type({{8,"Name"}}),materialBinding=Type({{2,"Material",materialType}});
        const auto boneBinding=Type({{8,"BoneName"},{10,"OBBMin",0,3},{10,"OBBMax",0,3}});
        const auto meshType=Type({{8,"Name"},{2,"PrimaryVertexData",vertexData},{2,"PrimaryTopology",topologyType},{3,"MaterialBindings",materialBinding},{3,"BoneBindings",boneBinding}});
        const auto meshBinding=Type({{2,"Mesh",meshType}});
        const auto modelType=Type({{8,"Name"},{2,"Skeleton",skeletonType},{9,"InitialPlacement"},{3,"MeshBindings",meshBinding}});
        const auto curveType=Type({{19,"Degree"},{3,"Knots",real},{3,"Controls",real}});
        const auto trackType=Type({{8,"Name"},{1,"PositionCurve",curveType},{1,"OrientationCurve",curveType},{1,"ScaleShearCurve",curveType}});
        const auto trackGroupType=Type({{8,"Name"},{3,"TransformTracks",trackType},{9,"InitialPlacement"},{19,"AccumulationFlags"}});
        const auto animationType=Type({{8,"Name"},{10,"Duration"},{10,"TimeStep"},{4,"TrackGroups",trackGroupType}});
        rootType=Type({{4,"Models",modelType},{4,"Animations",animationType}}); root=Object(rootType);
        model=Object(modelType); Name(model,"root"); Identity(model+8); const auto modelArray=Allocate(4); Pointer(modelArray,model); Array(root,0,1,modelArray);
        skeleton=Object(skeletonType); Name(skeleton,"root"); Pointer(model+4,skeleton); bone=Object(boneType); Name(bone,"root"); Value(bone+4,UINT32_MAX); Identity(bone+8);
        for(unsigned i=0;i<4;++i) Float(bone+76+i*20,1); Array(skeleton,4,1,bone);
        const auto mesh=Object(meshType); Name(mesh,"triangle"); const auto binding=Object(meshBinding); Pointer(binding,mesh); Array(model,76,1,binding);
        const auto vdata=Object(vertexData); Pointer(mesh+4,vdata); vertices=Allocate(120); Pointer(vdata,vertexType); Value(vdata+4,3); Pointer(vdata+8,vertices);
        for(unsigned i=0;i<3;++i) { data[vertices+i*40+12]=std::byte(255); Float(vertices+i*40+28,1); }
        Float(vertices+40,1); Float(vertices+84,1);
        const auto topology=Object(topologyType); Pointer(mesh+8,topology); const auto group=Object(groupType); Value(group+8,1); Array(topology,0,1,group);
        indices=Allocate(12); Value(indices+4,1); Value(indices+8,2); Array(topology,8,3,indices);
        const auto material=Object(materialType); Name(material,"material"); const auto mb=Object(materialBinding); Pointer(mb,material); Array(mesh,12,1,mb);
        const auto bb=Object(boneBinding); Name(bb,"root"); Array(mesh,20,1,bb);
        const auto animation=Object(animationType); Name(animation,"idle"); Float(animation+4,1); Float(animation+8,1.f/30);
        const auto animations=Allocate(4); Pointer(animations,animation); Array(root,8,1,animations);
        const auto tg=Object(trackGroupType); Name(tg,"root"); Identity(tg+12); const auto groups=Allocate(4); Pointer(groups,tg); Array(animation,12,1,groups);
        track=Object(trackType); Name(track,"root"); Array(tg,4,1,track); curve=track+4;
    }
    std::vector<std::byte> Bytes() const
    {
        const std::size_t tableEnd=132,relocations=tableEnd+data.size();
        std::vector<std::byte> bytes(relocations+fixups.size()*12);
        for(unsigned i=0;i<4;++i) Put(bytes,i*4,std::array<std::uint32_t,4>{0xcab067b8,0x0fb16df8,0x7e8c7284,0x1e00195e}[i]);
        Put(bytes,16,tableEnd); Put(bytes,32,6); Put(bytes,36,static_cast<std::uint32_t>(bytes.size())); Put(bytes,44,56); Put(bytes,48,1);
        Put(bytes,56,rootType); Put(bytes,64,root); Put(bytes,68,0x80000010);
        Put(bytes,92,tableEnd); Put(bytes,96,static_cast<std::uint32_t>(data.size())); Put(bytes,100,static_cast<std::uint32_t>(data.size())); Put(bytes,104,4);
        Put(bytes,108,static_cast<std::uint32_t>(data.size())); Put(bytes,112,static_cast<std::uint32_t>(data.size()));
        Put(bytes,116,static_cast<std::uint32_t>(relocations)); Put(bytes,120,static_cast<std::uint32_t>(fixups.size())); Put(bytes,124,static_cast<std::uint32_t>(bytes.size()));
        std::copy(data.begin(),data.end(),bytes.begin()+tableEnd);
        std::size_t at=relocations; for(auto [source,target]:fixups) { Put(bytes,at,source); Put(bytes,at+8,target); at+=12; }
        Checksum(bytes); return bytes;
    }
    static void Checksum(std::vector<std::byte>& bytes)
    {
        std::uint32_t crc=UINT32_MAX;
        for(std::size_t i=88;i<bytes.size();++i) { crc^=std::to_integer<std::uint32_t>(bytes[i]); for(unsigned b=0;b<8;++b) crc=(crc>>1)^((crc&1)?0xedb88320u:0); }
        Put(bytes,40,crc^UINT32_MAX);
    }
};
}
