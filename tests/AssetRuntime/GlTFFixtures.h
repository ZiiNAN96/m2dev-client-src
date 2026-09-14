#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <stdexcept>
#include <vector>

namespace GlTFFixtures
{
inline void U32(std::vector<std::byte>& out,std::uint32_t value)
{
    for(unsigned shift=0;shift<32;shift+=8) out.push_back(std::byte((value>>shift)&255));
}
inline void Put32(std::vector<std::byte>& out,std::size_t offset,std::uint32_t value)
{
    for(unsigned shift=0;shift<32;shift+=8) out[offset+shift/8]=std::byte((value>>shift)&255);
}
template<class T> inline void Append(std::vector<std::byte>& out,const T& value)
{
    const auto* bytes=reinterpret_cast<const std::byte*>(&value);out.insert(out.end(),bytes,bytes+sizeof(T));
}
inline std::vector<std::byte> GLB(std::string json,std::vector<std::byte> binary)
{
    while(json.size()%4) json+=' ';
    while(binary.size()%4) binary.push_back(std::byte{});
    std::vector<std::byte> result;
    U32(result,0x46546c67);U32(result,2);U32(result,static_cast<std::uint32_t>(28+json.size()+binary.size()));
    U32(result,static_cast<std::uint32_t>(json.size()));U32(result,0x4e4f534a);
    for(char c:json) result.push_back(std::byte(static_cast<unsigned char>(c)));
    U32(result,static_cast<std::uint32_t>(binary.size()));U32(result,0x004e4942);
    result.insert(result.end(),binary.begin(),binary.end());return result;
}
struct Triangle
{
    std::string json;
    std::vector<std::byte> binary;
    explicit Triangle(unsigned indexWidth=2,bool interleaved=true)
    {
        const std::array<std::array<float,8>,3> vertices{{{0,0,0,0,0,1,0,0},{1,0,0,0,0,1,1,0},{0,1,0,0,0,1,0,1}}};
        if(interleaved) for(const auto& vertex:vertices) Append(binary,vertex);
        else for(const auto& vertex:vertices) for(unsigned i=0;i<3;++i) Append(binary,vertex[i]);
        const auto indexOffset=binary.size();
        for(unsigned i=0;i<3;++i) for(unsigned b=0;b<indexWidth;++b) binary.push_back(std::byte((i>>(b*8))&255));
        const std::string width=std::to_string(indexWidth==1 ? 5121 : indexWidth==2 ? 5123 : 5125);
        json=R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[{"mesh":0}],"buffers":[{"byteLength":)"+std::to_string(binary.size())+
            R"(}],"bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":)"+std::to_string(indexOffset)+(interleaved ? ",\"byteStride\":32" : "")+
            R"(},{"buffer":0,"byteOffset":)"+std::to_string(indexOffset)+R"(,"byteLength":)"+std::to_string(indexWidth*3)+
            R"(}],"accessors":[{"bufferView":0,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":1,"componentType":)"+width+
            R"(,"count":3,"type":"SCALAR"})";
        if(interleaved) json+=R"(,{"bufferView":0,"byteOffset":12,"componentType":5126,"count":3,"type":"VEC3"},{"bufferView":0,"byteOffset":24,"componentType":5126,"count":3,"type":"VEC2"})";
        json+=R"(],"meshes":[{"primitives":[{"attributes":{"POSITION":0)";
        if(interleaved) json+=R"(,"NORMAL":2,"TEXCOORD_0":3)";
        json+=R"(},"indices":1}]}]})";
    }
    void Replace(std::string_view from,std::string_view to)
    {
        const auto pos=json.find(from);
        if(pos==std::string::npos) throw std::runtime_error("Fixture replacement did not match");
        json.replace(pos,from.size(),to);
    }
    std::vector<std::byte> Bytes() const { return GLB(json,binary); }
};

struct Builder
{
    std::vector<std::byte> binary;
    std::vector<std::string> views, accessors;
    std::string attributes{"\"POSITION\":0"}, indices, nodes{R"({"mesh":0})"}, extra;
    static std::string Join(const std::vector<std::string>& values)
    {
        std::string result;
        for (const auto& value : values) { if (!result.empty()) result += ','; result += value; }
        return result;
    }
    template<class T, std::size_t N> unsigned View(const std::array<T,N>& values, unsigned stride = 0)
    {
        while(binary.size()%4) binary.push_back(std::byte{});
        const auto offset = binary.size();
        Append(binary, values);
        views.push_back(R"({"buffer":0,"byteOffset":)" + std::to_string(offset) + R"(,"byteLength":)" +
            std::to_string(sizeof(values)) + (stride ? R"(,"byteStride":)" + std::to_string(stride) : "") + '}');
        return static_cast<unsigned>(views.size()-1);
    }
    unsigned Accessor(unsigned view, unsigned component, unsigned count, const char* type, unsigned offset = 0, bool normalized = false)
    {
        accessors.push_back(R"({"bufferView":)"+std::to_string(view)+R"(,"componentType":)"+std::to_string(component)+
            R"(,"count":)"+std::to_string(count)+R"(,"type":")"+type+R"(","byteOffset":)"+std::to_string(offset)+
            (normalized ? R"(,"normalized":true})" : "}"));
        return static_cast<unsigned>(accessors.size()-1);
    }
    Builder()
    {
        const std::array<std::array<float,8>,3> vertices{{{0,0,0,0,0,1,0,0},{1,0,0,0,0,1,1,0},{0,1,0,0,0,1,0,1}}};
        const auto view = View(vertices,32);
        Accessor(view,5126,3,"VEC3");
        Accessor(view,5126,3,"VEC3",12);
        Accessor(view,5126,3,"VEC2",24);
        attributes += R"(,"NORMAL":1,"TEXCOORD_0":2)";
    }
    std::string Json() const
    {
        return R"({"asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],"nodes":[)"+nodes+
            R"(],"buffers":[{"byteLength":)"+std::to_string(binary.size())+R"(}],"bufferViews":[)"+Join(views)+
            R"(],"accessors":[)"+Join(accessors)+R"(],"meshes":[{"primitives":[{"attributes":{)"+attributes+'}'+indices+
            R"(}]}])"+extra+'}';
    }
    std::vector<std::byte> Bytes() const { return GLB(Json(),binary); }
};
}
