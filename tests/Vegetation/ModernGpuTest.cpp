#include "Vegetation/VegetationRenderer.h"
#include "Renderer/DiligentD3D11Backend.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/ModernFrame.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Platform/PlatformWindow.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <chrono>
#include <thread>
#include <stdexcept>
namespace fs=std::filesystem;
namespace Renderer {class BackendTestAccess {public:static auto& State(DiligentD3D11Backend&backend){return *backend.m_impl;}};}
using namespace Vegetation;
void Check(bool ok,const std::string&why){if(!ok)throw std::runtime_error(why);}
unsigned errors{};
void DILIGENT_CALL_TYPE Message(Diligent::DEBUG_MESSAGE_SEVERITY level,const char*text,const char*,const char*,int){if(level>=Diligent::DEBUG_MESSAGE_SEVERITY_ERROR){++errors;std::cerr<<text<<'\n';}}
bool Read(const fs::path&root,std::string_view path,std::vector<std::byte>&bytes){if(path.starts_with("d:/ymir work/"))path.remove_prefix(13);std::ifstream in(root/path,std::ios::binary|std::ios::ate);if(!in)return false;bytes.resize(std::size_t(in.tellg()));in.seekg(0);return bool(in.read(reinterpret_cast<char*>(bytes.data()),bytes.size()));}
Vec3 Sub(Vec3 a,Vec3 b){for(unsigned i=0;i<3;++i)a[i]-=b[i];return a;}
float Dot(Vec3 a,Vec3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vec3 Cross(Vec3 a,Vec3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
Vec3 Unit(Vec3 a){const auto n=std::sqrt(Dot(a,a));for(auto&v:a)v/=n;return a;}
Matrix View(Vec3 eye,Vec3 target){const auto z=Unit(Sub(eye,target)),x=Unit(Cross({0,0,1},z)),y=Cross(z,x);return {x[0],y[0],z[0],0,x[1],y[1],z[1],0,x[2],y[2],z[2],0,-Dot(eye,x),-Dot(eye,y),-Dot(eye,z),1};}
void Save(const fs::path&path,const std::vector<std::uint8_t>&pixels,unsigned w,unsigned h){std::ofstream out(path,std::ios::binary);out<<"P6\n"<<w<<' '<<h<<"\n255\n";out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());}
std::uint64_t Difference(const std::vector<std::uint8_t>&a,const std::vector<std::uint8_t>&b){Check(a.size()==b.size(),"image sizes");std::uint64_t total=0;for(unsigned i=0;i<a.size();++i)total+=std::abs(int(a[i])-int(b[i]));return total;}
int main(int argc,char**argv){try{
    Check(argc==3,"usage: VegetationModernGpu fixture-root output");const fs::path root=argv[1],output=argv[2];fs::create_directories(output);
    Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
    Platform::PlatformWindow window;Platform::WindowCreateInfo info;info.title="H2 vegetation proof";Check(window.Create(info),"window");window.SetSize(640,480);
    Renderer::DiligentD3D11Backend backend;Check(backend.Initialize({window.GetNativeHandle().value,640,480}),"D3D11");
    {
        Renderer::DiligentStaticObjectRenderer renderer(backend);Check(renderer.Initialize(),"renderer");Runtime runtime;
        std::map<std::string,Renderer::TerrainTexturePtr>textures;
        auto texture=[&](std::string_view path){auto&result=textures[std::string(path)];if(result)return result;std::vector<std::byte> bytes;Check(Read(root,path,bytes)&&bytes.size()>=128,"DDS texture");
            auto u32=[&](unsigned at){std::uint32_t value;std::memcpy(&value,bytes.data()+at,4);return value;};
            Renderer::TerrainTextureData data;data.width=u32(16);data.height=u32(12);data.format=Renderer::TerrainTextureFormat::RGBA8;
            Check(bytes.size()==128+std::size_t(data.width)*data.height*4,"uncompressed test DDS");data.mips.push_back({bytes.data()+128,std::size_t(data.width)*data.height*4,data.width*4});result=renderer.UploadTexture(data);Check(bool(result),"texture upload");return result;};
        std::ofstream metrics(output/"performance.csv");metrics<<"case,instances,visible,culled,draws,triangles,uploads,instanceBytes,cpuMs,gpuColorMs\n";
        auto settings=Graphics::PresetSettings(Graphics::GraphicsPreset::High,Graphics::GraphicsStyle::Modern);settings.ambientOcclusion=Graphics::AmbientOcclusionQuality::Off;settings.shadows=Graphics::ShadowQuality::Low;settings.bloom=false;
        unsigned revision=0;
        Graphics::SceneLighting light;light.sunDirection={.3f,.5f,-.6f};light.sunIntensity=3.14159265f;light.ambient={.28f,.28f,.28f};
        auto render=[&](const std::string&label,const RenderAsset&asset,std::span<Instance*const>instances,Vec3 eye,Vec3 target,float time,float scale=1.f,unsigned quality=2,bool capture=true,float transmission=1.f,unsigned transparent=0,std::span<const GrassPlacement*const> placements={},float lodSeconds=-1.f,bool fixedTreeDetail=false){
            auto config=Graphics::Resolve(settings,++revision);Renderer::ApplyGraphicsRuntimeConfig(config);
            Check(backend.BeginFrame(),"frame");backend.Clear({true,Renderer::ClearColor{.15f,.2f,.3f,1}});Renderer::modernFrame->Begin(light);
            // Static gallery cases capture settled endpoints. Temporal cases
            // supply a separate clock while keeping wind and camera fixed.
            if(lodSeconds<0)for(auto* instance:instances)instance->transition={};
            RenderContext c;c.modern=true;c.time=time;c.lodTime=lodSeconds<0?time:lodSeconds;c.quality=ResolveQuality(quality);c.distanceScale=scale;c.camera=eye;c.view=View(eye,target);
            c.fixedTreeDetail=fixedTreeDetail;
            c.quality.transmission*=transmission;
            c.state.blend=transparent!=0;
            c.projection={1.299038f,0,0,0,0,1.732051f,0,0,0,0,-50000.f/49990.f,-1,0,0,-500000.f/49990.f,0};c.state.sampling={true,true,true,true,true,false};
            Renderer::TerrainMatrices camera;camera.world=Identity;camera.view=c.view;camera.projection=c.projection;Renderer::modernFrame->SetCamera(camera);
            const auto before=statistics;const auto uploads=Renderer::vegetationInstanceUploads.load();
            auto&state=Renderer::BackendTestAccess::State(backend);Diligent::RefCntAutoPtr<Diligent::IQuery>query;Diligent::QueryDesc desc;desc.Name="H2 vegetation color GPU";desc.Type=Diligent::QUERY_TYPE_DURATION;state.device->CreateQuery(desc,&query);Check(bool(query),"duration query");
            state.context->BeginQuery(query);const auto start=std::chrono::steady_clock::now();
            if(!placements.empty())Check(DrawGrassBatch(placements,asset,renderer,c),"preloaded grass draw");
            else if(transparent==2){for(auto* instance:instances){const std::array<Instance*,1> singleton{instance};Check(DrawBatch(singleton,asset,renderer,c),"queued singleton vegetation draw");}}
            else Check(DrawBatch(instances,asset,renderer,c),"batched vegetation draw");
            const auto cpu=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();state.context->EndQuery(query);
            Renderer::modernFrame->End();Check(!renderer.Failed(),"modern shader/draw");
            std::vector<std::uint8_t> pixels;unsigned width{},height{};Check(backend.CaptureRGB(pixels,width,height),"capture");
            backend.EndFrame();backend.Present();renderer.ReleaseBindings();
            Diligent::QueryDataDuration duration;bool ready=false;
            for(unsigned attempt=0;attempt<200&&!ready;++attempt){ready=query->GetData(&duration,sizeof(duration),true);if(!ready)std::this_thread::sleep_for(std::chrono::milliseconds(1));}
            Check(ready&&duration.Frequency,"GPU duration ready after present");
            const double gpu=double(duration.Duration)*1000/duration.Frequency;
            if(capture)Save(output/(label+".ppm"),pixels,width,height);
            metrics<<label<<','<<instances.size()<<','<<statistics.visible-before.visible<<','<<statistics.culled-before.culled<<','<<statistics.batches-before.batches<<','<<statistics.triangles-before.triangles<<','<<Renderer::vegetationInstanceUploads-uploads<<','<<Renderer::vegetationInstanceBytes<<','<<cpu<<','<<gpu<<'\n';metrics.flush();Check(errors==0,"Diligent ERROR/FATAL zero");return pixels;
        };
        for(const auto*name:{"beech","grass","bush"}) {
            const auto loaded=runtime.LoadCompiled(std::string("vegetation/modern/")+name+".zveg",[&](std::string_view path,std::vector<std::byte>&bytes){return Read(root,path,bytes);});Check(bool(loaded),loaded.error);
            std::string error;auto asset=Prepare(loaded.asset,renderer,texture,error);Check(bool(asset),error);
            const float height=loaded.asset->metadata.bounds.max[2];Instance single(loaded.asset,Identity);std::array<Instance*,1>one{&single};
            const Vec3 eye{height*.4f,-height*1.7f,height*.6f},target{0,0,height*.5f};
            const auto still=render(std::string(name)+"-near",*asset,one,eye,target,0);
            const auto uploads=Renderer::vegetationInstanceUploads.load();const auto repeat=render(std::string(name)+"-repeat",*asset,one,eye,target,0);
            Check(Difference(still,repeat)==0,"repeat image deterministic");Check(Renderer::vegetationInstanceUploads==uploads,"unchanged static instances not uploaded again");
            const auto retained=asset->instanceBuffers;
            render(std::string(name)+"-temporarily-hidden",*asset,one,eye,{eye[0]*2,eye[1]*2,eye[2]},0,1,2,false);
            for(unsigned i=0;i<retained.size();++i)if(retained[i])Check(retained[i]==asset->instanceBuffers[i],"offscreen culling retains instance allocation");
            const auto restored=render(std::string(name)+"-visible-again",*asset,one,eye,target,0,1,2,false);
            Check(Difference(still,restored)==0,"returning after culling preserves solid tree");
            if(std::string(name)=="beech") {
                const auto blocked=render("tree-camera-blocker-solid",*asset,one,eye,target,0,1,2,true,1,1);
                Check(Difference(still,blocked)==0,"tree camera classification cannot enable transparency");
                single.transition={};single.stableLod=-1;
                const auto initial=render("tree-handover-near",*asset,one,eye,target,0,1,2,true,1,0,{},10);
                const auto start=render("tree-handover-start",*asset,one,eye,target,0,.5f,2,true,1,0,{},11);
                Check(Difference(initial,start)==0,"LOD threshold crossing cannot change pixels immediately");
                const auto middle=render("tree-handover-midpoint",*asset,one,eye,target,0,.5f,2,true,1,0,{},11.15f);
                Check(Difference(initial,middle)>100,"LOD handover progresses on GPU");
                const auto reverse=render("tree-handover-reverse",*asset,one,eye,target,0,1,2,true,1,0,{},11.15f);
                Check(Difference(middle,reverse)==0,"reversal preserves exact GPU coverage mask");
                const auto returned=render("tree-handover-returned",*asset,one,eye,target,0,1,2,true,1,0,{},11.4f);
                Check(Difference(initial,returned)==0,"return completes without holes or residual dither");
                // Isolate a foliage-only LOD change: trunk must stay solid and
                // submit once while both leaf representations share coverage.
                auto shared=std::make_shared<Asset>();shared->metadata=loaded.asset->metadata;shared->geometry=loaded.asset->geometry;
                shared->metadata.lods[2].meshes[0]=shared->metadata.lods[3].meshes[0];
                shared->stableLods=BuildStableLods(shared->metadata);
                auto sharedRender=Prepare(shared,renderer,texture,error);Check(bool(sharedRender),error);
                Instance sharedTree(shared,Identity);const std::array<Instance*,1> sharedOne{&sharedTree};
                render("tree-shared-trunk-near",*sharedRender,sharedOne,eye,target,0,1,2,false,1,0,{},20);
                render("tree-shared-trunk-start",*sharedRender,sharedOne,eye,target,0,.5f,2,false,1,0,{},21);
                const auto beforeBatches=statistics.batches;
                render("tree-shared-trunk-midpoint",*sharedRender,sharedOne,eye,target,0,.5f,2,true,1,0,{},21.15f);
                Check(statistics.batches-beforeBatches==3,"shared trunk draws once alongside two leaf LODs");
                const auto fixed=render("tree-fixed-high-near",*asset,one,eye,target,0,1,2,true,1,0,{},30,true);
                for(float scale:{.5f,.25f,.12f,.5f,1.f}) {
                    const auto before=statistics;
                    const auto image=render("tree-fixed-high-"+std::to_string(scale),*asset,one,eye,target,0,scale,2,true,1,0,{},31,true);
                    Check(Difference(fixed,image)==0,"fixed high remains pixel identical across former LOD ranges");
                    Check(statistics.batches-before.batches==2&&statistics.lodChanges==before.lodChanges&&!single.transition.active,"fixed high draws one solid trunk and crown without handover");
                    Check(single.lod.meshes==loaded.asset->stableLods.front().state.meshes,"fixed high selects highest authored 3D geometry");
                }
                const auto culled=statistics.culled;
                render("tree-fixed-high-culled",*asset,one,eye,target,0,.001f,2,false,1,0,{},32,true);
                Check(statistics.culled==culled+1,"fixed high still honors distance culling");
            }
            const auto wind=render(std::string(name)+"-wind",*asset,one,eye,target,1.3f);Check(Difference(still,wind)>100,"visible GPU wind");
            if(std::string(name)=="grass") {
                const auto placed=PlaceGrass(GrassCandidate{},0,1);const std::array<const GrassPlacement*,1> compact{&placed};
                const auto preloaded=render("grass-preloaded",*asset,one,eye,target,0,1,2,true,1,0,compact);
                Check(Difference(still,preloaded)==0,"preloaded compact grass preserves exact native instance pixels");
                const auto beforeFar=statistics.visible;
                render("grass-preloaded-far",*asset,one,{0,-7500,500},{0,0,34},0,1,2,true,1,0,compact);
                Check(statistics.visible==beforeFar+1,"preloaded far grass survives former 26m cutoff");
                auto transform=Identity;transform[12]=height*.5f;Instance other(loaded.asset,transform);std::array<Instance*,2> pair{&single,&other};
                const auto batched=render("grass-transparent-batch",*asset,pair,eye,target,0,1,2,true,1,1);
                const auto singletons=render("grass-transparent-singletons",*asset,pair,eye,target,0,1,2,true,1,2);
                Check(Difference(batched,singletons)==0,"queued camera blockers retain independent instance data");
            }
            if(std::string(name)=="beech") {
                for(unsigned lod=0;lod<3;++lod)render("tree-lod-"+std::to_string(lod+1),*asset,one,{0,-loaded.asset->metadata.lodDistances[lod]*1.18f,height*.6f},target,0);
                for(float fraction:{-.07f,-.03f,0.f,.03f,.07f})render("tree-transition-"+std::to_string(fraction),*asset,one,{0,-loaded.asset->metadata.lodDistances[1]*(1+fraction),height*.6f},target,0);
                const auto sun=light;light.sunDirection={0,-1,-.15f};
                const auto transmitted=render("tree-backlight",*asset,one,eye,target,0);
                const auto opaque=render("tree-transmission-off",*asset,one,eye,target,0,1,2,true,0);
                Check(Difference(transmitted,opaque)>100,"shared-sun leaf transmission changes pixels");
                light.sunDirection={0,0,-1};render("tree-noon",*asset,one,eye,target,0);light=sun;
            }
            const unsigned count=std::string(name)=="grass"?1000:600;std::vector<std::unique_ptr<Instance>>owned;std::vector<Instance*>many;
            for(unsigned i=0;i<count;++i){auto m=Identity;if(i%2)m[0]=-1;m[12]=(int(i%25)-12)*height*.24f;m[13]=(int(i/25)-12)*height*.24f;owned.push_back(std::make_unique<Instance>(loaded.asset,m,i));many.push_back(owned.back().get());}
            const auto draws=statistics.batches,geometry=statistics.uploads;
            render(std::string(name)+"-dense",*asset,many,{0,-height*7,height*4},{0,0,height*.3f},0,10);
            Check(statistics.batches-draws<=8&&statistics.uploads==geometry,"hundreds of instances use shared geometry and bounded draws");
            for(unsigned q=0;q<4;++q)render(std::string(name)+"-quality-"+std::to_string(q),*asset,many,{0,-height*7,height*4},{0,0,height*.3f},0,10,q);
            const auto previousCulled=statistics.culled;render(std::string(name)+"-behind-camera",*asset,many,{0,-height*7,height*4},{0,-height*20,height*4},0,10);Check(statistics.culled>previousCulled,"frustum culling rejects scene behind camera");
        }
        runtime.Clear();textures.clear();renderer.ReleaseBindings();Check(liveAssets==0&&liveInstances==0&&liveGeometry==0&&liveRenderAssets==0&&Renderer::liveVegetationInstanceBuffers==0&&Renderer::vegetationInstanceBytes==0,"all vegetation resources released");
        Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0,"shared GPU geometry/textures released");
    }
    backend.Shutdown();Check(errors==0,"Diligent diagnostics");std::cout<<"PASS H2 real instancing, no redundant uploads, culling, LOD, wind, materials, GPU timings and zero lifetime resources\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
