#pragma once
#include "GR2File.h"
#include <memory>
#include <set>
#include <string_view>

namespace AssetRuntime::GR2
{
struct Member { std::uint32_t kind{}, width{}; std::string name; Ref type; std::size_t offset{}, size{}; };
struct Type { std::vector<Member> members; std::size_t size{}; };
struct Object { Ref data, type; explicit operator bool() const { return bool(data) && bool(type); } };
class Types
{
public:
    explicit Types(const File& file) : file(file) {}
    const File& file;
    const Type& Get(Ref type);
    const Member* Find(Object object, std::string_view name);
    Ref Field(Object object, std::string_view name);
    Object Child(Object object, std::string_view name);
    std::vector<Object> Array(Object object, std::string_view name);
    std::string Text(Object object, std::string_view name);
    std::int32_t Integer(Object object, std::string_view name);
    float Real(Object object, std::string_view name);
    std::vector<float> Reals(Object object, std::string_view name);
    Object Root() const { return {file.header.rootObject,file.header.rootType}; }
    std::string Describe(Ref type);
    void Charge(std::size_t bytes) { Range(workBytes_,bytes,256u*1024u*1024u); workBytes_+=bytes; }
private:
    std::map<Ref,Type> types_;
    std::set<Ref> building_;
    std::size_t members_{};
    std::size_t workBytes_{};
};
}
