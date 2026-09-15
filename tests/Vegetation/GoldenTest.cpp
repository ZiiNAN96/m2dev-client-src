#include "Vegetation/VegetationRenderer.h"
#include <rapidjson/document.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace Vegetation;
namespace fs=std::filesystem;
void Check(bool ok,const std::string&why){if(!ok)throw std::runtime_error(why);}
std::vector<std::byte> Read(const fs::path&p){std::ifstream f(p,std::ios::binary|std::ios::ate);Check(bool(f),"missing "+p.string());const auto size=f.tellg();Check(size>0&&size<128*1024*1024,"read bound");std::vector<std::byte>b(static_cast<std::size_t>(size));f.seekg(0);Check(bool(f.read(reinterpret_cast<char*>(b.data()),size)),"read");return b;}
rapidjson::Document Json(const std::vector<std::byte>&b){rapidjson::Document d;d.Parse(reinterpret_cast<const char*>(b.data()),b.size());Check(!d.HasParseError(),"JSON");return d;}
void Near(double a,double b,double tolerance=.001){Check(std::isfinite(a)&&std::abs(a-b)<=tolerance,"numeric golden mismatch: "+std::to_string(a)+" != "+std::to_string(b));}
void Equal(const rapidjson::Value&a,const rapidjson::Value&b){
    if(a.IsNumber()&&b.IsNumber()){Near(a.GetDouble(),b.GetDouble());return;}
    Check(a.GetType()==b.GetType(),"golden type");
    if(a.IsArray()){Check(a.Size()==b.Size(),"golden array size");for(unsigned i=0;i<a.Size();++i)Equal(a[i],b[i]);}
    else if(a.IsString())Check(std::string(a.GetString())==b.GetString(),"golden string");
    else Check(a==b,"golden value");
}
struct Capture final:Renderer::IStaticObjectRenderer{
    std::vector<Renderer::StaticObjectDraw> draws;
    Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData&)override{return std::make_shared<Renderer::TerrainTexture>();}
    Renderer::StaticObjectGeometryPtr UploadGeometry(const Renderer::StaticObjectSource&)override{return std::make_shared<Renderer::StaticObjectGeometry>();}
    void Draw(const Renderer::StaticObjectGeometryPtr&,const Renderer::TerrainTexturePtr&texture,const Renderer::StaticObjectDraw&d)override{Check(bool(texture),"draw texture");draws.push_back(d);}
    void ReleaseBindings()override{}
};
int main(int argc,char**argv){try{
    Check(argc==3,"usage: VegetationGoldenTest compiled-directory golden-json");const fs::path compiled=argv[1];const auto goldens=Json(Read(argv[2]));Runtime runtime;
    const auto registry=Read(compiled/"vegetation/registry.json");Check(bool(runtime.registry.Parse({reinterpret_cast<const char*>(registry.data()),registry.size()})),"registry");
    unsigned reads=0,assets=0,transforms=0;
    const auto reader=[&](std::string_view p,std::vector<std::byte>&b){Check(p.ends_with(".zveg")||p.ends_with(".glb"),"legacy source must never be read");++reads;b=Read(compiled/fs::path(p));return true;};
    for(const auto&g:goldens["assets"].GetArray()){
        const std::string key=g["asset"].GetString();const auto result=runtime.Load(key,reader);Check(bool(result),result.error);const auto asset=result.asset;const auto&meta=asset->metadata;const auto&model=*asset->geometry.Model(0).Get();const auto&r=g["reference"];const auto&baseline=g["compiledBaseline"];
        const auto serialized=SerializeMetadata(meta);rapidjson::Document actual;actual.Parse(serialized.c_str());
        for(const auto*field:{"geometry","shadowTexture","bounds","renderBounds","lodLimits","wind","parts","collisions"})Equal(actual[field],baseline[field]);
        Equal(actual["bounds"],r["bounds"]);Check(meta.lods.size()==baseline["lodCount"].GetUint(),"LOD count");
        for(const auto&s:baseline["samples"].GetArray())Equal(actual["lods"][s["index"].GetUint()],s["state"]);
        unsigned meshIndex=0;
        auto meshCheck=[&](unsigned kind,unsigned lod,unsigned vertices,unsigned indices,const rapidjson::Value*golden){
            Check(meshIndex<model.meshes.size(),"part exists");const auto&part=meta.parts[meshIndex];const auto&m=model.meshes[meshIndex];
            Check(unsigned(part.kind)==kind&&part.lod==lod&&part.mesh==meshIndex&&m.vertexCount==vertices&&m.indexCount==indices,"SDK part/LOD/vertex/index counts");
            Check(m.vertexExtrasChannels==63&&m.materialBindings.size()==1,"auxiliary/material channels");const auto&mat=model.materials[m.materialBindings[0]];
            Check(mat.alphaTest&&!mat.blending&&mat.depthWrite&&mat.culling==(kind==0?AssetRuntime::Culling::Clockwise:AssetRuntime::Culling::None),"material MASK/depth/double-sided");Near(mat.alphaCutoff,84./255,1e-6);
            std::vector<Renderer::StaticObjectVertex> v(vertices);Check(asset->geometry.Get()->CopyVertices(0,meshIndex,AssetRuntime::VertexLayout::PositionNormalUV,std::as_writable_bytes(std::span(v)))==AssetRuntime::AssetError::None,"vertex copy");
            if(golden&&kind<2)for(const auto&s:(*golden)["selectedVertices"].GetArray()){
                const auto index=s["index"].GetUint();for(unsigned k=0;k<3;++k)Near(v[index][k],s["position"][k].GetDouble(),.05);for(unsigned k=0;k<2;++k)Near(v[index][6+k],s["uv"][k].GetDouble(),1e-5);
                const auto c=s["color"].GetUint();const unsigned shifts[]={16,8,0,24};for(unsigned k=0;k<4;++k)Near(m.vertexExtras[index].color[k],double((c>>shifts[k])&255)/255,1e-6);
            }
            if(golden&&kind==2)for(const auto&s:(*golden)["selectedLeaves"].GetArray())for(unsigned corner=0;corner<4;++corner){
                const auto index=s["index"].GetUint()*4+corner;for(unsigned k=0;k<3;++k)Near(v[index][k],s["center"][k].GetDouble()+s["offsets"][corner*4+k].GetDouble(),.05);for(unsigned k=0;k<2;++k)Near(v[index][6+k],s["uv"][corner*2+k].GetDouble(),1e-5);
            }
            ++meshIndex;
        };
        unsigned kind=0;for(const auto*field:{"branchLODs","frondLODs"}){unsigned lod=0;for(const auto&e:r[field].GetArray()){const auto&v=e["geometry"];if(v["triangles"].GetUint())meshCheck(kind,lod,v["vertices"].GetUint(),v["triangles"].GetUint()*3,&v);++lod;}++kind;}
        unsigned lod=0;for(const auto&v:r["leafLODs"].GetArray()){const unsigned count=v["count"].GetUint();if(count)meshCheck(2,lod,count*4,count*6,&v);++lod;}meshCheck(3,0,4,6,nullptr);Check(meshIndex==model.meshes.size(),"all parts covered");
        for(const auto&t:g["textures"].GetArray()){
            const std::string path=t["path"].GetString();if(std::string(t["kind"].GetString())=="selfShadow")Check(meta.shadowTexture==path,"shadow texture reference");
            else Check(std::any_of(model.materials.begin(),model.materials.end(),[&](const auto&m){return m.textures[0]==path;}),"SDK texture reference");
        }
        for(const auto&t:g["selectedMapTransforms"].GetArray())if(t.HasMember("transform")){
            Matrix world=Identity;std::istringstream position(t["transform"][0].GetString());position>>world[12]>>world[13]>>world[14];Check(bool(position),"real map position");world[14]+=std::stof(t["transform"][3].GetString());
            // Legacy tree placement consumes translation + height offset; stored object Euler angles do not rotate camera-facing vegetation.
            Instance tree(asset,world);const auto bounds=TransformBounds(meta.bounds,tree.transform);for(unsigned k=0;k<3;++k){Near(bounds.min[k],meta.bounds.min[k]+world[12+k],.02);Near(bounds.max[k],meta.bounds.max[k]+world[12+k],.02);}Check(tree.transform==world&&tree.phase==WindPhase(world),"real map transform/phase preserved");++transforms;
        }
        Capture renderer;std::string error;auto render=Prepare(asset,renderer,[](std::string_view){return std::make_shared<Renderer::TerrainTexture>();},error);Check(bool(render),error);Instance instance(asset,Identity);RenderContext context;context.windStrength=1;context.time=1.25f;context.state.fog=Renderer::TerrainFog::Exp;context.state.fogParameters={0,0,.000004f,0};const auto readsBefore=reads;
        for(float level:{1.f,.5f,0.f,.5f,1.f}){
            context.camera={0,meta.farDistance-level*(meta.farDistance-meta.nearDistance),0};renderer.draws.clear();Check(Draw(instance,*render,renderer,context),"LOD visible");Check(!renderer.draws.empty()&&renderer.draws.size()<=5,"bounded draw count");
            unsigned index=0;for(unsigned slot=0;slot<5;++slot)if(instance.lod.meshes[slot]>=0){const auto&d=renderer.draws[index++];const auto kind=meta.parts[instance.lod.meshes[slot]].kind;
                Check(d.alphaTest==Renderer::StaticObjectAlphaTest::Greater&&d.alphaReference==unsigned(instance.lod.alpha[slot])&&d.depthWrite&&!d.blend,"per-LOD alpha/depth/blend state");Check(d.cull==(kind==PartKind::Branch?Renderer::StaticObjectCull::Clockwise:Renderer::StaticObjectCull::None),"part culling");
                const auto&w=meta.wind;const float amplitude=kind==PartKind::Leaf?w.leafAmplitude:kind==PartKind::Branch?w.branchAmplitude:kind==PartKind::Frond?w.frondAmplitude:0;Near(d.wind[1],amplitude*w.strength);Near(d.wind[0],context.time+instance.phase);Near(d.wind[2],w.frequency);
                if(kind==PartKind::Leaf){Check(d.cardFog&&d.cardMode==1,"leaf card fog");Near(d.fogParameters[0],0);Near(d.fogParameters[1],2.3f/.000004f,.1);}if(kind==PartKind::Billboard)Check(d.cardMode==2,"billboard material");
            }
            if(level==0)Check(instance.lod.meshes[4]>=0,"far billboard");else Check(instance.lod.meshes[4]<0,"near/mid geometry");
        }
        Check(reads==readsBefore&&runtime.Load(key,reader).asset==asset&&reads==readsBefore,"shared compiled asset/no frame IO");++assets;std::cout<<"PASS "<<key<<'\n';
    }
    runtime.Clear();Check(assets==9&&transforms>0&&liveAssets==0&&liveInstances==0&&liveRenderAssets==0&&liveGeometry==0,"coverage/lifetime");std::cout<<"PASS nine SDK-free goldens, "<<transforms<<" real placements, counts/vertices/materials/LOD/wind/alpha, resources=0\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
