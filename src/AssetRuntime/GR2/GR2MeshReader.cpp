#include "GR2Reader.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <algorithm>
#include <numeric>

namespace AssetRuntime::GR2
{
namespace
{
template<std::size_t N> std::array<float,N> Floats(Types& t,Object object,std::string_view name)
{
    const auto values=t.Reals(object,name); Require(values.size()==N,"vertex/bounds dimension: "+std::string(name));
    std::array<float,N> result; std::copy(values.begin(),values.end(),result.begin()); return result;
}
}
MeshData ReadMesh(Types& t,Object source,MeshAsset& mesh,ModelAsset& model,std::map<Ref,std::uint32_t>& materials,Contents& contents)
{
    AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Mesh);
    MeshData data; mesh.name=t.Text(source,"Name"); mesh.twoSided=mesh.name.starts_with("2x");
    if(!t.Array(source,"MorphTargets").empty()) Unsupported("mesh morph targets");
    const auto vertexData=t.Child(source,"PrimaryVertexData");
    Require(bool(vertexData),"missing primary vertex data");
    const auto vertices=t.Array(vertexData,"Vertices");
    const auto* vertexMember=t.Find(vertexData,"Vertices");
    Require(vertexMember!=nullptr,"missing vertex array");
    const auto vertexType=vertexMember->kind==7?t.file.Pointer(t.Field(vertexData,"Vertices")):vertexMember->type;
    const Object layout{vertexData.data,vertexType};
    Require(bool(vertexType),"missing vertex layout");
    mesh.vertexCount=static_cast<std::uint32_t>(vertices.size());
    const auto& type=t.Get(vertexType); mesh.sourceVertexStride=static_cast<std::uint32_t>(type.size);
    ++contents.vertexFormats[t.Describe(vertexType)];
    const auto* weights=t.Find(layout,"BoneWeights"); const auto* joints=t.Find(layout,"BoneIndices");
    const bool weighted=weights!=nullptr;
    Require(weighted==(joints!=nullptr),"incomplete skin vertex layout");
    const bool uv1=t.Find(layout,"TextureCoordinates1")!=nullptr;
    Require(t.Find(layout,"Position") && t.Find(layout,"Normal") && t.Find(layout,"TextureCoordinates0"),"incomplete vertex layout");
    for(const auto& member:type.members) {
        if(member.name=="Position"||member.name=="Normal") Require(member.kind==10&&member.width==3,"invalid position/normal layout");
        else if(member.name=="TextureCoordinates0"||member.name=="TextureCoordinates1") Require(member.kind==10&&member.width==2,"invalid UV layout");
        else if(member.name=="BoneWeights") Require(member.kind==14&&member.width==4,"unsupported weights layout");
        else if(member.name=="BoneIndices") Require(member.kind==12&&member.width==4,"unsupported bone index layout");
        else Unsupported("vertex component "+member.name);
    }
    mesh.vertexAttributes=Position|Normal|UV0|(uv1?UV1:0u);
    mesh.vertexLayout=weighted?VertexLayout::WeightedPositionNormalUV:(uv1?VertexLayout::PositionNormalUV2:VertexLayout::PositionNormalUV);
    if(weighted && uv1) Unsupported("weighted dual-UV layout");
    for(auto material:t.Array(source,"MaterialBindings")) {
        const auto object=t.Child(material,"Material");
        auto [where,added]=materials.emplace(object.data,static_cast<std::uint32_t>(model.materials.size()));
        if(added) model.materials.push_back(ReadMaterial(t,object));
        mesh.materialBindings.push_back(where->second);
    }
    for(auto bone:t.Array(source,"BoneBindings")) {
        mesh.skin.boneNames.push_back(t.Text(bone,"BoneName"));
        Bounds bounds; bounds.min=Floats<3>(t,bone,"OBBMin"); bounds.max=Floats<3>(t,bone,"OBBMax"); bounds.valid=true;
        mesh.skin.boneBounds.push_back(bounds);
        mesh.skin.meshToSkeleton.push_back(model.skeleton?model.skeleton->FindBone(mesh.skin.boneNames.back()):-1);
    }
    mesh.skin.validRemap=!mesh.skin.meshToSkeleton.empty() && std::all_of(mesh.skin.meshToSkeleton.begin(),mesh.skin.meshToSkeleton.end(),[](auto b){return b>=0;});
    mesh.deformation=weighted&&mesh.skin.boneNames.size()>1?Deformation::Skinned:Deformation::Rigid;
    if(mesh.deformation==Deformation::Skinned) Require(mesh.skin.validRemap,"invalid skin remap");
    if(weighted) { mesh.skin.influencesPerVertex=4; mesh.skin.weightOffset=12; mesh.skin.indexOffset=16; mesh.skin.normalizedByteWeights=mesh.skin.byteBoneIndices=true; }
    data.vertices.reserve(vertices.size());
    for(auto sourceVertex:vertices) {
        Vertex vertex; vertex.position=Floats<3>(t,sourceVertex,"Position"); vertex.normal=Floats<3>(t,sourceVertex,"Normal");
        vertex.uv=Floats<2>(t,sourceVertex,"TextureCoordinates0"); if(uv1) vertex.uv1=Floats<2>(t,sourceVertex,"TextureCoordinates1");
        if(weighted) {
            const auto w=t.file.Bytes(t.file.Add(sourceVertex.data,weights->offset),4), j=t.file.Bytes(t.file.Add(sourceVertex.data,joints->offset),4);
            for(unsigned i=0;i<4;++i) { vertex.weights[i]=std::to_integer<std::uint8_t>(w[i]); vertex.joints[i]=std::to_integer<std::uint8_t>(j[i]);
                Require(!vertex.weights[i] || vertex.joints[i]<mesh.skin.boneNames.size(),"weighted bone index out of range"); }
            Require(std::accumulate(vertex.weights.begin(),vertex.weights.end(),0)==255,"invalid normalized weight sum");
        }
        if(!mesh.bounds.valid) { mesh.bounds.min=mesh.bounds.max=vertex.position; mesh.bounds.valid=true; }
        else for(unsigned i=0;i<3;++i) { mesh.bounds.min[i]=std::min(mesh.bounds.min[i],vertex.position[i]); mesh.bounds.max[i]=std::max(mesh.bounds.max[i],vertex.position[i]); }
        data.vertices.push_back(vertex);
    }
    const auto topology=t.Child(source,"PrimaryTopology");
    Require(bool(topology),"missing primary topology");
    const auto wide=t.Array(topology,"Indices"), narrow=t.Array(topology,"Indices16");
    Require(wide.empty()||narrow.empty(),"duplicate index streams");
    const auto& indices=wide.empty()?narrow:wide;
    Require((!indices.empty() || vertices.empty()) && indices.size()%3==0,"invalid triangle topology");
    mesh.indexWidth=indices.empty()?IndexWidth::Unknown:wide.empty()?IndexWidth::UInt16:IndexWidth::UInt32; mesh.indexCount=static_cast<std::uint32_t>(indices.size());
    for(auto index:indices) {
        const auto& layout=t.Get(index.type); Require(layout.size==(wide.empty()?2u:4u) && layout.members.size()==1,"invalid index element");
        std::uint32_t value;
        if(wide.empty()) { const auto bytes=t.file.Bytes(index.data,2); value=std::to_integer<unsigned>(bytes[0])|(std::to_integer<unsigned>(bytes[1])<<8); }
        else value=t.file.Uint(index.data);
        Require(value<mesh.vertexCount,"mesh index out of range"); data.indices.push_back(value);
    }
    for(auto group:t.Array(topology,"Groups")) {
        const auto material=t.Integer(group,"MaterialIndex"), first=t.Integer(group,"TriFirst"), count=t.Integer(group,"TriCount");
        Require(first>=0 && count>=0,"negative triangle group");
        Range(Product(static_cast<std::size_t>(first),3),Product(static_cast<std::size_t>(count),3),indices.size());
        mesh.materialGroups.push_back({material<0||std::size_t(material)>=mesh.materialBindings.size()?UINT32_MAX:std::uint32_t(material),std::uint32_t(first)*3,std::uint32_t(count)*3});
    }
    Require(!mesh.materialGroups.empty() || (vertices.empty() && indices.empty()),"missing material groups");
    // The SDK exposes no vertex layout for a zero-vertex record. Validate its
    // serialized type above, then retain the same empty upload metadata.
    if(vertices.empty()) { mesh.sourceVertexStride=0;mesh.vertexLayout=VertexLayout::Unknown;mesh.vertexAttributes=0; }
    return data;
}
}
