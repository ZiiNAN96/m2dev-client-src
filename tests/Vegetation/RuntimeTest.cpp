#include "Vegetation/VegetationRuntime.h"
#include "Vegetation/VegetationRenderer.h"
#include "../AssetRuntime/GlTFFixtures.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Vegetation;
void Check(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
struct MeshRenderer final : Renderer::IStaticObjectRenderer {
    unsigned uploads{},draws{};
    Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData&)override{return std::make_shared<Renderer::TerrainTexture>();}
    Renderer::StaticObjectGeometryPtr UploadGeometry(const Renderer::StaticObjectSource&)override{++uploads;return std::make_shared<Renderer::StaticObjectGeometry>();}
    void Draw(const Renderer::StaticObjectGeometryPtr&,const Renderer::TerrainTexturePtr&,const Renderer::StaticObjectDraw&)override{++draws;}
    void ReleaseBindings()override{}
};
void LoadedContracts(){
    GlTFFixtures::Builder fixture;
    const auto colors=fixture.Accessor(fixture.View(std::array<std::uint8_t,12>{255,255,255,255,255,255,255,255,255,255,255,255}),5121,3,"VEC4",0,true);
    const auto zero=fixture.Accessor(fixture.View(std::array<float,9>{}),5126,3,"VEC3");
    const auto flex=fixture.Accessor(fixture.View(std::array<float,3>{1,1,1}),5126,3,"SCALAR");
    fixture.attributes+=",\"COLOR_0\":"+std::to_string(colors)+",\"TEXCOORD_1\":2,\"_ZIINAN_PIVOT\":"+std::to_string(zero)+",\"_ZIINAN_FLEXIBILITY\":"+std::to_string(flex)+",\"_ZIINAN_CARD_PITCH_COS\":"+std::to_string(zero)+",\"_ZIINAN_CARD_PITCH_SIN\":"+std::to_string(zero);
    Metadata m;m.geometry="vegetation/triangle.glb";m.bounds={{0,0,0},{100,0,100},true};m.renderBounds=m.bounds;m.nearDistance=1000;m.farDistance=2000;m.cullDistance=4000;m.parts={{PartKind::Branch,0,0}};m.lods.resize(2);for(auto&l:m.lods)l.meshes[0]=0;
    const auto json=SerializeMetadata(m);auto binary=fixture.Bytes();Runtime runtime;Check(bool(runtime.registry.Add("fixture.spt","vegetation/triangle.zveg")),"fixture registry");unsigned reads=0;
    auto reader=[&](std::string_view path,std::vector<std::byte>&out){++reads;if(path.ends_with(".glb"))out=binary;else{out.resize(json.size());std::memcpy(out.data(),json.data(),json.size());}return true;};
    {
        auto loaded=runtime.Load("fixture.spt",reader);Check(bool(loaded),loaded.error.c_str());Check(reads==2&&runtime.Load("FIXTURE.SPT",reader).asset==loaded.asset&&reads==2,"compiled asset sharing and cached reads");
        MeshRenderer renderer;std::string error;auto missing=Prepare(loaded.asset,renderer,[](std::string_view){return Renderer::TerrainTexturePtr{};},error);Check(!missing&&!error.empty()&&liveRenderAssets==0&&liveGeometry==0,"missing texture fails and releases geometry");
        renderer.uploads=0;unsigned textureReads=0;auto render=Prepare(loaded.asset,renderer,[&](std::string_view){++textureReads;return std::make_shared<Renderer::TerrainTexture>();},error);Check(bool(render),error.c_str());
        std::vector<std::unique_ptr<Instance>> instances;for(unsigned i=0;i<1000;++i)instances.push_back(std::make_unique<Instance>(loaded.asset,Identity,i));
        for(auto&i:instances)Check(Draw(*i,*render,renderer,{}),"shared instance draw");
        Check(renderer.uploads==1&&textureReads==1&&renderer.draws==1000&&reads==2,"1000 instances share geometry and textures without frame IO");
        runtime.Clear();Check(liveAssets==1,"instances retain cleared cached asset");
    }
    Check(liveAssets==0&&liveInstances==0&&liveRenderAssets==0&&liveGeometry==0,"loaded and GPU adapter lifetime");
    runtime.Clear();binary.resize(12);Check(!runtime.Load("fixture.spt",reader),"corrupt compiled GLB");
    runtime.Clear();auto missingGeometry=[&](std::string_view path,std::vector<std::byte>&out){if(path.ends_with(".glb"))return false;return reader(path,out);};Check(!runtime.Load("fixture.spt",missingGeometry),"missing compiled geometry");
    runtime.Clear();auto corruptMetadata=[](std::string_view,std::vector<std::byte>&out){out={std::byte{'{'}};return true;};Check(!runtime.Load("fixture.spt",corruptMetadata),"invalid ZVEG load");
    for(const auto& replacement:std::vector<std::pair<std::string,std::string>>{{"\"parts\":[[0,0,0]]","\"parts\":[[3,1,0]]"},{"\"parts\":[[0,0,0]]","\"parts\":[[0,0,1]]"},{"\"bounds\":[0.0,0.0,0.0,100.0,0.0,100.0]","\"bounds\":[10,0,0,0,0,0]"},{"\"lods\":[[0,-1,-1,-1,-1","\"lods\":[[-1,-1,-1,-1,0"}}){auto bad=json;const auto at=bad.find(replacement.first);Check(at!=bad.npos,"invalid fixture token");bad.replace(at,replacement.first.size(),replacement.second);Metadata parsed;Check(!ParseMetadata(bad,parsed),"invalid billboard/part/bounds/LOD rejected");}
}
int main(){try{
    Registry r;Check(bool(r.Add("D:\\YMIR WORK\\TREE\\fixture.spt","vegetation/fixture.zveg")),"registry add");Check(r.Resolve("d:/ymir work/tree/fixture.spt")!=nullptr,"case slash normalization");Check(!r.Resolve("missing.spt"),"invalid lookup");Check(!r.Add("d:/ymir work/tree/fixture.spt","vegetation/other.zveg"),"duplicate normalized key");Check(!r.Add("other.spt","vegetation/../outside.zveg"),"traversal");
    Registry round;Check(bool(round.Parse(r.Serialize()))&&round.Size()==1,"registry roundtrip");Check(!round.Parse("{\"version\":1,\"version\":1,\"entries\":{}}"),"duplicate JSON");Check(round.Size()==1,"failed parse is atomic");
    Metadata m;m.geometry="vegetation/fixture.glb";m.bounds={{-1,-2,0},{1,2,5},true};m.renderBounds=m.bounds;m.nearDistance=10;m.farDistance=50;m.cullDistance=100;m.parts={{PartKind::Branch,0,0},{PartKind::Billboard,0,1}};
    m.lods.resize(3);m.lods[0].meshes[4]=1;m.lods[1].meshes[0]=0;m.lods[2].meshes[0]=0;m.lods[1].alpha[0]=0;m.lods[2].alpha[0]=100;
    Metadata parsed;Check(bool(ParseMetadata(SerializeMetadata(m),parsed)),"metadata roundtrip");Check(parsed.parts.size()==2&&parsed.lods.size()==3,"metadata counts");
    Check(SelectLOD(m,0).meshes[0]==0&&SelectLOD(m,50).meshes[4]==1,"near and far LOD");Check(std::abs(SelectLOD(m,20).alpha[0]-50)<1e-5f,"LOD alpha interpolation");Check(SelectLOD(m,101).meshes[4]==-1,"distance cull");Check(SelectLOD(m,std::numeric_limits<float>::quiet_NaN()).meshes[0]==-1,"nonfinite distance");
    Matrix t=Identity;t[0]=2;t[5]=3;t[12]=100;t[13]=200;t[14]=300;auto b=TransformBounds(m.bounds,t);Check(b.min==Vec3{98,194,300}&&b.max==Vec3{102,206,305},"transformed bounds");
    Matrix rotated=Identity;rotated[0]=0;rotated[1]=1;rotated[4]=-1;rotated[5]=0;b=TransformBounds(m.bounds,rotated);Check(b.min==Vec3{-2,-1,0}&&b.max==Vec3{2,1,5},"rotation preserved");
    const std::array<std::array<float,4>,1> outside{{{1,0,0,-1000}}};Check(!Visible(m.bounds,outside),"frustum rejection");Check(WindPhase(t,1)==WindPhase(t,1)&&WindPhase(t,1)!=WindPhase(t,2),"stable independent phases");
    for(const auto token:{std::string("\"version\":1"),std::string("\"lodLimits\":[10.0,50.0,100.0]")}){auto bad=SerializeMetadata(m);const auto i=bad.find(token);Check(i!=bad.npos,"fixture field");bad.replace(i,token.size(),token.starts_with("\"version")?"\"version\":2":"\"lodLimits\":[50,10,100]");Check(!ParseMetadata(bad,parsed),"invalid metadata rejected");}
    {auto asset=std::make_shared<Asset>();asset->metadata=m;{Instance a(asset,Identity,1),b(asset,t,2);Check(a.asset==b.asset&&liveAssets==1&&liveInstances==2,"shared asset");Check(a.Update({0,0,0})&&!b.Update({0,0,0}),"per-instance LOD and cull");}Check(liveInstances==0,"instance lifetime");}Check(liveAssets==0,"asset lifetime");
    Runtime runtime;runtime.registry=r;int reads=0;auto missing=[&](std::string_view,std::vector<std::byte>&){++reads;return false;};Check(!runtime.Load("missing.spt",missing)&&reads==0,"no read for unknown key");Check(!runtime.Load("d:/ymir work/tree/fixture.spt",missing)&&reads==1,"missing compiled asset");Check(!runtime.Load("d:/ymir work/tree/fixture.spt",missing)&&reads==1,"failed loads cached; no repeated IO");
    LoadedContracts();std::cout<<"PASS vegetation registry/metadata/bounds/LOD/transform/wind/1000 shared instances/lifetime/failure contracts\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
