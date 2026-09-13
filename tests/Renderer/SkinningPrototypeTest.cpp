#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterLib/ResourceManager.h"
#include "EterLib/TerrainTextureLoader.h"
#include "EterLib/Camera.h"
#include "EterImageLib/ScreenshotJPEG.h"
#include "PackLib/PackManager.h"
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"

using namespace Renderer;
float CCamera::CAMERA_MAX_DISTANCE=2500.f;
static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
#include "SkinningGpuReadback.h"
static std::atomic_uint validationWarnings{};
static void DILIGENT_CALL_TYPE ValidateMessage(Diligent::DEBUG_MESSAGE_SEVERITY severity,const char* message,const char*,const char*,int)
{
    if(severity>=Diligent::DEBUG_MESSAGE_SEVERITY_WARNING) { ++validationWarnings;std::cerr<<"Diligent validation: "<<message<<'\n'; }
}
static double gpuDrawUs[2]{};
static size_t timedDraws[2]{};
static size_t unavailableTimings{};
static uint64_t actualCpuVertexBytes{}, actualGpuVertexBytes{};
static std::vector<std::pair<Diligent::RefCntAutoPtr<Diligent::IQuery>,bool>> pendingTimings;
static void CollectTimings(DiligentD3D11Backend& backend)
{
    BackendTestAccess::Idle(backend);
    for(auto& [query,gpu]:pendingTimings) {
        Diligent::QueryDataDuration duration;
        if(query->GetData(&duration,sizeof(duration)) && duration.Frequency) {
            gpuDrawUs[gpu]+=double(duration.Duration)*1e6/double(duration.Frequency);++timedDraws[gpu];
        } else ++unavailableTimings;
    }
    pendingTimings.clear();
}
struct Asset
{
    granny_file* file{}; CGrannyModel* model{};
    explicit Asset(const std::string& path) {
        file=GrannyReadEntireFile(path.c_str()); Check(file!=nullptr,"Real GR2 fixture required");
        auto* info=GrannyGetFileInfo(file); Check(info && info->ModelCount,"Model fixture");
        model=new CGrannyModel; Check(model->CreateFromGrannyModelPointer(info->Models[0]),"Native model");
        model->CaptureActorSource(); Check(bool(model->GetActorSource()),"Native actor source capture");
    }
    ~Asset() { if(model) model->Release(); if(file) GrannyFreeFile(file); }
};
struct Clip
{
    granny_file* file{}; CGrannyMotion motion;
    explicit Clip(const std::string& path) {
        file=GrannyReadEntireFile(path.c_str()); Check(file!=nullptr,"Real animation required");
        auto* info=GrannyGetFileInfo(file); Check(info && info->AnimationCount,"Animation fixture");
        Check(motion.BindGrannyAnimation(info->Animations[0]),"Animation binding");
    }
    ~Clip() { motion.Destroy(); if(file) GrannyFreeFile(file); }
};
static std::vector<TerrainTexturePtr> Textures(Asset& asset,const std::string& root,DiligentActorRenderer& renderer)
{
    std::vector<TerrainTexturePtr> result;
    auto* native=asset.model->GetGrannyModelPointer();
    for(int m=0;m<native->MeshBindingCount;++m) {
        auto* mesh=native->MeshBindings[m].Mesh;
        for(int i=0;i<mesh->MaterialBindingCount;++i) {
            auto* texture=GrannyGetMaterialTextureByType(mesh->MaterialBindings[i].Material,GrannyDiffuseColorTexture);
            Check(texture && texture->FromFileName,"Original material texture");
            std::string path=texture->FromFileName; std::replace(path.begin(),path.end(),'\\','/');
            std::transform(path.begin(),path.end(),path.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
            const auto offset=path.find("ymir work/");
            path=offset!=std::string::npos ? root+"/PC/"+path.substr(offset) :
                root+"/PC/ymir work/pc/warrior/"+std::filesystem::path(path).filename().string();
            std::ifstream input(path,std::ios::binary); Check(bool(input),"Original DDS fixture");
            std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});
            result.push_back(LoadTerrainTextureMemory(bytes.data(),bytes.size(),renderer));
            Check(bool(result.back()),"Original DDS upload");
            std::cout<<"TEXTURE mesh="<<m<<" material="<<i<<" path="<<path<<'\n';
        }
    }
    return result;
}
static StaticObjectDraw DrawState(unsigned width,unsigned height,const Math::Matrix& world)
{
    StaticObjectDraw draw;
    Math::Matrix view,projection,normal;
    const Math::Vector3 eye(260,-480,210),target(0,0,95),up(0,0,1);
    Math::MatrixLookAtRH(&view,&eye,&target,&up);
    Math::MatrixOrthoRH(&projection,300.f*width/height,300.f,1.f,1500.f);
    memcpy(draw.matrices.world.data(),&world,64); memcpy(draw.matrices.view.data(),&view,64);
    memcpy(draw.matrices.projection.data(),&projection,64);
    normal=world*view; Check(Math::MatrixInverse(&normal,nullptr,&normal)!=nullptr,"Normal inverse");
    Math::MatrixTranspose(&normal,&normal); memcpy(draw.normalTransform.data(),&normal,64);
    draw.ambient={.3f,.3f,.3f,1}; draw.diffuse={.7f,.7f,.7f,1}; draw.lightDirection={.3f,.4f,.8660254f,0};
    draw.cull=StaticObjectCull::None; draw.normalizeNormals=true; return draw;
}
static std::vector<uint8_t> RenderPose(DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,
    Asset& asset,CGrannyModelInstance& actor,const std::vector<TerrainTexturePtr>& textures,
    StaticObjectDraw draw,bool gpu,const std::filesystem::path& screenshot)
{
    backend.Clear({true,ClearColor{0.08f,0.1f,0.13f,1}});
    auto& data=actor.GetActorRenderData(); const auto& source=*asset.model->GetActorSource();
    const auto uploadedBefore=renderer.BytesUploaded();
    if(!gpu) {
        if(!data.geometry) data.geometry=renderer.CreateGeometry(source);
        Check(renderer.UpdateVertices(data.geometry,data.vertices,source.deformVertexCount),"CPU dynamic PNT upload");
    }
    Check(bool(data.geometry),"Current render geometry");
    const auto timing=BackendTestAccess::BeginTiming(backend);
    uint32_t first=0,materialOffset=0;
    for(size_t m=0;m<asset.model->GetSkinningData()->meshes.size();++m) {
        const auto& mesh=*asset.model->GetSkinningData()->meshes[m];
        for(const auto& group:mesh.groups) {
            draw.baseVertex=mesh.deformVertexOffset; draw.vertexCount=static_cast<uint32_t>(mesh.vertices.size());
            draw.firstIndex=first+group.firstIndex; draw.indexCount=group.indexCount;
            renderer.Draw(&actor,data.geometry,textures.at(materialOffset+group.materialIndex),draw);
        }
        first+=static_cast<uint32_t>(mesh.indices.size());
        materialOffset+=asset.model->GetGrannyModelPointer()->MeshBindings[m].Mesh->MaterialBindingCount;
    }
    Check(!renderer.Failed(),"Material/skin draw succeeded");
    const auto uploaded=renderer.BytesUploaded()-uploadedBefore;
    Check(uploaded==(gpu ? 0 : uint64_t(source.deformVertexCount)*sizeof(StaticObjectVertex)),"Actual CPU PNT upload / GPU no vertex upload");
    if(gpu) actualGpuVertexBytes+=uploaded; else actualCpuVertexBytes+=uploaded;
    BackendTestAccess::EndTiming(backend,timing);
    std::vector<uint8_t> rgb; uint32_t width{},height{};
    Check(backend.CaptureRGB(rgb,width,height),"Composed GPU readback");
    if(timing) pendingTimings.emplace_back(timing,gpu);
    if(!screenshot.empty()) Check(SaveScreenshotJPEG(screenshot.c_str(),rgb,width,height),"Comparison screenshot");
    return rgb;
}
int main(int argc,char** argv)
{
    HWND window=CreateWindowW(L"STATIC",L"B3 GPU skinning parity",WS_OVERLAPPEDWINDOW,0,0,640,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    try {
        Check(argc==3,"Asset root and evidence directory required"); std::filesystem::path output=argv[2];
        std::filesystem::create_directories(output);
        CPackManager packs; CResourceManager resources;
        Diligent::GetEngineFactoryD3D11()->SetMessageCallback(ValidateMessage);
        DiligentD3D11Backend backend; Check(window && backend.Initialize({window,640,640}),"Diligent context");
        startupSkinningMode=PrototypeSkinningMode::GPUPrototype;
        float maxPosition=0,maxNormal=0; size_t samples=0;
        {
            DiligentActorRenderer renderer(backend); Check(renderer.Initialize(),"CPU and separate GPU VS pipelines");
            actorRenderer=&renderer; actorWorldFrame=true;
            const std::string base=std::string(argv[1])+"/PC/ymir work/pc/warrior/";
            Asset asset(base+"warrior_novice.gr2"),armor(base+"warrior_4-1.gr2");
            Check(IsReferenceSkinningModel(*asset.model->GetSkinningData()) && !IsReferenceSkinningModel(*armor.model->GetSkinningData()),"Only reference mesh contract accepted");
            auto textures=Textures(asset,argv[1],renderer);
            for(const auto& mesh:asset.model->GetSkinningData()->meshes)
                std::cout<<"MESH "<<mesh->name<<" vertices="<<mesh->vertices.size()<<" indices="<<mesh->indices.size()<<" palette="<<mesh->meshBoneCount<<" base="<<mesh->deformVertexOffset<<'\n';
            Math::Matrix world,scale; Math::MatrixRotationZ(&world,.37f); Math::MatrixScaling(&scale,1.3f,.7f,1.1f);
            world=scale*world; world._41=17;world._42=-9;world._43=4;
            for(const std::string clipName:{"wait","walk","run","attack"}) {
                Clip clip(base+"general/"+clipName+".gr2");
                CGrannyModelInstance cpu,gpu;
                cpu.SetMainModelPointer(asset.model,nullptr); gpu.SetMainModelPointer(asset.model,nullptr);
                cpu.SetMotionPointer(&clip.motion); gpu.SetMotionPointer(&clip.motion);
                for(float time:{0.f,.17f,.37f,.63f}) {
                    const unsigned extent=time>.5f?800:640;
                    Check(backend.Resize(0,0) && !backend.BeginFrame() && backend.Resize(extent,extent),"Suspend/restore/resize");
                    Check(backend.BeginFrame(),"Frame"); ++actorFrameSerial; renderer.ResetFrame();
                    cpu.SetLocalTime(time);gpu.SetLocalTime(time);cpu.Update(120);gpu.Update(120);
                    ActorInstanceSet scope;scope.instances[0]=&cpu;
                    { ActorDeformScope deform(scope); cpu.Deform(&world); }
                    scope.instances[0]=&gpu;scope.prototypeBody=&gpu;
                    { ActorDeformScope deform(scope); gpu.Deform(&world); }
                    Check(cpu.GetActorRenderData().ready && !cpu.GetActorRenderData().gpuPrototype,"CPU remains active");
                    Check(gpu.GetActorRenderData().gpuPrototype && gpu.GetActorRenderData().vertices.empty(),"GPU skips CPU deform/copy");
                    auto cp=cpu.GetSkinningPalette(),gp=gpu.GetSkinningPalette();
                    Check(cp && gp && cp->matrices==gp->matrices,"Identical native animation/clock/composite matrices");
                    std::vector<SkinningVertex> vertices;std::vector<uint16_t> indices;
                    Check(BuildPrototypeVertices(*asset.model->GetSkinningData(),gpu.GetSkinningRemaps(),*gp,vertices,indices),"Original static remap");
                    Check(indices==asset.model->GetActorSource()->indices,"Reused exact native actor index order");
                    auto numerical=BackendTestAccess::Skin(backend,vertices,*gp);
                    renderer.ReleaseBindings();
                    const auto& expected=cpu.GetActorRenderData().vertices;
                    for(size_t v=0;v<vertices.size();++v) for(size_t c=0;c<3;++c) {
                        float p=std::abs(numerical[2*v][c]-expected[v][c]),n=std::abs(numerical[2*v+1][c]-expected[v][c+3]);
                        maxPosition=std::max(maxPosition,p);maxNormal=std::max(maxNormal,n);
                        Check(std::isfinite(p) && p<=1e-4f+2e-6f*std::max(1.f,std::abs(expected[v][c])),"GPU vs native CPU position tolerance");
                        Check(std::isfinite(n) && n<=1e-5f,"GPU vs native CPU normal tolerance");
                    }
                    samples+=vertices.size();
                    const auto draw=DrawState(extent,extent,world);
                    const auto shot=time==.37f;
                    auto a=RenderPose(backend,renderer,asset,cpu,textures,draw,false,shot?output/(clipName+"-cpu.jpg"):std::filesystem::path{});
                    auto b=RenderPose(backend,renderer,asset,gpu,textures,draw,true,shot?output/(clipName+"-gpu.jpg"):std::filesystem::path{});
                    if(shot) {
                        CGrannyModelInstance other;other.SetMainModelPointer(asset.model,nullptr);other.SetMotionPointer(&clip.motion);
                        other.SetLocalTime(.71f);other.Update(120);ActorInstanceSet target;target.instances[0]=target.prototypeBody=&other;
                        { ActorDeformScope scope(target);other.Deform(&world); }
                        Check(other.GetActorRenderData().gpuPrototype,"Independent GPU actor B");
                        RenderPose(backend,renderer,asset,other,textures,draw,true,{});
                        const auto repeated=RenderPose(backend,renderer,asset,gpu,textures,draw,true,{});
                        Check(b==repeated,"A B A uses each actor's own palette, never stale texture-cache SRB");
                        other.Clear();
                    }
                    size_t changed=0,large=0,visible=0; unsigned maxDelta=0;
                    for(size_t i=0;i<a.size();++i) {
                        auto delta=unsigned(std::abs(int(a[i])-int(b[i]))); changed+=delta!=0;large+=delta>2;maxDelta=std::max(maxDelta,delta);
                        visible+=a[i]>60;
                    }
                    Check(visible>5000,"Nonempty textured warrior image");
                    Check(large<=a.size()/10000 && changed<=a.size()/100,"CPU/GPU raster parity tolerance");
                    std::cout<<"POSE "<<clipName<<" time="<<time<<" pixels="<<extent*extent<<" changedChannels="<<changed<<" largeChannels="<<large<<" maxDelta="<<maxDelta<<'\n';
                    backend.EndFrame(); backend.Present(); CollectTimings(backend);
                }
                renderer.ReleaseBindings(); cpu.Clear();gpu.Clear();
                Check(!livePrototypeGeometry && !livePrototypePalettes,"Despawn GPU buffers zero");
            }
            // Invalid palettes and unsupported models must take the original CPU path, not a stale GPU draw.
            Check(backend.BeginFrame(),"Fallback frame");
            CGrannyModelInstance actor;actor.SetMainModelPointer(asset.model,nullptr);
            ActorInstanceSet target;target.instances[0]=&actor;target.prototypeBody=&actor;
            { ActorDeformScope scope(target);actor.Deform(&world); }
            Check(actor.GetActorRenderData().gpuPrototype,"Reference before shape change");
            const auto oldGeometry=std::weak_ptr<StaticObjectGeometry>(actor.GetActorRenderData().geometry);
            actor.SetMainModelPointer(armor.model,nullptr);
            { ActorDeformScope scope(target);actor.Deform(&world); }
            Check(actor.GetActorRenderData().ready && !actor.GetActorRenderData().gpuPrototype && !actor.GetActorRenderData().vertices.empty(),"Unsupported armor CPU fallback");
            Check(oldGeometry.expired(),"Shape change releases old GPU reference geometry");
            actor.SetMainModelPointer(asset.model,nullptr);
            { ActorDeformScope scope(target);actor.Deform(&world); }
            Check(actor.GetActorRenderData().gpuPrototype,"Return to reference after CPU armor");
            Asset hairAsset(base+"hair/hair_1_1.gr2");CGrannyModelInstance hair;
            CGrannyModelInstance* owner=&actor;hair.SetLinkedModelPointer(hairAsset.model,nullptr,&owner);
            target.instances[4]=&hair;
            { ActorDeformScope scope(target);hair.Deform(&world); }
            Check(hair.GetActorRenderData().ready && !hair.GetActorRenderData().gpuPrototype &&
                hair.GetSkinningPalette()==actor.GetSkinningPalette(),"Skinned hair stays CPU with GPU body's current native palette");
            hair.Clear();
            BonePalette bad; StaticObjectGeometryPtr rejected;
            Check(!renderer.PreparePrototype(rejected,*asset.model->GetSkinningData(),{},bad),"Missing palette rejected");
            bad.skeleton=asset.model->GetSkinningData()->skeleton;bad.matrices.resize(75);bad.ready=true;
            bad.matrices[0][0]=std::numeric_limits<float>::quiet_NaN();
            Check(!ValidPrototypePalette(bad),"Nonfinite matrix rejected");
            auto skeleton=std::make_shared<SkeletonLayout>(); skeleton->names.resize(163);bad.skeleton=skeleton;bad.matrices.assign(163,{});
            Check(ValidPrototypePalette(bad),"163 matrix boundary supported");
            bad.matrices[162][0]=bad.matrices[162][5]=bad.matrices[162][10]=bad.matrices[162][15]=1;
            bad.matrices[162][12]=7;
            SkinningVertex highest{};highest.position[0]=3;highest.normal[2]=1;highest.weights[0]=255;highest.indices[0]=162;
            const auto boundary=BackendTestAccess::Skin(backend,{highest},bad);
            Check(boundary[0][0]==10 && boundary[1][2]==1,"Actual GPU reads bone 162 in full 163 palette");
            renderer.ReleaseBindings();
            skeleton->names.resize(164);bad.matrices.resize(164);Check(!ValidPrototypePalette(bad),"164 matrix overflow fallback");
            actor.Clear();backend.EndFrame();backend.Present();renderer.ReleaseBindings();textures.clear();
            Check(!renderer.LiveGeometryCount() && !renderer.LiveTextureCount() && !livePrototypeGeometry && !livePrototypePalettes,"All actor GPU resources zero");
            actorRenderer=nullptr;actorWorldFrame=false;actorDeformTargets={};
        }
        resources.Destroy();BackendTestAccess::Validate(backend);backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(validationWarnings==0,"No Diligent validation warnings/errors");
        Check(!liveSkinMeshes && !liveBoneRemaps && !liveBonePalettes && !skinSidecarFailures,"B2 ownership remains clean");
        std::cout<<"PASS samples="<<samples<<" maxPosition="<<maxPosition<<" maxNormal="<<maxNormal
            <<" PrototypeGeometry="<<livePrototypeGeometry<<" PrototypePalettes="<<livePrototypePalettes
            <<" frames="<<prototypeFrames<<" boneWrittenBytes="<<prototypeBoneBytes<<" prepareUs="<<prototypePrepareUs<<'\n';
        std::cout<<"GPU actor-draw timings CPU-path us="<<gpuDrawUs[0]<<" samples="<<timedDraws[0]
            <<" GPU-path us="<<gpuDrawUs[1]<<" samples="<<timedDraws[1]<<" unavailable="<<unavailableTimings<<" (diagnostic readback fixture, not gameplay FPS)\n";
        std::cout<<"Actual dynamic vertex upload bytes CPU="<<actualCpuVertexBytes<<" GPU="<<actualGpuVertexBytes<<'\n';
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';if(window) DestroyWindow(window);return 1; }
}
