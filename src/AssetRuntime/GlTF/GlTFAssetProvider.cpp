#include "GlTFAssetProvider.h"
#include "EterBase/MapLoadTrace.h"
#include "GlTFJsonValidation.h"
#include "cgltf.h"
#include "AssetRuntime/RuntimeAnimationInstance.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace AssetRuntime
{
namespace
{
// ZiiNAN BEGIN - glTF provider
constexpr std::size_t MaxInput = 128u * 1024u * 1024u;
constexpr std::size_t MaxJson = 8u * 1024u * 1024u;
constexpr std::size_t MaxAllocation = 256u * 1024u * 1024u;
constexpr std::size_t MaxElements = 4u * 1024u * 1024u;
constexpr std::size_t MaxObjects = 32768;
constexpr std::size_t MaxImageBytes = 32u * 1024u * 1024u;
constexpr float NativeUnitsPerMeter = 100.f;
constexpr Matrix4 Identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
// Row vectors: right-handed Y-up meters -> right-handed Z-up native units.
constexpr Matrix4 Basis{NativeUnitsPerMeter,0,0,0,0,0,NativeUnitsPerMeter,0,
    0,-NativeUnitsPerMeter,0,0,0,0,0,1};
constexpr float MetersPerNativeUnit = 1.f / NativeUnitsPerMeter;
constexpr Matrix4 InverseBasis{MetersPerNativeUnit,0,0,0,0,0,-MetersPerNativeUnit,0,
    0,MetersPerNativeUnit,0,0,0,0,0,1};

struct Failure : std::runtime_error
{
    AssetError error;
    Failure(AssetError value, std::string reason) : std::runtime_error(std::move(reason)), error(value) {}
};
void Require(bool value, std::string_view reason)
{
    if (!value) throw Failure(AssetError::InvalidAsset, std::string(reason));
}
void Supported(bool value, std::string_view reason)
{
    if (!value) throw Failure(AssetError::UnsupportedLayout, std::string(reason));
}
bool Fits(std::size_t offset, std::size_t count, std::size_t stride, std::size_t element, std::size_t size)
{
    return offset <= size && element <= size - offset && count > 0 &&
        (count == 1 || (stride > 0 && count - 1 <= (size - offset - element) / stride));
}
template<class T> T Read(const std::byte* data)
{
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return value;
}
std::uint32_t U32(const std::byte* data)
{
    return std::uint32_t(std::to_integer<unsigned char>(data[0])) |
        (std::uint32_t(std::to_integer<unsigned char>(data[1])) << 8) |
        (std::uint32_t(std::to_integer<unsigned char>(data[2])) << 16) |
        (std::uint32_t(std::to_integer<unsigned char>(data[3])) << 24);
}
std::uint32_t Big32(const std::byte* data)
{
    return (std::uint32_t(std::to_integer<unsigned char>(data[0])) << 24) |
        (std::uint32_t(std::to_integer<unsigned char>(data[1])) << 16) |
        (std::uint32_t(std::to_integer<unsigned char>(data[2])) << 8) |
        std::uint32_t(std::to_integer<unsigned char>(data[3]));
}
std::string Name(const char* value, std::string_view fallback)
{
    return value && *value ? value : std::string(fallback);
}
void ValidateContainer(std::span<const std::byte> bytes)
{
    Require(bytes.size() >= 20 && bytes.size() <= MaxInput, "GLB input is empty, truncated, or exceeds 128 MiB");
    Require(U32(bytes.data()) == 0x46546c67 && U32(bytes.data() + 4) == 2,
        "Expected GLB 2.0 container; external .gltf resources are not enabled");
    Require(U32(bytes.data() + 8) == bytes.size(), "GLB declared length differs from input length");
    std::size_t offset = 12, chunkIndex = 0;
    while (offset < bytes.size()) {
        Require(bytes.size() - offset >= 8, "Truncated GLB chunk header");
        const auto length = U32(bytes.data() + offset);
        const auto type = U32(bytes.data() + offset + 4);
        Require(length % 4 == 0 && length <= bytes.size() - offset - 8, "Invalid GLB chunk length");
        Require((chunkIndex == 0 && type == 0x4e4f534a) || (chunkIndex == 1 && type == 0x004e4942),
            "GLB must contain JSON followed by at most one BIN chunk");
        if (chunkIndex == 0) {
            Require(length > 0 && length <= MaxJson, "GLB JSON size limit exceeded");
            Require(GlTFDetail::JsonValidation({reinterpret_cast<const char*>(bytes.data()+offset+8),length}).Run(),
                "Invalid GLB JSON syntax, integer field, duplicate key, or material/animation enum");
            bool quoted = false, escaped = false;
            int depth = 0;
            for (std::size_t i = offset + 8; i < offset + 8 + length; ++i) {
                const auto c = std::to_integer<char>(bytes[i]);
                if (quoted) {
                    if (escaped) escaped = false;
                    else if (c == '\\') escaped = true;
                    else if (c == '"') quoted = false;
                } else if (c == '"') quoted = true;
                else if (c == '[' || c == '{') Require(++depth <= 64, "GLB JSON nesting limit exceeded");
                else if (c == ']' || c == '}') Require(--depth >= 0, "Invalid GLB JSON nesting");
            }
            Require(!quoted && depth == 0, "Truncated GLB JSON structure");
        }
        offset += 8 + length;
        ++chunkIndex;
    }
}
struct AllocationBudget { std::size_t allocated{}; };
void* Allocate(void* user, cgltf_size size)
{
    auto& budget = *static_cast<AllocationBudget*>(user);
    if (size > MaxAllocation - budget.allocated) return nullptr;
    void* result = std::malloc(size ? size : 1);
    if (result) budget.allocated += size;
    return result;
}
void Free(void*, void* pointer) { std::free(pointer); }
Matrix4 Multiply(const Matrix4& a, const Matrix4& b)
{
    Matrix4 result{};
    for (std::size_t r = 0; r < 4; ++r)
        for (std::size_t c = 0; c < 4; ++c)
            for (std::size_t k = 0; k < 4; ++k) result[r * 4 + c] += a[r * 4 + k] * b[k * 4 + c];
    for (float value : result) Require(std::isfinite(value), "Non-finite composed node matrix");
    return result;
}
Matrix4 ConvertMatrix(const Matrix4& source) { return Multiply(Multiply(InverseBasis, source), Basis); }
float Determinant(const Matrix4& m)
{
    return m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);
}
std::array<float,3> Position(const float* p, const Matrix4& m)
{
    return {p[0]*m[0]+p[1]*m[4]+p[2]*m[8]+m[12],
        p[0]*m[1]+p[1]*m[5]+p[2]*m[9]+m[13], p[0]*m[2]+p[1]*m[6]+p[2]*m[10]+m[14]};
}
std::array<float,3> Normalize(std::array<float,3> n)
{
    const double length = std::sqrt(double(n[0])*n[0]+double(n[1])*n[1]+double(n[2])*n[2]);
    Require(std::isfinite(length) && length > 1e-20, "Degenerate or non-finite normal");
    for (float& value : n) value = static_cast<float>(value / length);
    return n;
}
std::array<float,3> Normal(const float* n, const Matrix4& m)
{
    const float det = Determinant(m);
    Require(std::isfinite(det) && std::abs(det) > 1e-20f, "Singular mesh transform cannot transform normals");
    return Normalize({(n[0]*(m[5]*m[10]-m[6]*m[9])+n[1]*(m[2]*m[9]-m[1]*m[10])+n[2]*(m[1]*m[6]-m[2]*m[5]))/det,
        (n[0]*(m[6]*m[8]-m[4]*m[10])+n[1]*(m[0]*m[10]-m[2]*m[8])+n[2]*(m[2]*m[4]-m[0]*m[6]))/det,
        (n[0]*(m[4]*m[9]-m[5]*m[8])+n[1]*(m[1]*m[8]-m[0]*m[9])+n[2]*(m[0]*m[5]-m[1]*m[4]))/det});
}
bool Unsigned(cgltf_component_type type)
{
    return type == cgltf_component_type_r_8u || type == cgltf_component_type_r_16u || type == cgltf_component_type_r_32u;
}
std::uint32_t Integer(const std::byte* p, cgltf_component_type type)
{
    if (type == cgltf_component_type_r_8u) return Read<std::uint8_t>(p);
    if (type == cgltf_component_type_r_16u) return Read<std::uint16_t>(p);
    if (type == cgltf_component_type_r_32u) return Read<std::uint32_t>(p);
    throw Failure(AssetError::InvalidAsset, "Expected unsigned integer component");
}
float Component(const std::byte* p, cgltf_component_type type, bool normalized)
{
    switch (type) {
    case cgltf_component_type_r_8: { const auto value=Read<std::int8_t>(p); return normalized ? std::max(-1.f,value/127.f) : float(value); }
    case cgltf_component_type_r_8u: { const auto value=Read<std::uint8_t>(p); return normalized ? value/255.f : float(value); }
    case cgltf_component_type_r_16: { const auto value=Read<std::int16_t>(p); return normalized ? std::max(-1.f,value/32767.f) : float(value); }
    case cgltf_component_type_r_16u: { const auto value=Read<std::uint16_t>(p); return normalized ? value/65535.f : float(value); }
    case cgltf_component_type_r_32u: return float(Read<std::uint32_t>(p));
    case cgltf_component_type_r_32f: return Read<float>(p);
    default: throw Failure(AssetError::InvalidAsset, "Invalid accessor component type");
    }
}
const std::byte* View(const cgltf_buffer_view* view)
{
    return static_cast<const std::byte*>(view->buffer->data) + view->offset;
}
void ValidateData(cgltf_data& data)
{
    Require(data.file_type == cgltf_file_type_glb && data.asset.version && std::string_view(data.asset.version) == "2.0", "Only glTF 2.0 GLB is supported");
    Require(!data.asset.min_version || std::string_view(data.asset.min_version) == "2.0", "Unsupported minimum glTF version");
    for (std::size_t count : {data.accessors_count,data.buffer_views_count,data.nodes_count,data.meshes_count,
        data.materials_count,data.images_count,data.animations_count,data.skins_count,data.scenes_count,data.textures_count,data.samplers_count})
        Require(count <= MaxObjects, "Asset object count limit exceeded");
    for (std::size_t i = 0; i < data.extensions_required_count; ++i) {
        const std::string_view name(data.extensions_required[i]);
        Supported(name == "KHR_texture_transform" || name == "KHR_mesh_quantization",
            std::string("Unsupported required extension: ") + std::string(name));
    }
    for(std::size_t i=0;i<data.samplers_count;++i) {
        const auto& sampler=data.samplers[i];
        Require(sampler.mag_filter==cgltf_filter_type_undefined || sampler.mag_filter==cgltf_filter_type_nearest || sampler.mag_filter==cgltf_filter_type_linear,
            "Invalid texture magnification filter");
        Require(sampler.min_filter==cgltf_filter_type_undefined || sampler.min_filter==cgltf_filter_type_nearest || sampler.min_filter==cgltf_filter_type_linear ||
            sampler.min_filter==cgltf_filter_type_nearest_mipmap_nearest || sampler.min_filter==cgltf_filter_type_linear_mipmap_nearest ||
            sampler.min_filter==cgltf_filter_type_nearest_mipmap_linear || sampler.min_filter==cgltf_filter_type_linear_mipmap_linear,
            "Invalid texture minification filter");
        Supported((sampler.mag_filter==cgltf_filter_type_undefined || sampler.mag_filter==cgltf_filter_type_linear) &&
            (sampler.min_filter==cgltf_filter_type_undefined || sampler.min_filter==cgltf_filter_type_linear || sampler.min_filter==cgltf_filter_type_linear_mipmap_linear),
            "Only the existing linear texture sampler is supported");
        for(auto wrap:{sampler.wrap_s,sampler.wrap_t}) {
            Require(wrap==cgltf_wrap_mode_repeat || wrap==cgltf_wrap_mode_clamp_to_edge || wrap==cgltf_wrap_mode_mirrored_repeat, "Invalid texture wrap mode");
            Supported(wrap==cgltf_wrap_mode_repeat, "Only repeat texture wrapping is supported by the current material adapter");
        }
    }
    Require(data.buffers_count == 1 && data.buffers[0].uri == nullptr, "GLB must use one embedded BIN buffer; external/data URIs are unsupported");
    Require(data.buffers[0].size <= data.bin_size && data.bin_size - data.buffers[0].size <= 3 && data.bin,
        "Embedded BIN is missing or smaller than its declared buffer");
    data.buffers[0].data = const_cast<void*>(data.bin);
    for (std::size_t i = 0; i < data.buffer_views_count; ++i) {
        const auto& v = data.buffer_views[i];
        Require(v.buffer && v.buffer->data && v.offset <= v.buffer->size && v.size <= v.buffer->size - v.offset,
            "Buffer view exceeds embedded buffer bounds");
        Supported(!v.has_meshopt_compression, "EXT_meshopt_compression is not enabled");
        Require(v.stride == 0 || (v.stride >= 4 && v.stride <= 252 && v.stride % 4 == 0), "Invalid buffer view byteStride");
    }
    std::size_t sparseElements=0;
    for(std::size_t i=0;i<data.accessors_count;++i) if(data.accessors[i].is_sparse) {
        Require(data.accessors[i].sparse.count<=MaxElements-sparseElements, "Cumulative sparse accessor validation budget exceeded");
        sparseElements+=data.accessors[i].sparse.count;
    }
    for (std::size_t i = 0; i < data.accessors_count; ++i) {
        const auto& a = data.accessors[i];
        const auto component = cgltf_component_size(a.component_type);
        const auto element = cgltf_calc_size(a.type,a.component_type);
        Require(component && element && a.count > 0 && a.count <= MaxElements && a.stride >= element && a.stride % component == 0,
            "Invalid accessor type, count or stride");
        Require(!a.normalized || (a.component_type != cgltf_component_type_r_32f && a.component_type != cgltf_component_type_r_32u),
            "Invalid normalized accessor component");
        Require(a.offset % component == 0, "Misaligned accessor byteOffset");
        if (a.buffer_view) {
            Require((a.buffer_view->offset + a.offset) % component == 0 && Fits(a.offset,a.count,a.stride,element,a.buffer_view->size),
                "Accessor range exceeds buffer view");
        } else Require(a.offset == 0, "Accessor without buffer view has byteOffset");
        if (a.is_sparse) {
            const auto& s = a.sparse;
            const auto width = cgltf_component_size(s.indices_component_type);
            Require(s.count > 0 && s.count <= a.count && Unsigned(s.indices_component_type) && s.indices_buffer_view && s.values_buffer_view,
                "Invalid sparse accessor references or count");
            Require(!s.indices_buffer_view->stride && !s.values_buffer_view->stride &&
                Fits(s.indices_byte_offset,s.count,width,width,s.indices_buffer_view->size) &&
                Fits(s.values_byte_offset,s.count,element,element,s.values_buffer_view->size), "Sparse accessor exceeds buffer view");
            Require(s.indices_byte_offset % width == 0 && s.values_byte_offset % component == 0, "Misaligned sparse accessor");
            std::uint32_t previous{};
            for (std::size_t k=0;k<s.count;++k) {
                const auto index=Integer(View(s.indices_buffer_view)+s.indices_byte_offset+k*width,s.indices_component_type);
                Require(index<a.count && (k==0 || index>previous), "Sparse indices must be increasing, unique and within accessor count");
                previous=index;
            }
        }
    }
}
// Decode once with memcpy-based reads; sparse values are tightly packed even when base data is interleaved.
std::vector<float> Floats(const cgltf_accessor* a, cgltf_type expected, std::size_t& decoded)
{
    Require(a && a->type == expected, "Missing accessor or incompatible accessor shape");
    const auto components = cgltf_num_components(expected);
    const auto count = a->count * components;
    Require(count <= MaxAllocation / sizeof(float) && count * sizeof(float) <= MaxAllocation - decoded,
        "Decoded accessor allocation limit exceeded");
    decoded += count * sizeof(float);
    std::vector<float> values(count);
    const auto width = cgltf_component_size(a->component_type);
    const auto element = cgltf_calc_size(a->type,a->component_type);
    // All runtime consumers below use scalar/vector or float MAT4, which have no matrix column padding.
    Require(element == components*width, "Packed matrix component layout is unsupported");
    const auto convert = [&](std::size_t destination, const std::byte* source) {
        for (std::size_t c=0;c<components;++c) {
            const float value = Component(source+c*width,a->component_type,a->normalized!=0);
            Require(std::isfinite(value), "Accessor contains NaN or infinity");
            values[destination*components+c]=value;
        }
    };
    if (a->buffer_view) for (std::size_t i=0;i<a->count;++i) convert(i,View(a->buffer_view)+a->offset+i*a->stride);
    if (a->is_sparse) {
        const auto& s=a->sparse;
        const auto indexWidth=cgltf_component_size(s.indices_component_type);
        for (std::size_t i=0;i<s.count;++i) convert(Integer(View(s.indices_buffer_view)+s.indices_byte_offset+i*indexWidth,s.indices_component_type),
            View(s.values_buffer_view)+s.values_byte_offset+i*element);
    }
    return values;
}
std::vector<std::uint32_t> Indices(const cgltf_accessor* a, std::size_t vertices, std::size_t& decoded)
{
    const auto count=a ? a->count : vertices;
    Require(count%3==0 && count>0 && count<=MaxElements && count*sizeof(std::uint32_t)<=MaxAllocation-decoded, "Triangle index count or allocation limit is invalid");
    decoded += count*sizeof(std::uint32_t);
    std::vector<std::uint32_t> result(count);
    if (!a) std::iota(result.begin(),result.end(),0u);
    else {
        Require(a->type==cgltf_type_scalar && Unsigned(a->component_type) && !a->normalized && a->stride==cgltf_component_size(a->component_type),
            "Indices must be tightly packed unsigned scalar values");
        if (a->buffer_view) for(std::size_t i=0;i<count;++i) result[i]=Integer(View(a->buffer_view)+a->offset+i*a->stride,a->component_type);
        if (a->is_sparse) {
            const auto& s=a->sparse;
            const auto width=cgltf_component_size(s.indices_component_type);
            for(std::size_t i=0;i<s.count;++i) result[Integer(View(s.indices_buffer_view)+s.indices_byte_offset+i*width,s.indices_component_type)]=
                Integer(View(s.values_buffer_view)+s.values_byte_offset+i*a->stride,a->component_type);
        }
    }
    for(auto index:result) Require(index<vertices, "Triangle index exceeds vertex count");
    return result;
}
const cgltf_accessor* Attribute(const cgltf_primitive& primitive, cgltf_attribute_type type, int set=0)
{
    const cgltf_accessor* result=nullptr;
    for(std::size_t i=0;i<primitive.attributes_count;++i) if(primitive.attributes[i].type==type && primitive.attributes[i].index==set) {
        Require(!result, "Duplicate primitive attribute semantic");
        result=primitive.attributes[i].data;
    }
    return result;
}
struct NodeGraph { std::vector<Matrix4> local, world; std::vector<std::size_t> selected; };
NodeGraph Nodes(const cgltf_data& data)
{
    NodeGraph graph;
    graph.local.resize(data.nodes_count); graph.world.resize(data.nodes_count);
    std::vector<unsigned char> state(data.nodes_count);
    std::vector<unsigned> incoming(data.nodes_count);
    std::vector<unsigned> depths(data.nodes_count);
    for(std::size_t i=0;i<data.nodes_count;++i) {
        const auto& node=data.nodes[i];
        Require(!(node.has_matrix && (node.has_translation || node.has_rotation || node.has_scale)), "Node cannot combine matrix and TRS");
        Supported(!node.has_mesh_gpu_instancing, "EXT_mesh_gpu_instancing is unsupported");
        Supported(!node.weights_count, "Morph node weights are unsupported");
        if(node.has_rotation) {
            double length=0; for(float q:node.rotation) length+=double(q)*q;
            Require(std::isfinite(length) && std::abs(length-1)<.002, "Node rotation quaternion must be normalized");
        }
        cgltf_node_transform_local(&node,graph.local[i].data());
        for(float value:graph.local[i]) Require(std::isfinite(value), "Non-finite node transform");
        const auto& m=graph.local[i];
        Require(std::abs(m[3])+std::abs(m[7])+std::abs(m[11])<1e-6f && std::abs(m[15]-1)<1e-6f, "Node transform must be affine");
        for(std::size_t c=0;c<node.children_count;++c) {
            const auto* child=node.children[c];
            Require(child && child->parent==&node, "Invalid child parent relationship");
            const auto index=std::size_t(child-data.nodes);
            Require(index<data.nodes_count && ++incoming[index]==1, "Node has repeated child or multiple parents");
        }
    }
    for(std::size_t i=0;i<data.nodes_count;++i) if(state[i]!=2) {
        std::vector<std::size_t> chain;
        std::size_t current=i;
        while(state[current]!=2) {
            Require(state[current]==0, "Cyclic node hierarchy");
            state[current]=1; chain.push_back(current);
            Require(chain.size()<=256, "Node hierarchy depth limit exceeded");
            if(!data.nodes[current].parent) break;
            current=std::size_t(data.nodes[current].parent-data.nodes);
            Require(current<data.nodes_count, "Invalid node parent");
        }
        for(auto it=chain.rbegin();it!=chain.rend();++it) {
            const auto* parent=data.nodes[*it].parent;
            depths[*it]=parent ? depths[std::size_t(parent-data.nodes)]+1 : 1;
            Require(depths[*it]<=256, "Node hierarchy depth limit exceeded");
            graph.world[*it]=parent ? Multiply(graph.local[*it],graph.world[std::size_t(parent-data.nodes)]) : graph.local[*it];
            state[*it]=2;
        }
    }
    std::vector<std::size_t> pending;
    const auto* scene=data.scene ? data.scene : (data.scenes_count ? &data.scenes[0] : nullptr);
    if(scene) for(std::size_t i=0;i<scene->nodes_count;++i) {
        Require(scene->nodes[i] && !scene->nodes[i]->parent, "Scene root must have no parent");
        pending.push_back(std::size_t(scene->nodes[i]-data.nodes));
    } else for(std::size_t i=0;i<data.nodes_count;++i) if(!data.nodes[i].parent) pending.push_back(i);
    std::fill(state.begin(),state.end(),0);
    for(std::size_t i=0;i<pending.size();++i) {
        const auto index=pending[i];
        Require(index<data.nodes_count && !state[index], "Duplicate node in active scene");
        state[index]=1; graph.selected.push_back(index);
        for(std::size_t c=0;c<data.nodes[index].children_count;++c) pending.push_back(std::size_t(data.nodes[index].children[c]-data.nodes));
    }
    return graph;
}
std::uint32_t Crc32(std::span<const std::byte> bytes)
{
    std::uint32_t crc=0xffffffffu;
    for(auto byte:bytes) { crc^=std::to_integer<unsigned char>(byte); for(int bit=0;bit<8;++bit) crc=(crc>>1)^(0xedb88320u & (0u-(crc&1u))); }
    return ~crc;
}
std::size_t ValidateImage(std::span<const std::byte> bytes, std::string_view mime)
{
    Require(bytes.size()<=MaxImageBytes, "Encoded image exceeds 32 MiB");
    unsigned width=0,height=0;
    if(mime=="image/png") {
        const unsigned char signature[]{137,80,78,71,13,10,26,10};
        Require(bytes.size()>=33 && std::memcmp(bytes.data(),signature,8)==0, "Invalid PNG signature");
        bool header=false,imageData=false,end=false;
        std::size_t offset=8;
        while(offset<bytes.size()) {
            Require(bytes.size()-offset>=12, "Truncated PNG chunk");
            const auto size=Big32(bytes.data()+offset);
            Require(size<=bytes.size()-offset-12, "PNG chunk exceeds image bounds");
            const auto* type=bytes.data()+offset+4;
            Require(Crc32(bytes.subspan(offset+4,size+4))==Big32(bytes.data()+offset+8+size), "PNG checksum mismatch");
            if(std::memcmp(type,"IHDR",4)==0) {
                Require(!header && offset==8 && size==13, "Invalid PNG header");
                width=Big32(type+4);height=Big32(type+8);header=true;
            } else if(std::memcmp(type,"IDAT",4)==0) imageData=true;
            else if(std::memcmp(type,"IEND",4)==0) { Require(size==0 && offset+12==bytes.size(), "Invalid PNG end chunk");end=true; }
            offset+=size+12;
        }
        Require(header && imageData && end, "Incomplete PNG image");
    } else if(mime=="image/jpeg") {
        Require(bytes.size()>=4 && bytes[0]==std::byte{0xff} && bytes[1]==std::byte{0xd8} &&
            bytes[bytes.size()-2]==std::byte{0xff} && bytes.back()==std::byte{0xd9}, "Invalid JPEG container");
        std::size_t offset=2;
        while(offset+4<=bytes.size()) {
            Require(bytes[offset++]==std::byte{0xff}, "Invalid JPEG marker");
            while(offset<bytes.size() && bytes[offset]==std::byte{0xff}) ++offset;
            Require(offset+3<=bytes.size(), "Truncated JPEG marker");
            const auto marker=std::to_integer<unsigned char>(bytes[offset++]);
            if(marker==0xda || marker==0xd9) break;
            const unsigned length=(unsigned(std::to_integer<unsigned char>(bytes[offset]))<<8)|std::to_integer<unsigned char>(bytes[offset+1]);
            Require(length>=2 && length<=bytes.size()-offset, "Invalid JPEG segment length");
            if((marker>=0xc0 && marker<=0xc3) || (marker>=0xc5 && marker<=0xc7) || (marker>=0xc9 && marker<=0xcb) || (marker>=0xcd && marker<=0xcf)) {
                Require(length>=8, "Invalid JPEG frame");
                height=(unsigned(std::to_integer<unsigned char>(bytes[offset+3]))<<8)|std::to_integer<unsigned char>(bytes[offset+4]);
                width=(unsigned(std::to_integer<unsigned char>(bytes[offset+5]))<<8)|std::to_integer<unsigned char>(bytes[offset+6]);
            }
            offset+=length;
        }
    } else throw Failure(AssetError::UnsupportedLayout,"Only embedded PNG and JPEG images are supported");
    Require(width>0 && height>0 && width<=8192 && height<=8192 && std::uint64_t(width)*height<=16u*1024u*1024u,
        "Image dimensions exceed decode budget or are invalid");
    return std::size_t(width)*height*4;
}
struct Vertex { std::array<float,3> position{},normal{}; std::array<float,2> uv{}; };
static_assert(sizeof(Vertex)==32);
struct MeshData { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
class StaticBinding final : public MeshBinding
{
public:
    explicit StaticBinding(ModelHandle owner):owner_(std::move(owner)){}
    std::span<const std::int32_t> BoneIndices() const override { return root_; }
    AssetError DeformVertices(std::span<std::byte>,std::span<const float>,bool) const override { return AssetError::UnsupportedLayout; }
private:
    ModelHandle owner_;
    std::array<std::int32_t,1> root_{0};
};
class StaticAnimation final : public AnimationInstance
{
public:
    explicit StaticAnimation(ModelHandle owner):owner_(std::move(owner)){}
    bool PreparePose() override { return true; }
    AssetError SetAnimation(const AnimationHandle&,float) override { return AssetError::UnsupportedLayout; }
    AssetError SetMotion(const AnimationHandle&,float,float,int,float) override { return AssetError::UnsupportedLayout; }
    AssetError ChangeMotion(const AnimationHandle&,float,int,float) override { return AssetError::UnsupportedLayout; }
    AssetError CopyMotionFrom(AnimationInstance&,float,bool) override { return AssetError::UnsupportedLayout; }
    bool IsPlaying() const override { return false; }
    void SetMotionAtEnd() override {}
    void SetClock(float) override {}
    void FreeCompletedControls() override {}
    void UpdateTransform(float,std::span<float,16>) const override {}
    std::span<const float> BoneWorldMatrix(BoneId bone) const override { return bone==0 ? std::span<const float>(matrix_) : std::span<const float>{}; }
    PoseView CompositePose() const override { return {matrix_}; }
    std::unique_ptr<MeshBinding> CreateMeshBinding(const ModelHandle& source,std::size_t mesh) const override
    {
        return source && source.GetDocument()==owner_.GetDocument() && source.Index()==owner_.Index() && mesh<source.Get()->meshes.size() ?
            std::make_unique<StaticBinding>(source) : nullptr;
    }
    PoseResult Evaluate(const PoseRequest& request) override
    {
        matrix_=Identity;
        if(!request.attachmentMatrix.empty()) {
            if(request.attachmentMatrix.size()!=16) return {{},AssetError::InvalidInput};
            for(float value:request.attachmentMatrix) if(!std::isfinite(value)) return {{},AssetError::InvalidInput};
            std::copy(request.attachmentMatrix.begin(),request.attachmentMatrix.end(),matrix_.begin());
        }
        return {{matrix_},AssetError::None};
    }
private:
    ModelHandle owner_;
    Matrix4 matrix_{Identity};
};
class Document final : public AssetDocument
{
public:
    Document(AssetId id,cgltf_data& data):AssetDocument(std::move(id))
    {
        ValidateData(data);
        const auto graph=Nodes(data);
        ModelAsset model;
        model.name=Name(data.scene ? data.scene->name : nullptr,Id());
        std::size_t decoded=0;
        ImportMaterials(data,model,decoded);
        ImportSkeleton(data,graph,model,decoded);
        ImportAnimations(data,model,decoded);
        for(auto nodeIndex:graph.selected) {
            const auto& node=data.nodes[nodeIndex];
            if(!node.mesh) continue;
            Require(node.mesh->primitives_count<=MaxObjects, "Primitive count limit exceeded");
            for(std::size_t p=0;p<node.mesh->primitives_count;++p) {
                Require(model.meshes.size()<MaxObjects, "Scene mesh count limit exceeded");
                ImportPrimitive(data,node,node.mesh->primitives[p],graph.world[nodeIndex],model,decoded);
            }
        }
        Require(!model.meshes.empty(), "Active GLB scene has no mesh primitives");
        if(skeleton_) {
            std::size_t vertices=0;bool rigid=false,skinned=false;
            for(const auto& mesh:model.meshes) {
                vertices+=mesh.vertexCount;rigid|=mesh.deformation==Deformation::Rigid;skinned|=mesh.deformation==Deformation::Skinned;
            }
            Supported(vertices<=65535,"F5-X character exceeds the existing GPU combined 16-bit vertex range; split offline");
            model.deformation=rigid&&skinned?Deformation::Mixed:skinned?Deformation::Skinned:Deformation::Rigid;
        }
        models_.push_back(std::move(model));
    }
    AssetError CopyVertices(std::size_t model,std::size_t mesh,VertexLayout layout,std::span<std::byte> output) const override
    {
        if(model!=0 || mesh>=meshes_.size()) return AssetError::InvalidHandle;
        if(released_) return AssetError::UploadDataReleased;
        if(layout!=VertexLayout::PositionNormalUV && layout!=VertexLayout::WeightedPositionNormalUV) return AssetError::UnsupportedLayout;
        if(layout==VertexLayout::WeightedPositionNormalUV) {
            const auto& vertices=meshes_[mesh].vertices;
            const auto& skin=models_[0].meshes[mesh].skin;
            if(skin.jointIndices.size()!=vertices.size()) return AssetError::UnsupportedLayout;
            if(output.size()<vertices.size()*40) return AssetError::BufferTooSmall;
            for(std::size_t v=0;v<vertices.size();++v) {
                // Deterministic largest-remainder quantization to the production
                // UNORM8 convention; byte weights always total exactly 255.
                std::array<std::uint8_t,4> weights{},joints{};std::array<float,4> remainder{};unsigned total=0;
                for(unsigned k=0;k<4;++k) {
                    const float scaled=skin.jointWeights[v][k]*255;
                    weights[k]=static_cast<std::uint8_t>(std::floor(scaled));total+=weights[k];
                    remainder[k]=scaled-weights[k];joints[k]=static_cast<std::uint8_t>(skin.jointIndices[v][k]);
                }
                while(total<255) {const auto k=std::size_t(std::max_element(remainder.begin(),remainder.end())-remainder.begin());++weights[k];remainder[k]=-1;++total;}
                auto* out=output.data()+v*40;
                std::memcpy(out,vertices[v].position.data(),12);std::memcpy(out+12,weights.data(),4);std::memcpy(out+16,joints.data(),4);
                std::memcpy(out+20,vertices[v].normal.data(),12);std::memcpy(out+32,vertices[v].uv.data(),8);
            }
            return AssetError::None;
        }
        const auto& values=meshes_[mesh].vertices;
        if(output.size()<values.size()*sizeof(Vertex)) return AssetError::BufferTooSmall;
        std::memcpy(output.data(),values.data(),values.size()*sizeof(Vertex));
        return AssetError::None;
    }
    AssetError CopyIndices(std::size_t model,std::size_t mesh,IndexWidth width,std::span<std::byte> output) const override
    {
        if(model!=0 || mesh>=meshes_.size()) return AssetError::InvalidHandle;
        if(released_) return AssetError::UploadDataReleased;
        if(width!=IndexWidth::UInt16 && width!=IndexWidth::UInt32) return AssetError::InvalidIndexWidth;
        const auto& values=meshes_[mesh].indices;
        if(output.size()<values.size()*static_cast<std::size_t>(width)) return AssetError::BufferTooSmall;
        if(width==IndexWidth::UInt32) std::memcpy(output.data(),values.data(),values.size()*sizeof(std::uint32_t));
        else {
            if(models_[0].meshes[mesh].indexWidth==IndexWidth::UInt32) return AssetError::InvalidIndexWidth;
            for(std::size_t i=0;i<values.size();++i) { const auto value=static_cast<std::uint16_t>(values[i]);std::memcpy(output.data()+i*2,&value,2); }
        }
        return AssetError::None;
    }
    void ReleaseUploadData() override
    {
        for(auto& mesh:meshes_) { std::vector<Vertex>{}.swap(mesh.vertices);std::vector<std::uint32_t>{}.swap(mesh.indices); }
        released_=true;
    }
    std::unique_ptr<PoseEvaluator> CreatePose(const ModelHandle& model) const override { return CreateAnimationInstance(model); }
    std::unique_ptr<AnimationInstance> CreateAnimationInstance(const ModelHandle& model) const override
    {
        if(!model || model.Index()!=0 || model.GetDocument().get()!=this || !model.Get()->renderable) return {};
        if(skeleton_) return std::make_unique<RuntimeAnimationInstance>(model,skeleton_,clips_);
        return std::make_unique<StaticAnimation>(model);
    }
    const AR::RuntimeSkeleton* RuntimeSkeleton(std::size_t model) const override {return model==0?skeleton_.get():nullptr;}
    const AR::RuntimeAnimationClip* RuntimeClip(std::size_t clip) const override {return clip<clips_->size()?(*clips_)[clip].get():nullptr;}
private:
    void ImportMaterials(const cgltf_data& data,ModelAsset& model,std::size_t& decoded)
    {
        std::vector<std::shared_ptr<const EncodedImage>> images(data.images_count);
        std::vector<std::string> packImages(data.images_count);
        for(std::size_t i=0;i<data.images_count;++i) {
            const auto& source=data.images[i];
            if(source.uri) {
                const std::string path(source.uri);
                Supported(!source.buffer_view&&path.starts_with("d:/ymir work/")&&path.ends_with(".dds")&&path.size()<=1024&&
                    path.find("..") == std::string::npos&&path.find('\\')==std::string::npos&&path.find('%')==std::string::npos&&
                    std::none_of(path.begin(),path.end(),[](unsigned char c){return c<32;}),"Only canonical DDS pack texture references are supported");
                packImages[i]=path;continue; // Existing texture cache resolves this key; the provider performs no I/O.
            }
            Supported(!source.uri && source.buffer_view && source.mime_type, "Images must be embedded GLB PNG/JPEG buffer views");
            const auto bytes=std::span<const std::byte>(View(source.buffer_view),source.buffer_view->size);
            Require(bytes.size()<=MaxAllocation-decoded, "Encoded image allocation limit exceeded"); decoded+=bytes.size();
            const auto pixelBytes=ValidateImage(bytes,source.mime_type);
            Require(pixelBytes<=MaxAllocation-decoded, "Cumulative decoded image allocation budget exceeded");decoded+=pixelBytes;
            auto image=std::make_shared<EncodedImage>();
            image->mimeType=source.mime_type;
            std::uint64_t hash=14695981039346656037ull;
            for(auto byte:bytes) { hash^=std::to_integer<unsigned char>(byte);hash*=1099511628211ull; }
            image->id=Id()+"#image/"+std::to_string(i)+"/"+std::to_string(hash);
            image->bytes.assign(bytes.begin(),bytes.end());images[i]=std::move(image);
        }
        for(std::size_t i=0;i<data.materials_count;++i) {
            const auto& source=data.materials[i];
            MaterialAsset material;
            // Only an authored PBR block opts into the modern BRDF. A GLB
            // container or a missing material does not invent metallic data.
            if(source.has_pbr_metallic_roughness)material.model=MaterialModel::PBRMetallicRoughness;
            material.name=Name(source.name,"material-"+std::to_string(i));
            material.explicitRenderState=true;
            material.culling=source.double_sided ? Culling::None : Culling::Clockwise;
            material.doubleSided=source.double_sided;
            Require(source.alpha_mode==cgltf_alpha_mode_opaque || source.alpha_mode==cgltf_alpha_mode_mask || source.alpha_mode==cgltf_alpha_mode_blend, "Invalid alpha mode");
            material.alphaTest=source.alpha_mode==cgltf_alpha_mode_mask;
            material.blending=source.alpha_mode==cgltf_alpha_mode_blend;
            material.alphaMode=material.blending?AlphaMode::Blend:(material.alphaTest?AlphaMode::Mask:AlphaMode::Opaque);
            material.depthWrite=!material.blending;
            material.alphaCutoff=source.alpha_cutoff;
            Require(std::isfinite(material.alphaCutoff) && material.alphaCutoff>=0, "Invalid alpha cutoff");
            auto mapTexture=[&](const cgltf_texture_view& texture,MaterialTexture slot,std::uint8_t channel=0) {
                if(!texture.texture) return;
                Require(texture.texture->image, "Material texture has no image");
                Supported(texture.texcoord==0 && (!texture.has_transform || !texture.transform.has_texcoord || texture.transform.texcoord==0), "Only TEXCOORD_0 is supported for material textures");
                // The existing mesh importer bakes base-color transforms. Other
                // maps must address that same UV domain until independent UVs exist.
                if(slot!=MaterialTexture::BaseColor) {
                    const auto& base=source.pbr_metallic_roughness.base_color_texture;
                    Supported(!texture.has_transform && !base.has_transform, "Independent material texture transforms require offline UV baking");
                }
                const auto imageIndex=std::size_t(texture.texture->image-data.images);
                Require(imageIndex<images.size(), "Material image index out of range");
                auto& target=material.materialTextures[static_cast<std::size_t>(slot)];
                target.image=images[imageIndex]; target.id=target.image?target.image->id:packImages[imageIndex];
                target.channel=channel;
            };
            if(source.has_pbr_metallic_roughness) {
                std::copy_n(source.pbr_metallic_roughness.base_color_factor,4,material.baseColorFactor.begin());
                for(float value:material.baseColorFactor) Require(std::isfinite(value) && value>=0 && value<=1, "Invalid base color factor");
                material.metallic=source.pbr_metallic_roughness.metallic_factor;
                material.roughness=source.pbr_metallic_roughness.roughness_factor;
                for(float value:{material.metallic,material.roughness}) Require(std::isfinite(value)&&value>=0&&value<=1,"Invalid metallic/roughness factor");
                const auto& texture=source.pbr_metallic_roughness.base_color_texture;
                mapTexture(texture,MaterialTexture::BaseColor);
                mapTexture(source.pbr_metallic_roughness.metallic_roughness_texture,MaterialTexture::Roughness,1);
                mapTexture(source.pbr_metallic_roughness.metallic_roughness_texture,MaterialTexture::Metallic,2);
                if(texture.texture) {
                    Require(texture.texture->image, "Base color texture has no image");
                    Supported(texture.texcoord==0 && (!texture.has_transform || !texture.transform.has_texcoord || texture.transform.texcoord==0), "Only TEXCOORD_0 is supported for base color");
                    const auto imageIndex=std::size_t(texture.texture->image-data.images);
                    material.embeddedImages[0]=images[imageIndex];
                    material.textures[0]=material.embeddedImages[0]?material.embeddedImages[0]->id:packImages[imageIndex];
                }
            }
            mapTexture(source.normal_texture,MaterialTexture::Normal);
            mapTexture(source.occlusion_texture,MaterialTexture::Occlusion);
            mapTexture(source.emissive_texture,MaterialTexture::Emissive);
            material.normalScale=source.normal_texture.texture?source.normal_texture.scale:1.f;
            material.occlusionStrength=source.occlusion_texture.texture?source.occlusion_texture.scale:1.f;
            Require(std::isfinite(material.normalScale)&&material.normalScale>=0,"Invalid normal scale");
            Require(std::isfinite(material.occlusionStrength)&&material.occlusionStrength>=0&&material.occlusionStrength<=1,"Invalid occlusion strength");
            std::copy_n(source.emissive_factor,3,material.emissiveColor.begin());
            for(float value:material.emissiveColor) Require(std::isfinite(value)&&value>=0&&value<=1,"Invalid emissive factor");
            model.materials.push_back(std::move(material));
        }
        MaterialAsset fallback;
        fallback.name="default";fallback.explicitRenderState=true;fallback.alphaTest=false;
        fallback.alphaMode=AlphaMode::Opaque;
        model.materials.push_back(std::move(fallback));
    }
    void ImportSkeleton(const cgltf_data& data,const NodeGraph& graph,ModelAsset& model,std::size_t& decoded)
    {
        Supported(data.skins_count<=1, "F5-X supports one skin per document; multiple skins require explicit asset splitting");
        model.skeleton.emplace();
        auto& skeleton=*model.skeleton;
        if(!data.skins_count && !data.animations_count) {
            skeleton.name="static-root";skeleton.rootIndex=0;
            BoneAsset bone;bone.id=0;bone.name="root";bone.inverseBind=Identity;skeleton.bones.push_back(bone);
            return;
        }
        // Node-only animations from the existing offline exporter use the same
        // runtime hierarchy, with rigid mesh-to-node bindings and no skin joints.
        const cgltf_skin emptySkin{};
        const auto& skin=data.skins_count?data.skins[0]:emptySkin;
        Require(!data.skins_count || (skin.joints_count>0 && skin.joints_count<=163),"Skin requires 1..163 joints for the existing production GPU palette");
        Supported(data.nodes_count<=163,"F5-X joint and hierarchy node palette exceeds the production limit of 163 entries");
        skeleton.name=Name(skin.name,"skin");
        skeleton.sourceRootNode=skin.skeleton?static_cast<std::int32_t>(skin.skeleton-data.nodes):-1;
        nodeToBone_.assign(data.nodes_count,-1);
        std::vector<std::size_t> order;
        // Keep every skin joint at its original index. Append real hierarchy nodes in
        // source order; this preserves animated ancestors without synthetic roots.
        for(std::size_t i=0;i<skin.joints_count;++i) {
            Require(skin.joints[i],"Missing skin joint node");
            const auto node=std::size_t(skin.joints[i]-data.nodes);
            Require(node<data.nodes_count && nodeToBone_[node]<0,"Duplicate or invalid skin joint node");
            Require(std::find(graph.selected.begin(),graph.selected.end(),node)!=graph.selected.end(),"Skin joint is outside the active scene");
            if(skin.skeleton) {
                const cgltf_node* ancestor=skin.joints[i];
                while(ancestor && ancestor!=skin.skeleton) ancestor=ancestor->parent;
                Require(ancestor,"Skin skeleton root is not an ancestor of every joint");
            }
            nodeToBone_[node]=static_cast<BoneId>(order.size());order.push_back(node);
        }
        for(std::size_t i=0;i<data.nodes_count;++i) if(nodeToBone_[i]<0) {
            nodeToBone_[i]=static_cast<BoneId>(order.size());order.push_back(i);
        }
        std::vector<float> inverse;
        if(skin.inverse_bind_matrices) {
            Require(skin.inverse_bind_matrices->count==skin.joints_count && skin.inverse_bind_matrices->component_type==cgltf_component_type_r_32f,
                "Inverse bind matrix count must match the joint count and use FLOAT MAT4");
            inverse=Floats(skin.inverse_bind_matrices,cgltf_type_mat4,decoded);
        } // glTF specifies identity matrices when this optional accessor is absent.
        std::vector<AR::SkeletonBone> runtime;
        for(std::size_t i=0;i<order.size();++i) {
            const auto nodeIndex=order[i];const auto& node=data.nodes[nodeIndex];
            BoneAsset bone;bone.id=static_cast<BoneId>(i);bone.sourceNodeIndex=static_cast<std::int32_t>(nodeIndex);
            bone.name=Name(node.name,"node-"+std::to_string(nodeIndex));
            bone.parentIndex=node.parent?nodeToBone_[node.parent-data.nodes]:-1;
            bone.localBindMatrix=ConvertMatrix(graph.local[nodeIndex]);bone.hasLocalBindMatrix=true;
            AR::LocalTransform local;
            if(node.has_matrix) {
                local.translation={bone.localBindMatrix[12],bone.localBindMatrix[13],bone.localBindMatrix[14]};
                for(unsigned row=0;row<3;++row) for(unsigned col=0;col<3;++col)
                    local.scaleShear[row*3+col]=bone.localBindMatrix[row*4+col];
            } else {
                local.translation=Position(node.translation,Basis);
                local.rotation={node.rotation[0],-node.rotation[2],node.rotation[1],node.rotation[3]};
                local.scaleShear={node.scale[0],0,0,0,node.scale[2],0,0,0,node.scale[1]};
            }
            bone.localBind.flags=7;bone.localBind.position=local.translation;bone.localBind.orientation=local.rotation;bone.localBind.scaleShear=local.scaleShear;
            Matrix4 matrix=Identity;
            if(i<skin.joints_count && !inverse.empty()) std::copy_n(inverse.data()+i*16,16,matrix.begin());
            Require(matrix[3]==0 && matrix[7]==0 && matrix[11]==0 && matrix[15]==1 && std::abs(Determinant(matrix))>1e-20f,
                "Inverse bind matrix must be finite, affine and invertible");
            bone.inverseBind=ConvertMatrix(matrix);
            if(skeleton.sourceRootNode==bone.sourceNodeIndex) skeleton.rootIndex=bone.id;
            runtime.push_back({bone.name,bone.parentIndex,local,bone.inverseBind});
            skeleton.bones.push_back(std::move(bone));
        }
        if(skeleton.rootIndex<0) for(const auto& bone:skeleton.bones) if(bone.parentIndex<0) {skeleton.rootIndex=bone.id;break;}
        auto translated=std::make_shared<AR::RuntimeSkeleton>();std::string error;
        Require(translated->Initialize(std::move(runtime),error,AR::RuntimeSkeleton::SourceSemantics::IndexedForest),"RuntimeSkeleton: "+error);
        skeleton_=std::move(translated);
    }
    void ImportAnimations(const cgltf_data& data,ModelAsset& model,std::size_t& decoded)
    {
        for(std::size_t i=0;i<data.animations_count;++i) {
            const auto& source=data.animations[i];
            Require(source.samplers_count>0 && source.samplers_count<=MaxObjects && source.channels_count>0 && source.channels_count<=MaxObjects,
                "Invalid animation sampler/channel count");
            AnimationAsset animation;
            animation.name=Name(source.name,"animation-"+std::to_string(i));animation.metadataOnly=false;
            std::vector<AR::AnimationTrack> tracks;
            std::vector<std::vector<float>> times(source.samplers_count);
            for(std::size_t s=0;s<source.samplers_count;++s) {
                const auto& sampler=source.samplers[s];
                Require(sampler.input && sampler.output && sampler.input->component_type==cgltf_component_type_r_32f &&
                    sampler.output->component_type==cgltf_component_type_r_32f, "Animation samples must be float accessors");
                Require(sampler.interpolation==cgltf_interpolation_type_linear || sampler.interpolation==cgltf_interpolation_type_step ||
                    sampler.interpolation==cgltf_interpolation_type_cubic_spline, "Invalid animation interpolation");
                Supported(sampler.interpolation!=cgltf_interpolation_type_cubic_spline,"CUBICSPLINE is unsupported; export LINEAR or STEP explicitly");
                times[s]=Floats(sampler.input,cgltf_type_scalar,decoded);
                float previous=-1;
                for(float time:times[s]) { Require(time>=0 && time>previous, "Animation key times must be nonnegative and strictly increasing");previous=time; }
                animation.duration=std::max(animation.duration,times[s].back());
            }
            for(std::size_t c=0;c<source.channels_count;++c) {
                const auto& channel=source.channels[c];
                Require(channel.target_node && channel.sampler, "Animation channel lacks node or sampler");
                Supported(channel.target_path!=cgltf_animation_path_type_weights, "Morph animation channels are unsupported");
                AnimationChannelAsset metadata;
                metadata.targetNode=static_cast<std::int32_t>(channel.target_node-data.nodes);
                metadata.targetName=Name(channel.target_node->name,"node-"+std::to_string(metadata.targetNode));
                Require(metadata.targetNode>=0 && std::size_t(metadata.targetNode)<nodeToBone_.size(),"Unknown animation target node");
                Supported(!channel.target_node->has_matrix,"Animation targets must use TRS, not a matrix");
                cgltf_type shape=cgltf_type_vec3;
                if(channel.target_path==cgltf_animation_path_type_translation) metadata.path=AnimationPath::Translation;
                else if(channel.target_path==cgltf_animation_path_type_rotation) { metadata.path=AnimationPath::Rotation;shape=cgltf_type_vec4; }
                else { Require(channel.target_path==cgltf_animation_path_type_scale,"Invalid animation target path");metadata.path=AnimationPath::Scale; }
                const auto& sampler=*channel.sampler;
                const auto keys=sampler.input->count;
                const auto multiplier=sampler.interpolation==cgltf_interpolation_type_cubic_spline ? 3u : 1u;
                Require(sampler.output->count==keys*multiplier, "Animation output count differs from key count");
                const auto output=Floats(sampler.output,shape,decoded);
                const auto runtimeKeySize=metadata.path==AnimationPath::Rotation?sizeof(AR::Keyframe<AR::Quaternion>):
                    metadata.path==AnimationPath::Scale?sizeof(AR::Keyframe<AR::ScaleShear>):sizeof(AR::Keyframe<AR::Vector3>);
                Require(keys<=(MaxAllocation-decoded)/runtimeKeySize,"Runtime animation key allocation budget exceeded");
                decoded+=keys*runtimeKeySize;
                if(shape==cgltf_type_vec4) for(std::size_t key=0;key<keys;++key) {
                    const auto start=(key*multiplier+(multiplier==3 ? 1 : 0))*4;
                    double length=0;for(std::size_t component=0;component<4;++component) length+=double(output[start+component])*output[start+component];
                    Require(std::abs(length-1)<.002, "Rotation animation quaternion must be normalized");
                }
                metadata.keyframeCount=static_cast<std::uint32_t>(keys);
                metadata.interpolation=sampler.interpolation==cgltf_interpolation_type_cubic_spline ? AnimationInterpolation::CubicSpline :
                    sampler.interpolation==cgltf_interpolation_type_step ? AnimationInterpolation::Step : AnimationInterpolation::Linear;
                for(const auto& existing:animation.channels) Require(existing.targetNode!=metadata.targetNode || existing.path!=metadata.path,
                    "Repeated animation target node/path");
                const auto bone=static_cast<std::uint32_t>(nodeToBone_[metadata.targetNode]);
                auto track=std::find_if(tracks.begin(),tracks.end(),[&](const auto& item){return item.targetBone==bone;});
                if(track==tracks.end()) {tracks.push_back({});track=std::prev(tracks.end());track->targetBone=bone;}
                const auto interpolation=sampler.interpolation==cgltf_interpolation_type_step?AR::Interpolation::Step:AR::Interpolation::Linear;
                const auto& keyTimes=times[std::size_t(channel.sampler-source.samplers)];
                for(std::size_t k=0;k<keys;++k) {
                    if(metadata.path==AnimationPath::Translation) {
                        track->translation.interpolation=interpolation;
                        track->translation.keys.push_back({keyTimes[k],Position(output.data()+k*3,Basis)});
                    } else if(metadata.path==AnimationPath::Rotation) {
                        track->rotation.interpolation=interpolation==AR::Interpolation::Linear?AR::Interpolation::SphericalLinear:interpolation;
                        track->rotation.keys.push_back({keyTimes[k],{output[k*4],-output[k*4+2],output[k*4+1],output[k*4+3]}});
                    } else {
                        track->scaleShear.interpolation=interpolation;
                        track->scaleShear.keys.push_back({keyTimes[k],{output[k*3],0,0,0,output[k*3+2],0,0,0,output[k*3+1]}});
                    }
                }
                animation.channels.push_back(std::move(metadata));
            }
            animation.trackGroupCount=static_cast<std::uint32_t>(animation.channels.size());
            model.animations.push_back(static_cast<std::uint32_t>(animations_.size()));
            auto clip=std::make_shared<AR::RuntimeAnimationClip>();std::string error;
            Require(clip->Initialize(animation.name,animation.duration,false,std::move(tracks),*skeleton_,error),"RuntimeAnimationClip: "+error);
            clips_->push_back(std::move(clip));
            animations_.push_back(std::move(animation));
        }
    }
    void ImportPrimitive(const cgltf_data& data,const cgltf_node& node,const cgltf_primitive& primitive,const Matrix4& world,ModelAsset& model,std::size_t& decoded)
    {
        Supported(primitive.type==cgltf_primitive_type_triangles, "Only TRIANGLES primitive mode is supported");
        Supported(!primitive.has_draco_mesh_compression, "KHR_draco_mesh_compression is unsupported");
        Supported(!primitive.targets_count, "Morph targets are unsupported");
        const auto* positions=Attribute(primitive,cgltf_attribute_type_position);
        const auto* normals=Attribute(primitive,cgltf_attribute_type_normal);
        const auto* uv=Attribute(primitive,cgltf_attribute_type_texcoord);
        const auto* joints=Attribute(primitive,cgltf_attribute_type_joints);
        const auto* weights=Attribute(primitive,cgltf_attribute_type_weights);
        const auto* tangents=Attribute(primitive,cgltf_attribute_type_tangent);
        const auto* colors=Attribute(primitive,cgltf_attribute_type_color);
        const auto* uv1=Attribute(primitive,cgltf_attribute_type_texcoord,1);
        const cgltf_accessor *pivots=nullptr,*flexibility=nullptr,*pitchCos=nullptr,*pitchSin=nullptr;
        for(std::size_t a=0;a<primitive.attributes_count;++a) {
            const auto& attr=primitive.attributes[a];if(!attr.name)continue;
            const std::string_view name(attr.name);
            if(name=="_ZIINAN_CARD_PITCH_COS"){Require(!pitchCos,"duplicate card pitch channel");pitchCos=attr.data;}
            if(name=="_ZIINAN_CARD_PITCH_SIN"){Require(!pitchSin,"duplicate card pitch channel");pitchSin=attr.data;}
            if(name=="_ZIINAN_PIVOT"){Require(!pivots,"duplicate pivot channel");pivots=attr.data;}
            if(name=="_ZIINAN_FLEXIBILITY"){Require(!flexibility,"duplicate flexibility channel");flexibility=attr.data;}
        }
        Require(positions, "Mesh primitive is missing POSITION");
        bool quantized=false;
        for(std::size_t i=0;i<data.extensions_used_count;++i) if(std::string_view(data.extensions_used[i])=="KHR_mesh_quantization") quantized=true;
        for(std::size_t i=0;i<data.extensions_required_count;++i) if(std::string_view(data.extensions_required[i])=="KHR_mesh_quantization") quantized=true;
        for(std::size_t a=0;a<primitive.attributes_count;++a) {
            const auto& attribute=primitive.attributes[a];
            const auto* accessor=attribute.data;
            Require(accessor && accessor->count==positions->count, "Vertex attribute count mismatch");
            Require(!accessor->buffer_view || (accessor->buffer_view->offset%4==0 && accessor->offset%4==0 && accessor->stride%4==0), "Vertex attributes require four-byte element alignment");
            if(attribute.type==cgltf_attribute_type_joints || attribute.type==cgltf_attribute_type_weights)
                Supported(attribute.index==0, "Only four JOINTS_0/WEIGHTS_0 skin influences are supported");
            const bool floating=accessor->component_type==cgltf_component_type_r_32f && !accessor->normalized;
            const bool signedSmall=accessor->component_type==cgltf_component_type_r_8 || accessor->component_type==cgltf_component_type_r_16;
            const bool unsignedSmall=accessor->component_type==cgltf_component_type_r_8u || accessor->component_type==cgltf_component_type_r_16u;
            if(attribute.type==cgltf_attribute_type_position)
                Require(floating || (quantized && (signedSmall || unsignedSmall)), "POSITION component type requires float or KHR_mesh_quantization");
            else if(attribute.type==cgltf_attribute_type_normal || attribute.type==cgltf_attribute_type_tangent)
                Require(floating || (quantized && signedSmall && accessor->normalized), "NORMAL/TANGENT must be float or normalized signed quantized data");
            else if(attribute.type==cgltf_attribute_type_texcoord)
                Require(floating || (unsignedSmall && accessor->normalized) || (quantized && (signedSmall || unsignedSmall)), "Invalid TEXCOORD component type");
            else if(attribute.type==cgltf_attribute_type_color)
                Require(attribute.index==0&&(floating||(unsignedSmall&&accessor->normalized)),"Invalid COLOR_0 component type");
            if(accessor==pivots||accessor==flexibility||accessor==pitchCos||accessor==pitchSin)Require(floating,"Auxiliary deformation channels require floats");
        }
        const auto p=Floats(positions,cgltf_type_vec3,decoded);
        const auto n=normals ? Floats(normals,cgltf_type_vec3,decoded) : std::vector<float>{};
        const auto t=uv ? Floats(uv,cgltf_type_vec2,decoded) : std::vector<float>{};
        const auto tangentValues=tangents ? Floats(tangents,cgltf_type_vec4,decoded) : std::vector<float>{};
        const auto colorValues=colors?Floats(colors,colors->type==cgltf_type_vec3?cgltf_type_vec3:cgltf_type_vec4,decoded):std::vector<float>{};
        const auto uv1Values=uv1?Floats(uv1,cgltf_type_vec2,decoded):std::vector<float>{};
        const auto pivotValues=pivots?Floats(pivots,cgltf_type_vec3,decoded):std::vector<float>{};
        const auto pitchCosValues=pitchCos?Floats(pitchCos,cgltf_type_vec3,decoded):std::vector<float>{};
        const auto pitchSinValues=pitchSin?Floats(pitchSin,cgltf_type_vec3,decoded):std::vector<float>{};
        const auto flexibilityValues=flexibility?Floats(flexibility,cgltf_type_scalar,decoded):std::vector<float>{};
        Require(positions->count*sizeof(Vertex)<=MaxAllocation-decoded,"Mesh vertex allocation limit exceeded");decoded+=positions->count*sizeof(Vertex);
        MeshData buffer;
        buffer.vertices.resize(positions->count);buffer.indices=Indices(primitive.indices,positions->count,decoded);
        // Skinned mesh-node transforms cancel in glTF skinning. Joint model matrices
        // already contain the scene/root transforms. Rigid nodes in a character are
        // also evaluated by the shared skeleton, so neither path bakes them twice.
        const auto transform=skeleton_?Basis:Multiply(world,Basis);
        const float determinant=Determinant(transform);
        Require(std::isfinite(determinant) && std::abs(determinant)>1e-20f, "Mesh transform is singular");
        if(determinant<0) for(std::size_t i=0;i<buffer.indices.size();i+=3) std::swap(buffer.indices[i+1],buffer.indices[i+2]);
        const cgltf_texture_view* texture=primitive.material && primitive.material->has_pbr_metallic_roughness ? &primitive.material->pbr_metallic_roughness.base_color_texture : nullptr;
        Require(!texture || !texture->texture || uv, "Textured primitive is missing TEXCOORD_0");
        for(std::size_t v=0;v<buffer.vertices.size();++v) {
            auto& vertex=buffer.vertices[v];
            vertex.position=Position(p.data()+v*3,transform);
            for(float value:vertex.position) Require(std::isfinite(value) && std::abs(value)<1e10f, "Transformed vertex exceeds coordinate bounds");
            if(normals) vertex.normal=Normal(n.data()+v*3,transform);
            if(uv) {
                vertex.uv={t[v*2],t[v*2+1]};
                if(texture && texture->has_transform) {
                    const auto& tr=texture->transform;
                    const float x=vertex.uv[0]*tr.scale[0],y=vertex.uv[1]*tr.scale[1];
                    vertex.uv={tr.offset[0]+std::cos(tr.rotation)*x-std::sin(tr.rotation)*y,
                        tr.offset[1]+std::sin(tr.rotation)*x+std::cos(tr.rotation)*y};
                }
                for(float value:vertex.uv) Require(std::isfinite(value), "Non-finite UV transform");
            }
        }
        if(!normals) {
            for(std::size_t i=0;i<buffer.indices.size();i+=3) {
                const auto& a=buffer.vertices[buffer.indices[i]].position;
                const auto& b=buffer.vertices[buffer.indices[i+1]].position;
                const auto& c=buffer.vertices[buffer.indices[i+2]].position;
                const std::array<float,3> u{b[0]-a[0],b[1]-a[1],b[2]-a[2]},v{c[0]-a[0],c[1]-a[1],c[2]-a[2]};
                const std::array<float,3> normal{u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0]};
                for(std::size_t corner=0;corner<3;++corner) for(std::size_t axis=0;axis<3;++axis) buffer.vertices[buffer.indices[i+corner]].normal[axis]+=normal[axis];
            }
            for(auto& vertex:buffer.vertices) {
                if(vertex.normal==std::array<float,3>{}) vertex.normal={0,0,1};
                else vertex.normal=Normalize(vertex.normal);
            }
        }
        MeshAsset mesh;
        mesh.name=Name(node.name,Name(node.mesh->name,"mesh"))+"/"+std::to_string(model.meshes.size());
        mesh.vertexCount=static_cast<std::uint32_t>(buffer.vertices.size());mesh.indexCount=static_cast<std::uint32_t>(buffer.indices.size());
        mesh.vertexLayout=VertexLayout::PositionNormalUV;mesh.sourceVertexStride=sizeof(Vertex);
        if(colors||uv1||pivots||flexibility||pitchCos||pitchSin) {
            Require(buffer.vertices.size()*sizeof(MeshAsset::VertexExtras)<=MaxAllocation-decoded,"Auxiliary vertex allocation limit exceeded");decoded+=buffer.vertices.size()*sizeof(MeshAsset::VertexExtras);
            mesh.vertexExtrasChannels=(colors?MeshAsset::ColorChannel:0)|(uv1?MeshAsset::UV1Channel:0)|(pivots?MeshAsset::PivotChannel:0)|(flexibility?MeshAsset::FlexibilityChannel:0)|(pitchCos?MeshAsset::CardPitchCosChannel:0)|(pitchSin?MeshAsset::CardPitchSinChannel:0);
            mesh.vertexExtras.resize(buffer.vertices.size());
            for(std::size_t v=0;v<buffer.vertices.size();++v){auto& extra=mesh.vertexExtras[v];
                if(colors){const unsigned width=colors->type==cgltf_type_vec3?3:4;for(unsigned k=0;k<width;++k){extra.color[k]=colorValues[v*width+k];Require(extra.color[k]>=0&&extra.color[k]<=1,"Invalid vertex color");}}
                if(uv1)extra.uv1={uv1Values[v*2],uv1Values[v*2+1]};
                if(pivots){extra.pivot=Position(pivotValues.data()+v*3,transform);for(float f:extra.pivot)Require(std::isfinite(f)&&std::abs(f)<1e10f,"Invalid transformed pivot");}
                if(flexibility){extra.flexibility=flexibilityValues[v];Require(extra.flexibility>=0&&extra.flexibility<=1,"Invalid flexibility");}
                if(pitchCos)for(unsigned k=0;k<3;++k){extra.cardPitchCos[k]=pitchCosValues[v*3+k];Require(std::abs(extra.cardPitchCos[k])<=4,"Invalid card pitch cosine");}
                if(pitchSin)for(unsigned k=0;k<3;++k){extra.cardPitchSin[k]=pitchSinValues[v*3+k];Require(std::abs(extra.cardPitchSin[k])<=4,"Invalid card pitch sine");}
            }
        }
        if(tangents) {
            Require(buffer.vertices.size()*sizeof(std::array<float,4>)<=MaxAllocation-decoded,"Tangent metadata allocation limit exceeded");
            decoded+=buffer.vertices.size()*sizeof(std::array<float,4>);
            mesh.tangents.reserve(buffer.vertices.size());
            auto linear=transform;linear[12]=linear[13]=linear[14]=0;
            for(std::size_t v=0;v<buffer.vertices.size();++v) {
                const float handedness=tangentValues[v*4+3];
                Require(handedness==1 || handedness==-1, "Tangent handedness must be -1 or +1");
                auto tangent=Position(tangentValues.data()+v*4,linear);
                const auto& normal=buffer.vertices[v].normal;
                const float dot=tangent[0]*normal[0]+tangent[1]*normal[1]+tangent[2]*normal[2];
                for(std::size_t axis=0;axis<3;++axis) tangent[axis]-=normal[axis]*dot;
                tangent=Normalize(tangent);
                mesh.tangents.push_back({tangent[0],tangent[1],tangent[2],determinant<0 ? -handedness : handedness});
            }
        }
        // The converted PNT stream always carries UV0; untextured input without UVs receives zero UVs.
        mesh.vertexAttributes=VertexAttribute::Position|VertexAttribute::Normal|VertexAttribute::UV0;
        mesh.indexWidth=*std::max_element(buffer.indices.begin(),buffer.indices.end())<=65535 ? IndexWidth::UInt16 : IndexWidth::UInt32;
        mesh.bounds.min=mesh.bounds.max=buffer.vertices[0].position;mesh.bounds.valid=true;
        for(const auto& vertex:buffer.vertices) for(std::size_t axis=0;axis<3;++axis) {
            mesh.bounds.min[axis]=std::min(mesh.bounds.min[axis],vertex.position[axis]);mesh.bounds.max[axis]=std::max(mesh.bounds.max[axis],vertex.position[axis]);
        }
        const auto material=primitive.material ? static_cast<std::uint32_t>(primitive.material-data.materials) : static_cast<std::uint32_t>(model.materials.size()-1);
        mesh.materialBindings={material};mesh.materialGroups={{0,0,mesh.indexCount}};mesh.twoSided=model.materials[material].culling==Culling::None;
        if(node.skin) {
            Require(joints && weights, "Skinned primitive requires JOINTS_0 and WEIGHTS_0");
            Require((joints->component_type==cgltf_component_type_r_8u || joints->component_type==cgltf_component_type_r_16u) && !joints->normalized,
                "JOINTS_0 must use unnormalized unsigned byte or short components");
            Require(weights->component_type==cgltf_component_type_r_32f ||
                ((weights->component_type==cgltf_component_type_r_8u || weights->component_type==cgltf_component_type_r_16u) && weights->normalized),
                "WEIGHTS_0 must use float or normalized unsigned byte/short components");
            const auto j=Floats(joints,cgltf_type_vec4,decoded),w=Floats(weights,cgltf_type_vec4,decoded);
            Require(buffer.vertices.size()*32<=MaxAllocation-decoded,"Skin metadata allocation limit exceeded");decoded+=buffer.vertices.size()*32;
            mesh.skin.jointIndices.resize(buffer.vertices.size());mesh.skin.jointWeights.resize(buffer.vertices.size());
            for(std::size_t v=0;v<buffer.vertices.size();++v) {
                float sum=0;
                for(std::size_t k=0;k<4;++k) {
                    Require(j[v*4+k]>=0 && j[v*4+k]<node.skin->joints_count && w[v*4+k]>=0 && w[v*4+k]<=1, "Skin joint index or weight is invalid");
                    mesh.skin.jointIndices[v][k]=static_cast<std::uint32_t>(j[v*4+k]);sum+=w[v*4+k];
                }
                Require(std::isfinite(sum) && std::abs(sum-1.f)<=.02f, "Skin weights must sum to one within 0.02; invalid sets are not repaired");
                for(std::size_t k=0;k<4;++k) mesh.skin.jointWeights[v][k]=w[v*4+k]/sum;
            }
            for(std::size_t i=0;i<node.skin->joints_count;++i) {
                const auto& bone=model.skeleton->bones[i];mesh.skin.boneNames.push_back(bone.name);mesh.skin.meshToSkeleton.push_back(bone.id);
                mesh.skin.boneBounds.push_back(mesh.bounds);
            }
            mesh.skin.influencesPerVertex=4;mesh.skin.validRemap=true;mesh.deformation=Deformation::Skinned;
            mesh.vertexLayout=VertexLayout::WeightedPositionNormalUV;mesh.sourceVertexStride=40;
            mesh.skin.weightOffset=12;mesh.skin.indexOffset=16;mesh.skin.normalizedByteWeights=true;mesh.skin.byteBoneIndices=true;
            model.deformation=Deformation::Skinned;
        } else {
            Require(!joints && !weights, "Skin attributes require a node skin");
            const BoneId bone=skeleton_?nodeToBone_[&node-data.nodes]:0;
            mesh.skin.boneNames={model.skeleton->bones[bone].name};mesh.skin.meshToSkeleton={bone};mesh.skin.boneBounds={mesh.bounds};mesh.skin.validRemap=true;
        }
        model.meshes.push_back(std::move(mesh));meshes_.push_back(std::move(buffer));
    }
    std::shared_ptr<const AR::RuntimeSkeleton> skeleton_;
    std::shared_ptr<std::vector<std::shared_ptr<const AR::RuntimeAnimationClip>>> clips_=std::make_shared<std::vector<std::shared_ptr<const AR::RuntimeAnimationClip>>>();
    std::vector<BoneId> nodeToBone_;
    std::vector<MeshData> meshes_;
    bool released_{};
};
class Provider final : public AssetProvider
{
public:
    LoadResult Load(AssetId id,std::span<const std::byte> bytes) override
    {
    MapLoadTrace::Scope p0lScope("Assets","GLB parse","cpu");
    MapLoadTrace::Count("glb-parse",id,bytes.size(),true);

        try {
            Supported(std::endian::native==std::endian::little, "Big-endian GLB import is not enabled");
            ValidateContainer(bytes);
            AllocationBudget budget;
            cgltf_options options{};
            options.type=cgltf_file_type_glb;
            options.memory={Allocate,Free,&budget};
            cgltf_data* raw=nullptr;
            const auto result=cgltf_parse(&options,bytes.data(),bytes.size(),&raw);
            std::unique_ptr<cgltf_data,decltype(&cgltf_free)> parsed(raw,cgltf_free);
            Require(result==cgltf_result_success && raw,"cgltf rejected malformed GLB structure (status "+std::to_string(result)+")");
            return {AssetHandle(std::make_shared<Document>(std::move(id),*parsed)),AssetError::None,{}};
        } catch(const Failure& failure) { return {{},failure.error,failure.what()}; }
        catch(const std::bad_alloc&) { return {{},AssetError::InvalidAsset,"GLB allocation budget exhausted"}; }
        catch(const std::exception& failure) { return {{},AssetError::InvalidAsset,failure.what()}; }
    }
};
// ZiiNAN END
}
AssetProvider& GetGlTFAssetProvider() { static Provider provider;return provider; }
}
