#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include "GlTFFixtures.h"
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>

using namespace AssetRuntime;
using GlTFFixtures::Triangle;
static void Check(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
static LoadResult Load(const std::vector<std::byte>& bytes) { return GetGlTFAssetProvider().Load("fixtures/test.glb",bytes); }
static void Reject(const std::vector<std::byte>& bytes,const char* reason)
{
    const auto result=Load(bytes);
    Check(!result && !result.diagnostic.empty(),reason);
}
static std::vector<float> Vertices(const AssetHandle& asset)
{
    std::vector<float> values(asset.Model(0).Get()->meshes[0].vertexCount*8);
    Check(asset.Get()->CopyVertices(0,0,VertexLayout::PositionNormalUV,std::as_writable_bytes(std::span(values)))==AssetError::None,"copy vertices");
    return values;
}

static void CheckSparseAndNormalized()
{
    GlTFFixtures::Builder fixture;
    const auto sparseIndices = fixture.View(std::array<std::uint8_t,2>{1,2});
    const auto sparseValues = fixture.View(std::array<float,6>{2,0,0, 0,3,0});
    fixture.accessors[0].pop_back();
    const auto sparse = R"(,"sparse":{"count":2,"indices":{"bufferView":)"+std::to_string(sparseIndices)+
        R"(,"componentType":5121},"values":{"bufferView":)"+std::to_string(sparseValues)+"}}}";
    fixture.accessors[0] += sparse;
    auto result = Load(fixture.Bytes()); Check(bool(result),result.diagnostic.c_str());
    auto values = Vertices(result.asset);
    Check(values[8]==200 && values[18]==300 && values[14]==1, "Sparse tightly packed values override interleaved base without corrupting UV");
    fixture.accessors[0] = R"({"componentType":5126,"count":3,"type":"VEC3")"+sparse;
    result = Load(fixture.Bytes()); Check(bool(result),result.diagnostic.c_str());
    values = Vertices(result.asset);
    Check(values[0]==0 && values[8]==200 && values[18]==300, "Sparse accessor with zero-filled absent base");
    fixture.binary[97] = std::byte{1};
    Reject(fixture.Bytes(),"Duplicate sparse indices rejected");
    fixture.binary[97] = std::byte{3};
    Reject(fixture.Bytes(),"Sparse index outside accessor rejected");
    fixture.binary[97] = std::byte{2};
    const auto uvView = fixture.View(std::array<std::uint8_t,12>{0,128,0,0, 255,0,0,0, 128,255,0,0},4);
    const auto uv = fixture.Accessor(uvView,5121,3,"VEC2",0,true);
    fixture.attributes = R"("POSITION":0,"NORMAL":1,"TEXCOORD_0":)"+std::to_string(uv);
    result = Load(fixture.Bytes()); Check(bool(result),result.diagnostic.c_str());
    values = Vertices(result.asset);
    Check(std::abs(values[7]-128.f/255.f)<1e-6f && values[14]==1 && values[23]==1,
        "Normalized unsigned-byte UV values preserve component scaling and V orientation");
}

static void CheckTangentsAndNormals()
{
    GlTFFixtures::Builder fixture;
    const auto view = fixture.View(std::array<float,12>{1,0,0,1, 1,0,0,1, 1,0,0,1});
    const auto tangent = fixture.Accessor(view,5126,3,"VEC4");
    fixture.attributes += R"(,"TANGENT":)"+std::to_string(tangent);
    fixture.nodes = R"({"mesh":0,"scale":[-2,3,4]})";
    auto result = Load(fixture.Bytes()); Check(bool(result),result.diagnostic.c_str());
    const auto& metadata = result.asset.Model(0).Get()->meshes[0].tangents;
    Check(metadata.size()==3 && metadata[0][0]==-1 && metadata[0][3]==-1,
        "Tangent direction and handedness follow mirrored node conversion");
    fixture.nodes = R"({"mesh":0,"scale":[2,1,4]})";
    const std::array<float,3> normal{std::sqrt(.5f),std::sqrt(.5f),0};
    for(std::size_t v=0;v<3;++v) std::memcpy(fixture.binary.data()+v*32+12,normal.data(),12);
    result = Load(fixture.Bytes()); Check(bool(result),result.diagnostic.c_str());
    const auto values = Vertices(result.asset);
    Check(std::abs(values[3]-1.f/std::sqrt(5.f))<1e-5f && std::abs(values[4])<1e-5f &&
        std::abs(values[5]-2.f/std::sqrt(5.f))<1e-5f, "Nonuniform node scale uses inverse-transpose normals");
}

static GlTFFixtures::Builder SkinAnimationFixture()
{
    GlTFFixtures::Builder fixture;
    fixture.nodes = R"({"mesh":0,"skin":0,"children":[1]},{"name":"root","children":[2]},{"name":"tip","translation":[0,1,0]})";
    const auto jointsView = fixture.View(std::array<std::uint8_t,12>{0,1,0,0, 0,1,0,0, 0,1,0,0});
    const auto joints = fixture.Accessor(jointsView,5121,3,"VEC4");
    const auto weightsView = fixture.View(std::array<std::uint8_t,12>{128,127,0,0, 128,127,0,0, 128,127,0,0});
    const auto weights = fixture.Accessor(weightsView,5121,3,"VEC4",0,true);
    fixture.attributes += R"(,"JOINTS_0":)"+std::to_string(joints)+R"(,"WEIGHTS_0":)"+std::to_string(weights);
    const std::array<float,32> inverse{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,
        1,0,0,0,0,1,0,0,0,0,1,0,0,-1,0,1};
    const auto inverseView = fixture.View(inverse);
    const auto inverseAccessor = fixture.Accessor(inverseView,5126,2,"MAT4");
    const auto timeView = fixture.View(std::array<float,2>{0,2});
    const auto time = fixture.Accessor(timeView,5126,2,"SCALAR");
    const auto outputView = fixture.View(std::array<float,6>{0,1,0, 0,2,0});
    const auto output = fixture.Accessor(outputView,5126,2,"VEC3");
    fixture.extra = R"(,"skins":[{"name":"two-joint","joints":[1,2],"skeleton":1,"inverseBindMatrices":)"+
        std::to_string(inverseAccessor)+R"(}],"animations":[{"name":"move","samplers":[{"input":)"+std::to_string(time)+
        R"(,"output":)"+std::to_string(output)+R"(,"interpolation":"STEP"}],"channels":[{"sampler":0,"target":{"node":2,"path":"translation"}}]}])";
    return fixture;
}

static void CheckSkinAndAnimationMetadata()
{
    auto fixture = SkinAnimationFixture();
    auto result = Load(fixture.Bytes()); Check(bool(result),result.diagnostic.c_str());
    const auto& model = *result.asset.Model(0).Get();
    const auto& bones = model.skeleton->bones;
    Check(!model.renderable && bones.size()==2 && bones[1].name=="tip" && bones[1].parentIndex==0 &&
        bones[1].localBindMatrix[14]==100 && bones[1].inverseBind[14]==-100, "Imported skin hierarchy and inverse bind use centralized coordinate conversion");
    const auto& skin = model.meshes[0].skin;
    Check(skin.influencesPerVertex==4 && skin.jointIndices[0][1]==1 &&
        std::abs(skin.jointWeights[0][0]+skin.jointWeights[0][1]-1)<1e-6f, "Imported joints and normalized weights retained in metadata");
    Check(!result.asset.Get()->CreateAnimationInstance(result.asset.Model(0)), "Skinned metadata cannot activate a fabricated animation engine");
    const auto* clip = result.asset.Animation(0).Get();
    Check(clip && clip->metadataOnly && clip->name=="move" && clip->duration==2 && clip->channels.size()==1 &&
        clip->channels[0].targetNode==2 && clip->channels[0].path==AnimationPath::Translation &&
        clip->channels[0].interpolation==AnimationInterpolation::Step, "Animation duration, targets and interpolation retained without evaluator");
    fixture.binary[96] = std::byte{2};
    Reject(fixture.Bytes(),"Skin joint outside skeleton rejected");
    fixture = SkinAnimationFixture();
    fixture.binary[108]=fixture.binary[109]=std::byte{};
    Reject(fixture.Bytes(),"Zero skin influence total rejected");
    auto json = SkinAnimationFixture().Json();
    auto pos = json.find(R"("STEP")"); json.replace(pos,6,R"("TYPO")");
    Reject(GlTFFixtures::GLB(json,SkinAnimationFixture().binary),"Unknown interpolation is rejected before cgltf defaulting");
}

static void CheckMalformedMetadata()
{
    for(const auto mode : {"OPAQUE","MASK","BLEND"}) {
        GlTFFixtures::Builder fixture; fixture.indices=R"(,"material":0)";
        fixture.extra=std::string(R"(,"materials":[{"alphaMode":")")+mode+
            R"(","alphaCutoff":0.25,"doubleSided":true,"pbrMetallicRoughness":{"baseColorFactor":[0.2,0.4,0.6,0.8]}}])";
        const auto result=Load(fixture.Bytes()); Check(bool(result),result.diagnostic.c_str());
        const auto& material=result.asset.Model(0).Get()->materials[0];
        Check(material.explicitRenderState && material.alphaTest==(std::string_view(mode)=="MASK") &&
            material.blending==(std::string_view(mode)=="BLEND") && material.depthWrite!=(std::string_view(mode)=="BLEND") &&
            material.alphaCutoff==.25f && material.baseColorFactor[2]==.6f && material.culling==Culling::None,
            "Material alpha, cutoff, color and culling translate into neutral contract");
    }
    for (const auto mode : {"4.5", "-1", "999999999999999999999999999999999999999", "true"}) {
        Triangle fixture; fixture.Replace(R"("indices":1)",std::string(R"("indices":1,"mode":)")+mode);
        Reject(fixture.Bytes(),"Invalid primitive numeric enum rejected");
    }
    for(const auto offset : {"-1","0.5","true","{}","[]","999999999999999999999999999999999999999"}) {
        Triangle fixture; fixture.Replace(R"("byteOffset":0)",std::string(R"("byteOffset":)")+offset);
        Reject(fixture.Bytes(),"Invalid accessor size/offset number rejected");
    }
    { GlTFFixtures::Builder fixture; fixture.indices=R"(,"material":0)"; fixture.extra=R"(,"materials":[{"alphaMode":"TYPO"}])";
      Reject(fixture.Bytes(),"Unknown alpha mode rejected before cgltf defaulting"); }
    for(const auto value : {"{}","[]","null","true"}) {
        GlTFFixtures::Builder fixture; fixture.indices=R"(,"material":0)";
        fixture.extra=std::string(R"(,"materials":[{"alphaMode":)")+value+"}]";
        Reject(fixture.Bytes(),"Non-string alpha mode rejected before cgltf defaulting");
    }
    { GlTFFixtures::Builder fixture; fixture.indices=R"(,"material":0)"; fixture.extra=R"(,"materials":[{"alphaMode":"MASK","alphaCutoff":2}])";
      const auto result=Load(fixture.Bytes()); Check(bool(result) && result.asset.Model(0).Get()->materials[0].alphaCutoff==2,"Valid MASK cutoff above one retained"); }
    { Triangle fixture; fixture.Replace(R"("componentType":5126)",R"("componentType":5125)");
      Reject(fixture.Bytes(),"Unsigned 32-bit POSITION is outside glTF and mesh quantization contracts"); }
    { Triangle fixture; const float nan=std::numeric_limits<float>::quiet_NaN(); std::memcpy(fixture.binary.data(),&nan,4);
      Reject(fixture.Bytes(),"Nonfinite accessor component rejected"); }
    { Triangle fixture; fixture.Replace(R"("mesh":0)",R"("mesh":0,"children":[1,1])"); fixture.Replace(R"("nodes":[{"mesh":0,"children":[1,1]}])",R"("nodes":[{"mesh":0,"children":[1,1]},{}])");
      Reject(fixture.Bytes(),"Repeated hierarchy child rejected"); }
}
int main(int argc,char** argv)
{
    try {
        const auto before=liveDocuments.load();
        CheckSparseAndNormalized();
        CheckTangentsAndNormals();
        CheckSkinAndAnimationMetadata();
        CheckMalformedMetadata();
        for(unsigned width:{1u,2u,4u}) {
            Triangle fixture(width);
            auto bytes=fixture.Bytes();
            auto loaded=Load(bytes);Check(bool(loaded),loaded.diagnostic.c_str());
            const auto v=Vertices(loaded.asset);
            Check(v[8]==100 && v[17]==0 && v[18]==100 && v[4]==-1 && v[7]==0,"central coordinates, units, normal and UV conversion");
            const auto& mesh=loaded.asset.Model(0).Get()->meshes[0];
            Check(mesh.bounds.valid && mesh.bounds.max[0]==100 && mesh.bounds.max[2]==100,"baked bounds");
            bytes.clear();bytes.shrink_to_fit();
            auto session=loaded.asset.Get()->CreateAnimationInstance(loaded.asset.Model(0));
            Check(session && session->PreparePose() && session->Evaluate({}).pose.Valid(),"static session owns imported model");
            Check(session->CreateMeshBinding(loaded.asset.Model(0),0)->BoneIndices()[0]==0,"rigid binding uses identity root");
            Check(session->SetMotion({},0,0,0,1)==AssetError::UnsupportedLayout,"no animation engine implied");
            std::array<std::uint32_t,3> indices{};
            Check(loaded.asset.Get()->CopyIndices(0,0,IndexWidth::UInt32,std::as_writable_bytes(std::span(indices)))==AssetError::None && indices[2]==2,"unsigned index widths widened correctly");
            loaded.asset.ReleaseUploadData();
            std::array<std::byte,96> output{};
            Check(loaded.asset.Get()->CopyVertices(0,0,VertexLayout::PositionNormalUV,output)==AssetError::UploadDataReleased,"upload release contract");
            Check(session->Evaluate({}).pose.Valid(),"session survives upload release");
        }
        {
            Triangle fixture;fixture.Replace(R"("nodes":[{"mesh":0}])",R"("nodes":[{"translation":[1,2,3],"children":[1,2]},{"mesh":0,"scale":[2,3,4]},{"mesh":0,"translation":[0,1,0]}])");
            const auto result=Load(fixture.Bytes());Check(bool(result),result.diagnostic.c_str());
            Check(result.asset.Model(0).Get()->meshes.size()==2,"multiple transformed mesh nodes");
            const auto v=Vertices(result.asset);Check(v[0]==100 && v[1]==-300 && v[2]==200 && v[8]==300,"hierarchical translation and nonuniform scale");
        }
        {
            Triangle fixture;fixture.Replace(R"("mesh":0)",R"("mesh":0,"scale":[-1,1,1])");
            const auto result=Load(fixture.Bytes());Check(bool(result),result.diagnostic.c_str());
            std::array<std::uint16_t,3> indices{};
            Check(result.asset.Get()->CopyIndices(0,0,IndexWidth::UInt16,std::as_writable_bytes(std::span(indices)))==AssetError::None && indices[1]==2,"mirrored node winding correction");
        }
        {
            Triangle fixture(1,false);
            const auto loaded=Load(fixture.Bytes());
            Check(bool(loaded),"generate missing normals for position-only triangle");
            const auto& mesh=loaded.asset.Model(0).Get()->meshes[0];
            Check(mesh.vertexAttributes==(VertexAttribute::Position|VertexAttribute::Normal|VertexAttribute::UV0),
                "Emitted PNT semantics satisfy existing consumer for position-only untextured input");
            const auto vertices=Vertices(loaded.asset);
            Check(vertices[6]==0 && vertices[7]==0 && vertices[14]==0 && vertices[15]==0 && vertices[22]==0 && vertices[23]==0,
                "Missing source UVs become defined zero UVs in converted stream");
        }
        auto bytes=Triangle().Bytes();
        Reject({},"missing input");
        bytes[0]=std::byte{};Reject(bytes,"bad magic");
        bytes=Triangle().Bytes();bytes.resize(bytes.size()-1);Reject(bytes,"truncated GLB");
        bytes=Triangle().Bytes();GlTFFixtures::Put32(bytes,12,0xfffffffc);Reject(bytes,"invalid chunk length");
        { Triangle fixture;fixture.Replace(R"("count":3)",R"("count":4294967295)");Reject(fixture.Bytes(),"huge accessor count"); }
        { Triangle fixture;fixture.Replace(R"("byteLength":96)",R"("byteLength":16)");Reject(fixture.Bytes(),"accessor out of bounds"); }
        { Triangle fixture;fixture.binary[96]=std::byte{9};Reject(fixture.Bytes(),"invalid mesh index"); }
        { Triangle fixture;fixture.Replace(R"("POSITION":0)",R"("TANGENT":0)");Reject(fixture.Bytes(),"missing positions"); }
        { Triangle fixture;fixture.Replace(R"("scene":0)",R"("extensionsRequired":["KHR_draco_mesh_compression"],"scene":0)");Reject(fixture.Bytes(),"required extension unsupported"); }
        { Triangle fixture;fixture.Replace(R"("mesh":0)",R"("mesh":0,"children":[0])");Reject(fixture.Bytes(),"cyclic hierarchy"); }
        { Triangle fixture;fixture.Replace(R"("indices":1)",R"("indices":1,"mode":1)");Reject(fixture.Bytes(),"unsupported line topology"); }
        { Triangle fixture;fixture.Replace(R"("mesh":0)",R"("mesh":0,"scale":[0,1,1])");Reject(fixture.Bytes(),"singular node transform"); }
        if(argc>1) {
            std::ifstream file(argv[1],std::ios::binary);
            Check(bool(file),"real fixture opens");
            const std::vector<char> source((std::istreambuf_iterator<char>(file)),{});
            const auto result=GetGlTFAssetProvider().Load(argv[1],{reinterpret_cast<const std::byte*>(source.data()),source.size()});
            Check(bool(result),result.diagnostic.c_str());
            Check(result.asset.Model(0).Get()->meshes.size()>=2 && result.asset.Model(0).Get()->materials[0].embeddedImages[0],"real textured multiple-node asset");
        }
        Check(liveDocuments==before && liveAnimationInstances==0 && liveMeshBindings==0,"document, session, binding lifetime balances");
        std::cout<<"PASS GLB conversion, ownership, transforms, index widths, validation and static sessions\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
}
