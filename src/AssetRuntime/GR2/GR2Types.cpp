#include "GR2Types.h"
#include <algorithm>

namespace AssetRuntime::GR2
{
const Type& Types::Get(Ref ref)
{
    if(const auto found=types_.find(ref);found!=types_.end()) return found->second;
    Require(building_.size()<64 && building_.insert(ref).second,"cyclic/deep inline type");
    Require(types_.size()<4096,"type allocation limit");
    Type type;
    for(std::size_t i=0;;++i) {
        Require(i<512 && ++members_<65536,"type member limit or missing terminator");
        const auto at=file.Add(ref,Product(i,32));
        const auto kind=file.Uint(at);
        if(!kind) break;
        file.Bytes(at,32);
        Member member; member.kind=kind; member.name=file.String(file.Pointer(file.Add(at,4)));
        Require(!member.name.empty(),"empty type member name");
        Require(std::none_of(type.members.begin(),type.members.end(),[&](const auto& m){return m.name==member.name;}),"duplicate type member");
        member.type=file.Pointer(file.Add(at,8)); member.width=file.Uint(file.Add(at,12));
        Require(member.width<=65536,"member array width limit");
        const auto width=std::max(1u,member.width);
        std::size_t size=0;
        switch(kind) {
        case 1: size=Get(member.type).size; break;
        case 2: case 8: case 22: size=4; break;
        case 3: case 4: case 5: size=8; break;
        case 7: size=12; break;
        case 9: size=68; break;
        case 10: case 19: case 20: size=4; break;
        case 11: case 12: case 13: case 14: size=1; break;
        case 15: case 16: case 17: case 18: case 21: size=2; break;
        default: Unsupported("member type "+std::to_string(kind)+" "+member.name);
        }
        member.offset=type.size; member.size=Product(size,width);
        Range(type.size,member.size,1024*1024); type.size+=member.size; type.members.push_back(std::move(member));
    }
    Require(type.size>0,"empty type definition");
    building_.erase(ref);
    return types_.emplace(ref,std::move(type)).first->second;
}
const Member* Types::Find(Object object, std::string_view name)
{
    Require(bool(object),"null typed object");
    const auto& type=Get(object.type); file.Bytes(object.data,type.size);
    for(const auto& member:type.members) if(member.name==name) return &member;
    return nullptr;
}
Ref Types::Field(Object object, std::string_view name)
{
    const auto* member=Find(object,name);
    if(!member) Unsupported("missing field "+std::string(name));
    return file.Add(object.data,member->offset);
}
Object Types::Child(Object object, std::string_view name)
{
    const auto* member=Find(object,name);
    if(!member) return {};
    const auto at=file.Add(object.data,member->offset);
    switch(member->kind) {
    case 1: return {at,member->type};
    case 2: return {file.Pointer(at),member->type};
    case 5: return {file.Pointer(file.Add(at,4)),file.Pointer(at)};
    default: Bad("field is not object: "+std::string(name));
    }
}
std::vector<Object> Types::Array(Object object, std::string_view name)
{
    const auto* member=Find(object,name);
    if(!member) return {};
    auto at=file.Add(object.data,member->offset); auto type=member->type;
    Require(member->kind==3 || member->kind==4 || member->kind==7,"field is not array: "+std::string(name));
    if(member->kind==7) { type=file.Pointer(at); at=file.Add(at,4); }
    const auto count=file.Uint(at); Require(count<=MaximumElements,"array count limit: "+std::string(name));
    if(!count) return {};
    const auto data=file.Pointer(file.Add(at,4));
    Require(bool(data)&&bool(type),"null nonempty array");
    const auto stride=member->kind==4?4:Get(type).size;
    file.Bytes(data,Product(count,stride));
    Charge(Product(count,std::max<std::size_t>(64,stride)));
    std::vector<Object> result; result.reserve(count);
    for(std::uint32_t i=0;i<count;++i) {
        const auto element=file.Add(data,Product(i,stride));
        result.push_back({member->kind==4?file.Pointer(element):element,type});
        Require(bool(result.back()),"null array element");
    }
    return result;
}
std::string Types::Text(Object object, std::string_view name)
{
    const auto* member=Find(object,name); if(!member) return {};
    Require(member->kind==8 && member->width<=1,"field is not string: "+std::string(name));
    auto text=file.String(file.Pointer(file.Add(object.data,member->offset))); Charge(text.size()*2); return text;
}
std::int32_t Types::Integer(Object object, std::string_view name)
{
    const auto* member=Find(object,name); if(!member) Unsupported("missing integer "+std::string(name));
    Require((member->kind==19 || member->kind==20) && member->width<=1,"field is not int32");
    return file.Int(file.Add(object.data,member->offset));
}
float Types::Real(Object object, std::string_view name)
{
    const auto* member=Find(object,name); Require(member && member->kind==10 && member->width<=1,"field is not real32");
    return file.Float(file.Add(object.data,member->offset));
}
std::vector<float> Types::Reals(Object object, std::string_view name)
{
    const auto* member=Find(object,name); Require(member!=nullptr,"missing float data");
    std::vector<float> result;
    if(member->kind==10) {
        const auto count=std::max(1u,member->width); auto at=file.Add(object.data,member->offset);
        Charge(Product(count,sizeof(float)));
        for(std::uint32_t i=0;i<count;++i) result.push_back(file.Float(file.Add(at,i*4)));
    } else {
        const auto objects=Array(object,name);
        if(!objects.empty()) { const auto& type=Get(objects[0].type); Require(type.members.size()==1 && type.members[0].kind==10,"float array element layout"); }
        for(const auto& item:objects) for(std::size_t i=0;i<Get(item.type).size;i+=4) result.push_back(file.Float(file.Add(item.data,i)));
    }
    return result;
}
std::string Types::Describe(Ref ref)
{
    const auto& type=Get(ref); std::string result=std::to_string(type.size)+":";
    for(const auto& member:type.members) result+=member.name+"/"+std::to_string(member.kind)+"x"+std::to_string(std::max(1u,member.width))+";";
    return result;
}
}
