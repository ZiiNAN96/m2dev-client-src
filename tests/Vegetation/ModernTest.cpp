#include "Vegetation/VegetationRuntime.h"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Vegetation;
void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
bool Read(const std::filesystem::path&root,std::string_view path,std::vector<std::byte>&bytes){std::ifstream in(root/path,std::ios::binary|std::ios::ate);if(!in)return false;bytes.resize(std::size_t(in.tellg()));in.seekg(0);return bool(in.read(reinterpret_cast<char*>(bytes.data()),bytes.size()));}
int main(int argc,char**argv){try{
    Metadata m;m.version=2;m.geometry="vegetation/test.glb";m.bounds={{-100,-100,0},{100,100,200},true};m.renderBounds={{-120,-120,-10},{120,120,220},true};m.nearDistance=10;m.farDistance=80;m.cullDistance=120;m.lodDistances={20,40,80};
    m.parts={{PartKind::Leaf,0,0},{PartKind::Leaf,1,1},{PartKind::Leaf,2,2},{PartKind::Billboard,0,3}};m.lods.resize(4);
    for(unsigned i=0;i<3;++i)m.lods[3-i].meshes[2]=i;m.lods[0].meshes[4]=3;
    Metadata round;Check(bool(ParseMetadata(SerializeMetadata(m),round)),"v2 roundtrip");
    for(unsigned i=0;i<3;++i){auto blend=SelectModernLOD(m,m.lodDistances[i]);Check(blend.first.meshes==m.lods[3-i].meshes&&blend.second.meshes==m.lods[2-i].meshes&&std::abs(blend.transition-.5f)<.0001f,"complementary LOD midpoint");}
    Check(SelectModernLOD(m,5).first.meshes[2]==0&&SelectModernLOD(m,110).first.meshes[4]==3,"near and impostor");
    Check(SelectModernLOD(m,121).first.meshes[4]==-1&&SelectModernLOD(m,std::numeric_limits<float>::quiet_NaN()).first.meshes[2]==-1,"invalid distance rejected");
    auto bad=m;bad.lodDistances={40,39,80};bool rejected=false;try{SerializeMetadata(bad);}catch(...){rejected=true;}Check(rejected,"overlapping LOD range rejected");
    bad=m;bad.renderBounds=m.bounds;rejected=false;try{SerializeMetadata(bad);}catch(...){rejected=true;}Check(rejected,"insufficient animated bounds rejected");
    const auto planes=FrustumPlanes(Identity,Identity);Check(Visible({{-.5f,-.5f,.1f},{.5f,.5f,.9f},true},planes),"inside frustum");
    Check(!Visible({{2,0,0},{3,1,1},true},planes)&&!Visible({{0,0,-2},{1,1,-1},true},planes),"side and near plane rejected");
    const auto candidates=GrassCandidates(77,-2,3,400),repeat=GrassCandidates(77,-2,3,400),other=GrassCandidates(78,-2,3,400);
    Check(candidates.size()==24&&candidates[0].x==repeat[0].x&&candidates[0].id==repeat[0].id&&candidates[0].id!=other[0].id,"map/cell/type deterministic candidates");
    const auto placement=PlaceGrass(candidates[0],123,.75f);const auto transform=placement.Transform();
    Check(sizeof(GrassPlacement)==32&&ValidTransform(transform),"compact map placement");
    Check(transform[12]==candidates[0].x&&transform[13]==-candidates[0].y&&transform[14]==123&&
        placement.phase==WindPhase(transform,candidates[0].id)&&placement.rank==candidates[0].rank/.75f,"preloaded terrain height, handedness, wind and density retained");
    unsigned previous=0;float distance=0;for(unsigned q=0;q<4;++q){const auto quality=ResolveQuality(q);unsigned count=0;for(auto&c:candidates){Check(c.x>=-800&&c.x<-400&&c.y>=1200&&c.y<1600,"cell ownership");if(c.rank<quality.grassDensity)++count;}Check(count>=previous&&quality.grassDistance>distance,"monotonic quality subset");previous=count;distance=quality.grassDistance;}
    WindProfile wind;wind.branchAmplitude=.02f;
    Check(BranchDisplacement(0,1000,1,1,wind,{1,0,0})==Vec3{},"root anchored");
    auto a=BranchDisplacement(1,1000,60.f/60,1,wind,{1,0,0}),b=BranchDisplacement(1,1000,120.f/120,1,wind,{1,0,0});Check(a==b&&a[0]!=0&&a[1]==0,"absolute time wind and world direction");
    if(argc==2){
        Runtime runtime;const std::filesystem::path root=argv[1];auto read=[&](std::string_view path,std::vector<std::byte>&bytes){return Read(root,path,bytes);};
        for(const auto*name:{"beech","grass","bush"}) {const auto path=std::string("vegetation/modern/")+name+".zveg";auto loaded=runtime.LoadCompiled(path,read);Check(bool(loaded),loaded.error.c_str());Check(loaded.asset->metadata.version==2,"modern generated metadata");Check(runtime.LoadCompiled(path,read).asset==loaded.asset,"shared compiled asset");}
        Check(bool(runtime.registry.Add("test.spt","vegetation/modern/beech.zveg")),"fallback registration");
        Check(bool(runtime.registry.AddOverride("test.spt","vegetation/modern/missing.zveg")),"optional override registration");
        auto fallback=runtime.Load("test.spt",read),modern=runtime.Load("test.spt",read,true);Check(fallback&&modern&&fallback.asset==modern.asset,"missing override retains existing tree");
        Registry serialized;Check(bool(serialized.Parse(runtime.registry.Serialize()))&&serialized.ResolveOverride("test.spt"),"override roundtrip");
        auto invalid=[](std::string_view,std::vector<std::byte>&bytes){bytes={std::byte{'{'}};return true;};Check(!runtime.LoadCompiled("vegetation/invalid.zveg",invalid),"invalid compiled file rejected");
    }
    if(argc==3) {
        const std::filesystem::path root=argv[2];Runtime runtime;std::vector<std::byte>bytes;Check(Read(root,"vegetation/registry.json",bytes),"legacy registry file");
        Check(bool(runtime.registry.Parse({reinterpret_cast<const char*>(bytes.data()),bytes.size()})),"legacy registry parse");
        for(const auto&[key,path]:runtime.registry.Entries()) {
            auto loaded=runtime.Load(key,[&](std::string_view p,std::vector<std::byte>&data){return Read(root,p,data);},true);Check(bool(loaded),loaded.error.c_str());
            Instance tree(loaded.asset,Identity);Check(tree.Update({0,0,0}),"legacy near visibility");
            Check(SelectLOD(loaded.asset->metadata,loaded.asset->metadata.farDistance).meshes[4]>=0,"legacy far representation");
        }
        Check(runtime.registry.Size()==118,"all 118 legacy compiled types load");std::cout<<"PASS 118/118 compiled legacy vegetation types, near and far\n";
    }
    Check(liveAssets==0&&liveInstances==0,"portable resource lifetime");std::cout<<"PASS H2 metadata/LOD/culling/wind/grass/quality/asset-sharing/override contracts\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
