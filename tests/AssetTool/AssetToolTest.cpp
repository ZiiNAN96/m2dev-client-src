#include "AssetTool/Scene.h"
#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>

using namespace ZiiNAN::AssetTool;
namespace fs = std::filesystem;
static void Check(bool value, const std::string& message) { if (!value) throw std::runtime_error(message); }
static bool Near(double a, double b) { return std::abs(a-b) < 1e-4; }
static std::string Errors(const Report& r) {
    std::string text;
    for (const auto& i : r.issues) text += i.code + ": " + i.context + ": " + i.message + "\n";
    return text;
}
static void ProcessOK(Scene& scene, const Options& options = {}) {
    Report report; const bool ok = Process(scene,options,report);
    Check(ok && report.Ok(), "Process failed: " + Errors(report));
}
static Scene Triangle() {
    Scene scene; scene.name="triangle"; scene.nodes.resize(1); scene.nodes[0].name="root"; scene.nodes[0].meshes={0};
    scene.materials.resize(1); scene.meshes.resize(1);
    auto& mesh=scene.meshes[0]; mesh.name="triangle"; mesh.vertices.resize(3); mesh.indices={0,1,2}; mesh.hasNormals=true; mesh.hasUV=true;
    mesh.vertices[1].position={1,0,0}; mesh.vertices[2].position={0,1,0};
    for (auto& v:mesh.vertices) v.normal={0,0,1};
    mesh.vertices[1].uv={1,0}; mesh.vertices[2].uv={0,1};
    return scene;
}
static Scene Skin() {
    auto scene=Triangle(); scene.meshes[0].skinned=true;
    for (unsigned i=0;i<5;++i) {
        Node node; node.name="joint"+std::to_string(i); node.parent=i?int(i):0;
        scene.nodes.push_back(node);
        scene.skeleton.joints.push_back({node.name,i+1,Identity});
    }
    scene.skeleton.root=1;
    for (auto& v:scene.meshes[0].vertices) v.influences={{0,.5f},{1,.4f},{2,.3f},{3,.2f},{4,.1f}};
    return scene;
}
static Scene Animated() {
    auto scene=Triangle(); scene.nodes[0].useTRS=true;
    Animation animation; animation.name="rotation"; animation.duration=2;
    Channel channel; channel.node=0; channel.path=AnimationPath::Rotation;
    channel.times={0,2}; channel.values={{0,0,0,2},{0,0,0,-3}};
    animation.channels.push_back(channel); scene.animations.push_back(animation);
    return scene;
}
static void Reject(Scene scene, const char* reason) {
    Report report;
    Check(!Process(scene,{},report) && !report.Ok() && !report.issues.empty(),std::string("Expected diagnosed rejection: ")+reason);
}
using TriangleSignature = std::array<float,24>;
static std::vector<TriangleSignature> Triangles(const Mesh& mesh) {
    std::vector<TriangleSignature> result;
    for (std::size_t i=0;i<mesh.indices.size();i+=3) {
        TriangleSignature triangle{};
        for(std::size_t c=0;c<3;++c) {
            const auto& v=mesh.vertices[mesh.indices[i+c]];
            std::copy(v.position.begin(),v.position.end(),triangle.begin()+c*8);
            std::copy(v.normal.begin(),v.normal.end(),triangle.begin()+c*8+3);
            std::copy(v.uv.begin(),v.uv.end(),triangle.begin()+c*8+6);
        }
        result.push_back(triangle);
    }
    return result;
}
static Scene Grid() {
    auto scene=Triangle(); auto& mesh=scene.meshes[0]; mesh.vertices.clear(); mesh.indices.clear();
    for (unsigned y=0;y<9;++y) for (unsigned x=0;x<9;++x) {
        Vertex vertex; vertex.position={float(x),float(y),0}; vertex.normal={0,0,1}; vertex.uv={x/8.f,y/8.f}; mesh.vertices.push_back(vertex);
    }
    // Deliberately uncached traversal, but deterministic and topologically unchanged.
    for(unsigned k=0;k<64;++k) {
        const unsigned cell=(k*17)%64, x=cell%8, y=cell/8, a=y*9+x;
        mesh.indices.insert(mesh.indices.end(),{a,a+1,a+10,a,a+10,a+9});
    }
    return scene;
}
static void UnitTests() {
    auto scene=Triangle(); ProcessOK(scene);
    Check(scene.bounds.valid && Near(scene.bounds.max[0],1) && Near(scene.bounds.max[1],1),"finite derived model bounds");
    scene=Triangle(); scene.meshes[0].indices[2]=8; Reject(scene,"invalid mesh index");
    scene=Triangle(); scene.meshes.clear(); Reject(scene,"no meshes");
    scene=Triangle(); scene.meshes[0].vertices.clear(); Reject(scene,"empty mesh vertices");
    scene=Triangle(); scene.meshes[0].indices.clear(); Reject(scene,"empty indices");
    scene=Triangle(); scene.meshes[0].vertices[0].position[0]=std::numeric_limits<float>::infinity(); Reject(scene,"nonfinite position");
    scene=Triangle(); scene.meshes[0].material=99; Reject(scene,"invalid material");
    scene=Triangle(); scene.nodes[0].parent=0; Reject(scene,"self cycle");
    scene=Triangle(); scene.nodes.resize(2); scene.nodes[0].parent=1; scene.nodes[1].parent=0; Reject(scene,"two-node cycle");
    scene=Triangle(); scene.nodes[0].transform[0]=0; Reject(scene,"singular node transform");
    scene=Triangle(); scene.meshes[0].indices={0,0,1};
    { Report report; Check(Process(scene,{},report) && report.Ok() && !report.issues.empty(),"degenerate triangles reported, not silently removed"); Check(scene.meshes[0].indices.size()==3,"degenerate topology preserved"); }
    scene=Triangle(); scene.meshes[0].hasNormals=false; for(auto& v:scene.meshes[0].vertices) v.normal={};
    { Options o; o.generateTangents=true; ProcessOK(scene,o); Check(scene.meshes[0].hasNormals && scene.meshes[0].hasTangents,"normal/tangent generation policy"); }
    scene=Triangle(); for(auto& v:scene.meshes[0].vertices) v.normal={0,0,2};
    { Report report; Check(Process(scene,{},report) && scene.meshes[0].vertices[0].normal==Vec3{0,0,1},"authored normal length normalized without changing direction");
      Check(std::any_of(report.issues.begin(),report.issues.end(),[](const Issue& issue){return issue.code=="normals_normalized";}),"material normal normalization is reported"); }
    scene=Triangle(); scene.meshes[0].vertices[0].normal={0,0,2};
    { Report report; Check(!Validate(scene,report),"validator requires unit normals before export"); }
    scene=Triangle(); scene.meshes[0].hasTangents=true; for(auto& v:scene.meshes[0].vertices) v.tangent={2,0,1,-1}; ProcessOK(scene);
    Check(scene.meshes[0].vertices[0].tangent==Vec4{1,0,0,-1},"tangent normalizes and orthogonalizes while preserving handedness");
    scene=Triangle(); scene.meshes[0].hasTangents=true; for(auto& v:scene.meshes[0].vertices) v.tangent={0,0,1,1}; Reject(scene,"parallel tangent");
    scene=Triangle(); scene.meshes[0].hasTangents=true; for(auto& v:scene.meshes[0].vertices) v.tangent={1,0,0,0}; Reject(scene,"invalid tangent handedness");
    scene=Skin(); ProcessOK(scene);
    for (const auto& vertex:scene.meshes[0].vertices) {
        Check(vertex.influences.size()==4,"five influences reduced to four");
        float sum=0; for(std::size_t i=0;i<4;++i) { Check(vertex.influences[i].joint==i,"stable weight and joint ordering"); sum+=vertex.influences[i].weight; }
        Check(Near(sum,1),"reduced weights normalized");
    }
    scene=Skin(); for(auto& v:scene.meshes[0].vertices) v.influences={{3,1},{1,1},{4,1},{2,1},{0,1}}; ProcessOK(scene);
    Check(scene.meshes[0].vertices[0].influences[0].joint==0 && scene.meshes[0].vertices[0].influences[3].joint==3,"weight ties use ascending joint id");
    scene=Skin(); for(auto& v:scene.meshes[0].vertices) v.influences={{0,.2f},{1,.4f},{0,.4f}}; ProcessOK(scene);
    Check(scene.meshes[0].vertices[0].influences.size()==2 && scene.meshes[0].vertices[0].influences[0].joint==0 && Near(scene.meshes[0].vertices[0].influences[0].weight,.6),"duplicate joint weights merged before reduction");
    scene=Skin(); scene.meshes[0].vertices[0].influences[0].joint=99; Reject(scene,"joint index outside skeleton");
    scene=Skin(); scene.meshes[0].vertices[0].influences[0].weight=-1; Reject(scene,"negative weight");
    scene=Skin(); scene.meshes[0].vertices[0].influences[0].weight=std::numeric_limits<float>::quiet_NaN(); Reject(scene,"nonfinite weight");
    scene=Skin(); scene.meshes[0].vertices[0].influences={{0,0}}; Reject(scene,"zero weight sum");
    scene=Skin(); scene.skeleton.joints[0].node=99; Reject(scene,"invalid bone node");
    scene=Animated(); ProcessOK(scene);
    Check(Near(scene.animations[0].channels[0].values[0][3],1) && Near(scene.animations[0].channels[0].values[1][3],1),"quaternion normalization and sign continuity");
    scene=Animated(); scene.animations[0].duration=4; ProcessOK(scene);
    Check(scene.animations[0].channels[0].times==std::vector<float>({0,2,4}) && scene.animations[0].channels[0].values[1]==scene.animations[0].channels[0].values[2],"declared trailing animation hold is represented by a final identical key");
    scene=Animated(); scene.animations[0].channels[0].times={2,1}; Reject(scene,"descending animation time");
    scene=Animated(); scene.animations[0].channels[0].times={0,0}; Reject(scene,"duplicate animation time");
    scene=Animated(); scene.animations[0].channels[0].node=99; Reject(scene,"invalid animation target");
    scene=Animated(); scene.animations[0].channels[0].values[0]={}; Reject(scene,"zero quaternion");
    scene=Grid(); auto before=Triangles(scene.meshes[0]); ProcessOK(scene); auto after=Triangles(scene.meshes[0]);
    std::sort(before.begin(),before.end()); std::sort(after.begin(),after.end()); Check(before==after,"cache/fetch optimization preserves full triangle attributes and winding");
    Check(scene.optimization.indicesBefore==scene.optimization.indicesAfter && scene.optimization.cacheMissRatioAfter<=scene.optimization.cacheMissRatioBefore,"grid optimization measured without changing triangle count");
    Scene lod; Report report; Check(MakeStaticLOD(scene,.5f,lod,report) && report.Ok(),"static LOD preparation succeeds");
    Check(lod.meshes[0].indices.size()<scene.meshes[0].indices.size() && scene.meshes[0].indices.size()==384,"LOD is reduced separate geometry, LOD0 unchanged");
    scene=Grid(); scene.materials[0].alpha=AlphaMode::Blend; before=Triangles(scene.meshes[0]); ProcessOK(scene);
    Check(before==Triangles(scene.meshes[0]),"BLEND optimization preserves original triangle order");
    scene=Skin(); ProcessOK(scene); report={}; Check(!MakeStaticLOD(scene,.5f,lod,report),"skinned LOD explicitly rejected");
    std::cout<<"PASS neutral scene validation, weights/quaternions, deterministic geometry optimization and static LOD\n";
}
static Scene Imported(const fs::path& input, const Options& options={}) {
    Scene scene; Report report; const bool ok=Import(input,options,scene,report);
    Check(ok && report.Ok(),"Import failed for "+PathUTF8(input)+": "+Errors(report));
    ProcessOK(scene,options); return scene;
}
static std::vector<std::byte> Bytes(const fs::path& path) {
    std::ifstream stream(path,std::ios::binary|std::ios::ate); Check(bool(stream),"read roundtrip output");
    const auto size=stream.tellg(); Check(size>0,"nonempty GLB");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size)); stream.seekg(0); stream.read(reinterpret_cast<char*>(bytes.data()),bytes.size());
    Check(bool(stream),"complete GLB read"); return bytes;
}
static AssetRuntime::LoadResult Roundtrip(const Scene& scene, const fs::path& path) {
    Report report; Check(WriteGLB(scene,path,report),"Write failed: "+Errors(report));
    auto result=AssetRuntime::GetGlTFAssetProvider().Load(PathUTF8(path),Bytes(path));
    Check(bool(result),"Real E1-X GlTFProvider failed: "+result.diagnostic);
    return result;
}
static void ImportTests(const fs::path& fixtures,const fs::path& output) {
    fs::create_directories(output);
    const auto simple=Imported(fixtures/"simple.obj");
    Check(simple.meshes.size()==1 && simple.meshes[0].indices.size()==6 && simple.meshes[0].hasNormals && simple.meshes[0].hasUV,"OBJ quad triangulates, normal generation and UV preserved");
    auto loaded=Roundtrip(simple,output/"simple.glb");
    Check(loaded.asset.Model(0).Get()->renderable,"static exported GLB renderable through existing provider"); loaded.asset.Reset();
    {
        const auto path=output/"undefined_material.obj";
        { std::ofstream stream(path); stream<<"v 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl undeclared_material\nf 1 2 3\n"; Check(bool(stream),"write undefined-material source fixture"); }
        Scene substituted; Report sourceReport;
        Check(Import(path,Options{},substituted,sourceReport),"Assimp recovers source with undefined material");
        const bool warned=std::any_of(sourceReport.issues.begin(),sourceReport.issues.end(),[](const Issue& issue){return issue.code=="assimp_source" && issue.message.find("failed to locate material")!=std::string::npos;});
        Check(warned,"Assimp material substitution is forwarded to structured diagnostics");
    }
    const auto dae=Imported(fixtures/"transformed_cm.dae");
    Check(Near(dae.sourceMetersPerUnit,.01),"DAE declared centimeter provenance retained for inspection");
    Check(Near(dae.bounds.min[0],1) && Near(dae.bounds.min[1],2) && Near(dae.bounds.min[2],3) && Near(dae.bounds.max[0],3) && Near(dae.bounds.max[1],3),"DAE centimeter metadata plus nested node transforms normalize to meters once");
    loaded=Roundtrip(dae,output/"transformed.glb");
    const auto& runtimeBounds=loaded.asset.Model(0).Get()->meshes[0].bounds;
    Check(Near(runtimeBounds.min[0],100) && Near(runtimeBounds.min[1],-300) && Near(runtimeBounds.min[2],200) && Near(runtimeBounds.max[0],300),"meter GLB normalizes through unchanged E1-X runtime to centimeter Z-up bounds"); loaded.asset.Reset();
    const auto fbx=Imported(fixtures/"simple_cm.fbx");
    Check(Near(fbx.bounds.min[0],1) && Near(fbx.bounds.min[1],2) && Near(fbx.bounds.min[2],3) && Near(fbx.bounds.max[0],2),"FBX declared centimeters and translated node normalized once");
    loaded=Roundtrip(fbx,output/"simple_fbx.glb"); loaded.asset.Reset();
    {
        const auto source=Bytes(fixtures/"simple_cm.fbx");
        std::string text(reinterpret_cast<const char*>(source.data()),source.size());
        const std::string centimeters="P: \"UnitScaleFactor\", \"double\", \"Number\", \"\",1";
        const auto position=text.find(centimeters); Check(position!=std::string::npos,"FBX unit fixture source marker");
        text.replace(position,centimeters.size(),"P: \"UnitScaleFactor\", \"double\", \"Number\", \"\",100");
        const auto path=output/"meter_units.fbx"; { std::ofstream stream(path); stream<<text; Check(bool(stream),"write meter-unit fixture variant"); }
        const auto meters=Imported(path);
        Check(Near(meters.sourceMetersPerUnit,1) && Near(meters.bounds.min[0],100) && Near(meters.bounds.min[1],200) && Near(meters.bounds.max[0],200),"FBX UnitScaleFactor is honored beyond default centimeters");
    }
    auto hold=Animated(); hold.animations[0].duration=4; ProcessOK(hold);
    loaded=Roundtrip(hold,output/"animation_hold.glb");
    Check(loaded.asset.AnimationCount()==1 && Near(loaded.asset.Animation(0).Get()->duration,4),"declared trailing hold survives writer/provider duration roundtrip"); loaded.asset.Reset();
    const auto alpha=Imported(fixtures/"alpha.obj");
    const auto& alphaMaterial=alpha.materials[alpha.meshes[0].material];
    Check(alphaMaterial.alpha==AlphaMode::Blend && Near(alphaMaterial.baseColor[3],.4),"OBJ source material opacity preserved");
    for(const auto& vertex:alpha.meshes[0].vertices) Check(Near(vertex.normal[2],1),"authored normal preserved");
    loaded=Roundtrip(alpha,output/"alpha.glb");
    bool blending=false; for(const auto& material:loaded.asset.Model(0).Get()->materials)
        blending |= material.blending && Near(material.baseColorFactor[3],.4) && Near(material.baseColorFactor[0],.2);
    Check(blending,"source diffuse factor and opacity survive GLB writer/provider"); loaded.asset.Reset();
    const auto skin=Imported(fixtures/"skinned_animation.dae");
    Check(skin.skeleton.joints.size()==2 && !skin.animations.empty() && Near(skin.animations[0].duration,2),"source skin, joints and seconds animation imported");
    loaded=Roundtrip(skin,output/"skin.glb");
    Check(!loaded.asset.Model(0).Get()->renderable && loaded.asset.Model(0).Get()->skeleton->bones.size()==2 && loaded.asset.AnimationCount()>0 && Near(loaded.asset.Animation(0).Get()->duration,2),"exported skin/animation retained as metadata only by E1-X provider"); loaded.asset.Reset();
    {
        const auto source=Bytes(fixtures/"skinned_animation.dae");
        std::string text(reinterpret_cast<const char*>(source.data()),source.size());
        const auto position=text.find("LINEAR LINEAR"); Check(position!=std::string::npos,"DAE interpolation fixture marker");
        text.replace(position,std::string("LINEAR LINEAR").size(),"STEP STEP");
        const auto path=output/"step_interpolation.dae"; { std::ofstream stream(path); stream<<text; Check(bool(stream),"write STEP source fixture variant"); }
        Scene stepped; Report stepReport;
        Check(Import(path,Options{},stepped,stepReport),"pinned Assimp STEP source imports sampled keys");
        const bool warned=std::any_of(stepReport.issues.begin(),stepReport.issues.end(),[](const Issue& issue){return issue.code=="source_animation_interpolation";});
        Check(warned,"source interpolation loss inside Assimp is explicitly reported, never claimed as exact STEP preservation");
    }
    {
        const auto source=Bytes(fixtures/"transformed_cm.dae");
        std::string text(reinterpret_cast<const char*>(source.data()),source.size());
        const auto position=text.find("<up_axis>Y_UP</up_axis>"); Check(position!=std::string::npos,"DAE up-axis fixture marker");
        text.replace(position,std::string("<up_axis>Y_UP</up_axis>").size(),"<up_axis>Z_UP</up_axis>");
        const auto path=output/"z_up.dae"; { std::ofstream stream(path); stream<<text; Check(bool(stream),"write Z-up source fixture variant"); }
        const auto zUp=Imported(path);
        Check(Near(zUp.sourceMetersPerUnit,.01) && Near(zUp.bounds.min[0],1) && Near(zUp.bounds.min[1],3) && Near(zUp.bounds.min[2],-3) && Near(zUp.bounds.max[2],-2),"DAE Z-up rotation and centimeters normalize once");
    }
    const auto stall=Imported(fixtures/"market_stall.obj");
    Check(stall.meshes.size()>=8 && stall.materials.size()>=4 && stall.images.size()==1 && stall.images[0].width==16,"realistic OBJ multi-mesh, materials and backslash texture lookup");
    loaded=Roundtrip(stall,output/"market_stall.glb");
    bool embedded=false; for(const auto& material:loaded.asset.Model(0).Get()->materials) embedded|=bool(material.embeddedImages[0]);
    Check(embedded && loaded.asset.Model(0).Get()->meshes.size()>=8,"embedded texture survives real provider roundtrip"); loaded.asset.Reset();
    auto repeated=Imported(fixtures/"market_stall.obj"); Report report;
    Check(WriteGLB(repeated,output/"market_stall_again.glb",report),"repeat independent import and export");
    Check(Bytes(output/"market_stall.glb")==Bytes(output/"market_stall_again.glb"),"same source and options produce byte-identical GLB");
    const auto unicodeDir=output/fs::path(std::u8string(u8"spaces and ünicode")); fs::create_directories(unicodeDir);
    fs::copy_file(fixtures/"simple.obj",unicodeDir/"input.obj",fs::copy_options::overwrite_existing);
    loaded=Roundtrip(Imported(unicodeDir/"input.obj"),unicodeDir/"output.glb"); loaded.asset.Reset();
    Options centimeters; centimeters.sourceMetersPerUnit=.01; const auto overridden=Imported(fixtures/"simple.obj",centimeters);
    Check(Near(overridden.bounds.max[0],.01),"explicit source unit override");
    Check(AssetRuntime::liveDocuments==0 && AssetRuntime::liveAnimationInstances==0 && AssetRuntime::liveMeshBindings==0,"roundtrip provider owners all released");
    std::cout<<"PASS OBJ/DAE/FBX import, metadata unit/node normalization, GLB provider roundtrip, skin/animation metadata, image embedding, Unicode path, byte determinism\n";
    std::cout<<ToJSON(stall,Report{})<<'\n';
}
int main(int argc,char** argv) {
    try {
        Check(argc==2 || argc==4,"Expected --unit or --import fixture-dir output-dir");
        if(std::string(argv[1])=="--unit") UnitTests();
        else { Check(argc==4 && std::string(argv[1])=="--import","unknown test mode"); ImportTests(fs::u8path(argv[2]),fs::u8path(argv[3])); }
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}
