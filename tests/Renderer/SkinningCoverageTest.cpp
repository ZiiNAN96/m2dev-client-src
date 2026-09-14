// ZiiNAN: GPU skinning actor coverage
#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterLib/ResourceManager.h"
#include "EterLib/Camera.h"
#include "PackLib/PackManager.h"
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace Renderer;
float CCamera::CAMERA_MAX_DISTANCE=2500.f;
static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
#include "SkinningGpuReadback.h"
static unsigned warnings=0;
static void DILIGENT_CALL_TYPE Message(Diligent::DEBUG_MESSAGE_SEVERITY severity,const char* message,const char*,const char*,int)
{ if(severity>=Diligent::DEBUG_MESSAGE_SEVERITY_WARNING) { ++warnings;std::cerr<<message<<'\n'; } }
struct Asset
{
    granny_file* file{}; CGrannyModel* model{}; std::string path;
    explicit Asset(const std::string& name):path(name) {
        file=GrannyReadEntireFile(path.c_str());Check(file!=nullptr,"Required real GR2 asset");
        auto* info=GrannyGetFileInfo(file);Check(info && info->ModelCount,"Required real model");
        model=new CGrannyModel;Check(model->CreateFromGrannyModelPointer(info->Models[0]),"Native model creation");
        model->CaptureActorSource();Check(bool(model->GetActorSource()),"Native actor source");
    }
    ~Asset() { if(model) model->Release();if(file) GrannyFreeFile(file); }
    Asset(const Asset&)=delete;
};
struct Clip
{
    granny_file* file{};CGrannyMotion motion;
    explicit Clip(const std::string& path) {
        file=GrannyReadEntireFile(path.c_str());Check(file!=nullptr,"Required real animation");
        auto* info=GrannyGetFileInfo(file);Check(info && info->AnimationCount,"Animation data");
        Check(motion.BindGrannyAnimation(info->Animations[0]),"Native motion binding");
    }
    ~Clip() { motion.Destroy();if(file) GrannyFreeFile(file); }
};
struct Pair
{
    CGrannyModelInstance cpu,gpu;
    CGrannyModelInstance* cpuOwner=&cpu;CGrannyModelInstance* gpuOwner=&gpu;
    void Set(Asset& asset,Pair* owner=nullptr) {
        cpu.Clear();gpu.Clear();
        if(owner) { cpu.SetLinkedModelPointer(asset.model,nullptr,&owner->cpuOwner,true);gpu.SetLinkedModelPointer(asset.model,nullptr,&owner->gpuOwner,true); }
        else { cpu.SetMainModelPointer(asset.model,nullptr);gpu.SetMainModelPointer(asset.model,nullptr); }
    }
    void Motion(Clip& clip,float time) {
        const auto start=cpu.GetLocalTime();Check(start==gpu.GetLocalTime(),"Identical native clock before motion change");
        cpu.SetMotionPointer(&clip.motion,0);gpu.SetMotionPointer(&clip.motion,0);
        cpu.SetLocalTime(start+time);gpu.SetLocalTime(start+time);cpu.Update(120);gpu.Update(120);
    }
    void Deform(const Math::Matrix& world,ActorPart part=ActorPart::Body,ActorCategory category=ActorCategory::Player) {
        ActorInstanceSet targets;targets.category=category;targets.instances[uint32_t(part)]=&cpu;
        { ActorDeformScope scope(targets);cpu.Deform(&world); }
        targets.instances[uint32_t(part)]=&gpu;targets.gpuSkinning=true;
        { ActorDeformScope scope(targets);gpu.Deform(&world); }
        Check(cpu.GetActorRenderData().ready && gpu.GetActorRenderData().ready,"Both current poses ready");
        Check(!cpu.GetActorRenderData().gpuPrototype,"Explicit CPU control remains CPU");
        Check(gpu.GetActorRenderData().gpuPrototype==bool(gpu.GetModel()->GetActorSource()->deformVertexCount),"Every real deformable part uses GPU; rigid parts remain rigid");
        Check(gpu.GetActorRenderData().vertices.empty(),"GPU/rigid path never copies full deformed vertices");
    }
};
static size_t samples=0,cases=0,maxBones=0;
static float maxPosition=0,maxNormal=0,maxNativePoseDelta=0;
static void Numeric(Pair& pair,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer)
{
    if(!pair.gpu.GetActorRenderData().gpuPrototype) return;
    auto cpu=pair.cpu.GetSkinningPalette(),gpu=pair.gpu.GetSkinningPalette();
    Check(cpu && gpu && cpu->matrices.size()==gpu->matrices.size(),"Matching native CPU/GPU pose sizes");
    // ZiiNAN: Independently sampled Granny poses may differ by tiny rounding; vertex/raster limits stay unchanged.
    for(size_t b=0;b<cpu->matrices.size();++b) for(size_t c=0;c<16;++c) {
        const auto delta=std::abs(cpu->matrices[b][c]-gpu->matrices[b][c]);
        if(!std::isfinite(delta) || delta>1e-5f)
            std::cerr<<"POSE difference bone="<<b<<" component="<<c<<" cpu="<<cpu->matrices[b][c]<<" gpu="<<gpu->matrices[b][c]<<'\n';
        Check(std::isfinite(delta) && delta<=1e-5f,"Independent native poses within 1e-5 rounding tolerance");
        maxNativePoseDelta=std::max(maxNativePoseDelta,delta);
    }
    maxBones=std::max(maxBones,gpu->matrices.size());
    std::vector<SkinningVertex> vertices;std::vector<uint16_t> indices;
    Check(BuildPrototypeVertices(*pair.gpu.GetModel()->GetSkinningData(),pair.gpu.GetSkinningRemaps(),*gpu,vertices,indices),"Original vertices and native destination remap");
    auto actual=BackendTestAccess::Skin(backend,vertices,*gpu);renderer.ReleaseBindings();
    const auto& expected=pair.cpu.GetActorRenderData().vertices;
    for(size_t v=0;v<vertices.size();++v) for(size_t c=0;c<3;++c) {
        const auto p=std::abs(actual[2*v][c]-expected.at(v)[c]);
        const auto n=std::abs(actual[2*v+1][c]-expected.at(v)[c+3]);
        Check(std::isfinite(p) && p<=1e-4f+2e-6f*std::max(1.f,std::abs(expected[v][c])),"Position B2/B3 tolerance");
        Check(std::isfinite(n) && n<=1e-5f,"Normal B2/B3 tolerance");
        maxPosition=std::max(maxPosition,p);maxNormal=std::max(maxNormal,n);
    }
    samples+=vertices.size();
}
static Math::Matrix World()
{
    Math::Matrix world,scale;Math::MatrixRotationZ(&world,.37f);Math::MatrixScaling(&scale,1.3f,.7f,1.1f);
    world=scale*world;world._41=17;world._42=-9;world._43=4;return world;
}
static StaticObjectDraw Camera(Pair& pair)
{
    Math::Vector3 lo(1e10f,1e10f,1e10f),hi(-1e10f,-1e10f,-1e10f);
    auto* model=pair.cpu.GetModel();const auto& source=*model->GetActorSource();
    const auto& vertices=source.IsRigid()?source.rigidVertices:pair.cpu.GetActorRenderData().vertices;
    for(int m=0;m<model->GetMeshCount();++m) {
        const auto* mesh=model->GetMeshPointer(m);
        const auto base=mesh->GetVertexBasePosition()+(mesh->CanDeformPNTVertices()?0:source.deformVertexCount);
        for(int v=0;v<mesh->GetVertexCount();++v) {
            const auto& p=vertices.at(base+v);Math::Vector3 point(p[0],p[1],p[2]),out;
            Math::Vec3TransformCoord(&out,&point,pair.cpu.GetStaticObjectWorldMatrix(m));
            lo.x=std::min(lo.x,out.x);lo.y=std::min(lo.y,out.y);lo.z=std::min(lo.z,out.z);
            hi.x=std::max(hi.x,out.x);hi.y=std::max(hi.y,out.y);hi.z=std::max(hi.z,out.z);
        }
    }
    const auto center=(lo+hi)*.5f;const float radius=std::max({hi.x-lo.x,hi.y-lo.y,hi.z-lo.z,1.f});
    const auto eye=center+Math::Vector3(.4f,-1.f,.3f)*radius*3.f;const Math::Vector3 up(0,0,1);
    Math::Matrix view,projection;Math::MatrixLookAtRH(&view,&eye,&center,&up);
    Math::MatrixOrthoRH(&projection,radius*1.7f,radius*1.7f,1.f,radius*8.f);
    StaticObjectDraw draw;memcpy(draw.matrices.view.data(),&view,64);memcpy(draw.matrices.projection.data(),&projection,64);
    draw.ambient={.6f,.6f,.6f,1};draw.diffuse={.4f,.4f,.4f,1};draw.lightDirection={.3f,.4f,.8660254f,0};
    draw.normalizeNormals=true;draw.cull=StaticObjectCull::None;return draw;
}
static uint32_t Draw(CGrannyModelInstance& actor,DiligentActorRenderer& renderer,const TerrainTexturePtr& texture,
    StaticObjectDraw draw,ActorPart part,ActorCategory category)
{
    auto* model=actor.GetModel();auto& data=actor.GetActorRenderData();const auto& source=*model->GetActorSource();
    if(!data.geometry) data.geometry=renderer.CreateGeometry(source,part,category);
    if(!data.gpuPrototype && !source.IsRigid() && data.uploadedRevision!=data.revision) {
        Check(renderer.UpdateVertices(data.geometry,data.vertices,source.deformVertexCount,category),"Actual CPU upload");data.uploadedRevision=data.revision;
    }
    const auto before=renderer.DrawCount();
    for(int m=0;m<model->GetMeshCount();++m) {
        const auto* mesh=model->GetMeshPointer(m);const auto* world=actor.GetStaticObjectWorldMatrix(m);
        memcpy(draw.matrices.world.data(),world,64);
        Math::Matrix view,normal;memcpy(&view,draw.matrices.view.data(),64);normal=*world*view;
        Check(Math::MatrixInverse(&normal,nullptr,&normal)!=nullptr,"Nonsingular normal transform");
        Math::MatrixTranspose(&normal,&normal);memcpy(draw.normalTransform.data(),&normal,64);
        draw.baseVertex=mesh->GetVertexBasePosition()+(mesh->CanDeformPNTVertices()?0:source.deformVertexCount);
        draw.vertexCount=mesh->GetVertexCount();
        for(auto type:{CGrannyMaterial::TYPE_DIFFUSE_PNT,CGrannyMaterial::TYPE_BLEND_PNT})
            for(auto* group=mesh->GetTriGroupNodeList(type);group;group=group->pNextTriGroupNode) {
                draw.firstIndex=group->idxPos;draw.indexCount=group->triCount*3;
                renderer.Draw(&actor,data.geometry,texture,draw,category,part);
            }
    }
    Check(!renderer.Failed(),"Original material groups/ranges accepted");return renderer.DrawCount()-before;
}
static std::vector<uint8_t> Image(CGrannyModelInstance& actor,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,
    const TerrainTexturePtr& texture,StaticObjectDraw draw,ActorPart part,ActorCategory category,uint32_t& count)
{
    backend.Clear({true,ClearColor{.08f,.1f,.13f,1}});count=Draw(actor,renderer,texture,draw,part,category);
    std::vector<uint8_t> result;uint32_t w,h;Check(backend.CaptureRGB(result,w,h),"Raster readback");return result;
}
static void Compare(Pair& pair,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,const TerrainTexturePtr& texture,
    const std::string& label,ActorPart part,ActorCategory category,bool materials)
{
    Numeric(pair,backend,renderer);auto draw=Camera(pair);uint32_t cpuCount=0,gpuCount=0;
    for(unsigned state=0;state<(materials?12u:1u);++state) {
        draw.cull=static_cast<StaticObjectCull>(state%3);draw.blend=(state/3)%2;draw.depthWrite=state<6;
        draw.alphaTest=static_cast<StaticObjectAlphaTest>(state%3);draw.alphaReference=96;
        draw.textureAlpha=true;draw.diffuse[3]=.65f;draw.textureFactor={.7f,.9f,.6f,.65f};draw.factorAlpha=state%2;
        uint32_t aCount,bCount;auto a=Image(pair.cpu,backend,renderer,texture,draw,part,category,aCount);
        const auto bytes=renderer.BytesUploaded();auto b=Image(pair.gpu,backend,renderer,texture,draw,part,category,bCount);
        Check(renderer.BytesUploaded()==bytes,"GPU draw uploads no CPU vertices");
        Check(aCount && aCount==bCount,"CPU/GPU category draw-count parity");cpuCount+=aCount;gpuCount+=bCount;
        size_t changed=0,large=0;for(size_t i=0;i<a.size();++i) { auto delta=std::abs(int(a[i])-int(b[i]));changed+=delta!=0;large+=delta>2; }
        if(changed>a.size()/100 || large>a.size()/10000) std::cerr<<label<<" state="<<state<<" changed="<<changed<<" large="<<large<<'\n';
        Check(changed<=a.size()/100 && large<=a.size()/10000,"CPU/GPU material raster tolerance");
    }
    auto palette=pair.gpu.GetSkinningPalette();size_t changes=0,meshBones=0;
    const auto& model=*pair.gpu.GetModel()->GetSkinningData();
    for(size_t m=0;m<model.meshes.size();++m) if(model.meshes[m]) {
        meshBones=std::max(meshBones,size_t(model.meshes[m]->meshBoneCount));
        for(size_t i=0;i<model.meshes[m]->meshBoneCount;++i) changes+=model.meshes[m]->meshToSourceSkeleton[i]!=pair.gpu.GetSkinningRemaps()[m]->meshToSkeleton[i];
    }
    std::cout<<"COVERAGE "<<label<<" category="<<unsigned(category)<<" part="<<unsigned(part)<<" sourceBones="<<model.skeleton->names.size()
        <<" palette="<<(palette?palette->matrices.size():0)<<" meshPalette="<<meshBones<<" remapChanges="<<changes
        <<" rigidVertices="<<pair.gpu.GetModel()->GetRigidVertexCount()<<" cpuDraws="<<cpuCount<<" gpuDraws="<<gpuCount<<'\n';++cases;
}
static void Frame(DiligentD3D11Backend& backend,DiligentActorRenderer& renderer)
{
    Check(backend.Resize(0,0) && !backend.BeginFrame() && backend.Resize(cases%2?480:512,cases%2?480:512),"Repeated suspend/resize/restore");
    Check(backend.BeginFrame(),"Frame begin");++actorFrameSerial;renderer.ResetFrame();
}
static void Finish(DiligentD3D11Backend& backend,DiligentActorRenderer& renderer)
{ backend.EndFrame();backend.Present();renderer.ReleaseBindings(); }
static void Crowd(Asset& asset,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,const TerrainTexturePtr& texture)
{
    for(unsigned count:{1,10,25,50,100}) {
        Frame(backend,renderer);std::vector<std::unique_ptr<Pair>> actors;
        const auto written=prototypeBoneBytes;
        for(unsigned i=0;i<count;++i) {
            auto pair=std::make_unique<Pair>();pair->Set(asset);auto world=World();world._41+=i*10.f;
            pair->Deform(world,ActorPart::Body,ActorCategory::Mob);actors.push_back(std::move(pair));
        }
        Check(livePrototypeStaticMeshes==1 && livePrototypeGeometry==count && livePrototypePalettes==count,"Identical mob instances share exactly one immutable VB/IB and own separate palettes");
        const auto draw=Camera(*actors[0]);uint32_t cpu=0,gpu=0;
        for(auto& pair:actors) cpu+=Draw(pair->cpu,renderer,texture,draw,ActorPart::Body,ActorCategory::Mob);
        auto cpuBytes=renderer.BytesUploaded();
        for(auto& pair:actors) gpu+=Draw(pair->gpu,renderer,texture,draw,ActorPart::Body,ActorCategory::Mob);
        Check(cpu==gpu && renderer.BytesUploaded()==cpuBytes,"Crowd draw/upload parity");
        Check(prototypeBoneBytes-written==count*16384ull,"Measured per-owner palette writes");
        Check(cpuBytes==count*uint64_t(asset.model->GetActorSource()->vertexCount)*32,"Measured CPU vertex writes");
        std::cout<<"UPLOAD actors="<<count<<" cpuVertexBytes="<<cpuBytes<<" gpuVertexBytes=0 gpuBoneBytes="<<prototypeBoneBytes-written
            <<" sharedStaticMeshes="<<livePrototypeStaticMeshes<<" palettes="<<livePrototypePalettes<<" cpuDraws="<<cpu<<" gpuDraws="<<gpu<<'\n';
        actors.clear();Finish(backend,renderer);
        Check(!livePrototypeGeometry && !livePrototypeStaticMeshes && !livePrototypePalettes,"Crowd despawn resources zero");
    }
}
struct FallbackInstance : CGrannyModelInstance
{
    void RejectRemap() { m_skinningRemaps.clear(); }
    void RestoreRemap() { m_skinningBindingDestination.reset(); }
};
static void LinkedLods(const std::string& root,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,const TerrainTexturePtr& texture,bool quick=false)
{
    // ZiiNAN: GPU skinning actor coverage - compare rebinding against freshly bound native instances.
    for(const std::string folder:{"PC/ymir work/pc/","pc2/ymir work/pc2/"})
    for(const std::string race:{"warrior","assassin","sura","shaman"}) {
        if(quick && race!="assassin") continue;
        const auto base=root+"/"+folder+race+"/";
        const auto shape=race=="shaman"?"miyeom":"novice";
        Asset body(base+race+"_"+shape+".gr2"),lod1(base+race+"_"+shape+"_lod_01.gr2"),
            lod2(base+race+"_"+shape+"_lod_02.gr2"),lod3(base+race+"_"+shape+"_lod_03.gr2");
        Clip clip(base+"general/run.gr2");Pair actor,reduced1,reduced2,reduced3;
        actor.Set(body);reduced1.Set(lod1);reduced2.Set(lod2);reduced3.Set(lod3);
        std::vector<std::string> styles={base+"hair/hair_1_1.gr2",base+"hair/hair_2_1.gr2",base+"hair/hair_3_1.gr2"};
        if(folder=="PC/ymir work/pc/") styles.push_back(base+"hair/hair_2_2.gr2");
        if(folder=="PC/ymir work/pc/" && (race=="assassin" || race=="shaman"))
            for(const auto* name:{"hair_15_1.gr2","hair_17_1.gr2"})
                styles.push_back(root+"/metin2_patch_mundi/ymir work/pc/"+race+"/hair/"+name);
        if(quick) styles={base+"hair/hair_2_1.gr2"};
        for(const auto& path:styles) {
            const auto name=std::filesystem::path(path).filename().string();
            Asset style(path);Pair hair;
            actor.cpuOwner=&actor.cpu;actor.gpuOwner=&actor.gpu;hair.Set(style,&actor);
            unsigned step=0;std::vector<std::shared_ptr<const BoneRemap>> original;
            for(auto* current:{&actor,&reduced1,&reduced2,&reduced3,&reduced2,&reduced1,&actor}) {
                Frame(backend,renderer);
                for(auto* pose:{&actor,&reduced1,&reduced2,&reduced3}) { pose->Motion(clip,.17f);pose->Deform(World()); }
                actor.cpuOwner=&current->cpu;actor.gpuOwner=&current->gpu;
                const auto oldGeometry=std::weak_ptr<StaticObjectGeometry>(hair.gpu.GetActorRenderData().geometry);
                const auto oldRemaps=hair.gpu.GetSkinningRemaps();
                hair.Deform(World(),ActorPart::Hair);
                Pair fresh;fresh.Set(style,&actor);fresh.Deform(World(),ActorPart::Hair);
                Check(hair.cpu.GetActorRenderData().vertices==fresh.cpu.GetActorRenderData().vertices,"Rebound CPU hair equals freshly constructed native binding, no stale bone indices");
                Check(hair.gpu.GetSkinningRemaps()==fresh.gpu.GetSkinningRemaps(),"GPU remap equals fresh current native target binding");
                for(int mesh=0;mesh<style.model->GetMeshCount();++mesh)
                    Check(!std::memcmp(hair.cpu.GetStaticObjectWorldMatrix(mesh),fresh.cpu.GetStaticObjectWorldMatrix(mesh),64) &&
                        !std::memcmp(hair.gpu.GetStaticObjectWorldMatrix(mesh),fresh.gpu.GetStaticObjectWorldMatrix(mesh),64),"Rigid and skinned hair world transforms match fresh native binding");
                if(step && oldRemaps!=hair.gpu.GetSkinningRemaps()) Check(oldGeometry.expired(),"Changed LOD remap releases previous GPU geometry");
                if(!step) original=hair.gpu.GetSkinningRemaps();
                if(step==6) Check(original==hair.gpu.GetSkinningRemaps(),"Return to full LOD restores exact original remap");
                Compare(hair,backend,renderer,texture,folder+race+" "+shape+" "+name+" LOD-step="+std::to_string(step),ActorPart::Hair,ActorCategory::Player,step==1);
                Finish(backend,renderer);++step;
            }
        }
    }
    Check(!livePrototypeGeometry && !livePrototypeStaticMeshes && !livePrototypePalettes,"Hair LOD destruction releases all GPU owners");
}
static void Fallback(Asset& asset,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer)
{
    Frame(backend,renderer);FallbackInstance actor;actor.SetMainModelPointer(asset.model,nullptr);
    ActorInstanceSet targets;targets.instances[0]=&actor;targets.gpuSkinning=true;const auto world=World();
    { ActorDeformScope scope(targets);actor.Deform(&world); }
    Check(actor.GetActorRenderData().gpuPrototype,"Valid pose uses GPU before rejection");
    auto old=std::weak_ptr<StaticObjectGeometry>(actor.GetActorRenderData().geometry);
    actor.RejectRemap();
    { ActorDeformScope scope(targets);actor.Deform(&world); }
    Check(actor.GetActorRenderData().ready && !actor.GetActorRenderData().gpuPrototype &&
        actor.GetActorRenderData().vertices.size()==asset.model->GetActorSource()->vertexCount,"Invalid sidecar safely uses native CPU pose, never invisible/stale GPU data");
    Check(old.expired(),"Fallback releases incompatible GPU handle");
    actor.RestoreRemap();
    { ActorDeformScope scope(targets);actor.Deform(&world); }
    Check(actor.GetActorRenderData().gpuPrototype,"Valid binding restored after CPU fallback");
    const auto palette=actor.GetSkinningPalette();
    BonePalette invalid;invalid.skeleton=palette->skeleton;invalid.matrices=palette->matrices;invalid.ready=true;
    invalid.matrices.back()[0]=std::numeric_limits<float>::quiet_NaN();
    auto handle=actor.GetActorRenderData().geometry;
    Check(!renderer.PreparePrototype(handle,*asset.model->GetSkinningData(),actor.GetSkinningRemaps(),invalid),"NaN rejected even after cached GPU geometry exists");
    auto data=*asset.model->GetSkinningData();data.status[0]=SkinDataStatus::UnsupportedLayout;
    StaticObjectGeometryPtr rejected;
    Check(!renderer.PreparePrototype(rejected,data,actor.GetSkinningRemaps(),*palette),"Unsupported layout explicitly rejected");
    handle.reset();actor.Clear();Finish(backend,renderer);
    Check(!livePrototypeGeometry && !livePrototypeStaticMeshes && !livePrototypePalettes,"Fallback/recovery/despawn GPU owners zero");
    std::cout<<"FALLBACK missing remap -> current native CPU pose -> restored GPU; NaN and unsupported layout rejected\n";
}
int main(int argc,char** argv)
{
    HWND window=nullptr;
    try {
        Check(argc==2 || argc==3,"Real asset root required");const std::string root=argv[1];
        const bool actorsOnly=argc==3 && std::string(argv[2])=="--actors-only";
        const bool lodQuick=argc==3 && std::string(argv[2])=="--lod-quick";
        Check(argc==2 || actorsOnly || lodQuick || std::string(argv[2])=="--lod-only","Known diagnostic subset required");
        Check(startupSkinningMode==PrototypeSkinningMode::GPU,"B6 production default; CPU reference still selected per comparison scope");
        CPackManager packs;CResourceManager resources;Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        window=CreateWindowW(L"STATIC",L"B4-X coverage parity",WS_OVERLAPPEDWINDOW,0,0,512,512,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend;Check(window && backend.Initialize({window,512,512}),"Diligent backend");
        startupSkinningMode=PrototypeSkinningMode::GPUPrototype;
        {
            DiligentActorRenderer renderer(backend);Check(renderer.Initialize(),"Existing B3 pipelines");actorRenderer=&renderer;actorWorldFrame=true;
            const uint8_t checker[]={220,180,100,255,90,170,230,32,110,210,110,128,240,220,160,255};
            TerrainTextureData textureData{2,2,TerrainTextureFormat::RGBA8,{{checker,sizeof(checker),8}}};
            auto texture=renderer.UploadTexture(textureData);Check(bool(texture),"Diagnostic material texture");const auto world=World();
            if(!actorsOnly) LinkedLods(root,backend,renderer,texture,lodQuick);
            if(argc==2) {
            for(const std::string folder:{"PC/ymir work/pc/","pc2/ymir work/pc2/"}) for(const std::string race:{"warrior","assassin","sura","shaman"}) {
                const auto base=root+"/"+folder+race+"/";
                Asset body(base+race+"_novice.gr2"),armorA(base+race+"_4-1.gr2"),armorB(base+race+"_lord.gr2"),armorC(base+race+"_marry_01.gr2");
                Asset hairA(base+"hair/hair_1_1.gr2"),hairB(base+"hair/hair_2_1.gr2");
                std::vector<std::unique_ptr<Clip>> clips;
                Pair actor,hair;unsigned shape=0;
                for(auto* asset:{&body,&armorA,&armorB,&armorC,&body}) {
                    hair.cpu.Clear();hair.gpu.Clear();actor.Set(*asset);
                    for(const std::string motion:{"wait","run","attack","damage","dead"}) {
                        const auto path=base+"general/"+motion+".gr2";
                        if(!std::filesystem::exists(path)) { std::cout<<"UNAVAILABLE clip="<<path<<'\n';continue; }
                        clips.push_back(std::make_unique<Clip>(path));Frame(backend,renderer);actor.Motion(*clips.back(),.37f);actor.Deform(world);
                        Compare(actor,backend,renderer,texture,folder+race+" shape="+std::to_string(shape)+" "+motion,ActorPart::Body,ActorCategory::Player,motion=="wait");
                        if(motion=="run") for(auto* style:{&hairA,&hairB,&hairA}) {
                            hair.Set(*style,&actor);const auto bytes=prototypeBoneBytes;hair.Deform(world,ActorPart::Hair);
                            Check(prototypeBoneBytes==bytes,"Linked hair shares current body bone buffer without duplicate upload");
                            Check(livePrototypePalettes==1,"Exactly one palette owner for body and linked hair");
                            Compare(hair,backend,renderer,texture,style->path+" shape="+std::to_string(shape),ActorPart::Hair,ActorCategory::Player,false);
                        }
                        Finish(backend,renderer);
                    }
                    ++shape;
                }
            }
            }
            if(argc==2 || actorsOnly) {
            struct Example { const char* path;ActorCategory category; };
            for(const auto& example:std::initializer_list<Example>{
                {"NPC/ymir work/npc/goods/goods.gr2",ActorCategory::Npc},
                {"NPC/ymir work/npc/blacksmith/blacksmith.gr2",ActorCategory::Npc},
                {"NPC/ymir work/npc/doctor/doctor.gr2",ActorCategory::Npc},
                {"NPC/ymir work/npc/sinseon/sinseon.gr2",ActorCategory::Npc},
                {"Monster/ymir work/monster/wolf/wolf.gr2",ActorCategory::Mob},
                {"Monster/ymir work/monster/orc_soldier/orc_soldier.gr2",ActorCategory::Mob},
                {"Monster/ymir work/monster/barbarian_bow/barbarian_bow.gr2",ActorCategory::Mob},
                {"monster2/ymir work/monster2/fire_dragon/fire_dragon.gr2",ActorCategory::Mob},
                {"Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2",ActorCategory::Mob},
                {"NPC/ymir work/npc/horse/horse_normal.gr2",ActorCategory::Mount},
                {"patch2/ymir work/npc/boar/boar.gr2",ActorCategory::Mount},
                {"patch2/ymir work/npc/lion_white/lion_white.gr2",ActorCategory::Mount},
                {"metin2_patch_halloween/ymir work/npc/horse_halloween1/horse_halloween1.gr2",ActorCategory::Mount},
                {"metin2_patch_pet1/ymir work/npc/dinosaur/dinosaur_3.gr2",ActorCategory::Mount},
                {"item/ymir work/item/weapon/00010.gr2",ActorCategory::Player}}) {
                Asset asset(root+"/"+example.path);std::vector<std::unique_ptr<Clip>> clips;Pair actor;actor.Set(asset);
                const auto base=std::filesystem::path(asset.path).parent_path();bool any=false;
                for(const auto* motion:{"wait","run","attack","attack1","damage","dead","00","20","30","31"}) {
                    const auto path=base/(std::string(motion)+".gr2");if(!std::filesystem::exists(path)) continue;
                    clips.push_back(std::make_unique<Clip>(path.string()));Frame(backend,renderer);actor.Motion(*clips.back(),.37f);actor.Deform(world,ActorPart::Body,example.category);
                    Compare(actor,backend,renderer,texture,asset.path+" "+motion,ActorPart::Body,example.category,!any);any=true;Finish(backend,renderer);
                }
                if(!any) { Frame(backend,renderer);actor.Deform(world,ActorPart::Body,example.category);
                    Compare(actor,backend,renderer,texture,asset.path+" bind",ActorPart::Body,example.category,true);Finish(backend,renderer); }
            }
            Check(maxBones==163,"Real 163-bone boss represented");
            }
            if(!lodQuick) { Asset mob(root+"/Monster/ymir work/monster/wolf/wolf.gr2");Crowd(mob,backend,renderer,texture);Fallback(mob,backend,renderer); }
            texture.reset();renderer.ReleaseBindings();
            Check(!renderer.LiveGeometryCount() && !renderer.LiveTextureCount() && !renderer.MountGeometryCount() && !renderer.AttachmentGeometryCount(),"Actor/mount/attachment resources zero");
            actorRenderer=nullptr;actorWorldFrame=false;actorDeformTargets={};
        }
        resources.Destroy();BackendTestAccess::Validate(backend);backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(!warnings && !livePrototypeGeometry && !livePrototypeStaticMeshes && !livePrototypePalettes,"GPU resources and validation warnings zero");
        Check(!liveSkinMeshes && !liveBoneRemaps && !liveBonePalettes && !skinSidecarFailures,"Source resources and preparation failures zero");
        std::cout<<"PASS B4-X cases="<<cases<<" samples="<<samples<<" maxPosition="<<maxPosition<<" maxNormal="<<maxNormal<<" maxNativePoseDelta="<<maxNativePoseDelta<<" maxBones="<<maxBones
            <<" StaticSkinMeshes="<<livePrototypeStaticMeshes<<" PrototypeGeometry="<<livePrototypeGeometry<<" PrototypePalettes="<<livePrototypePalettes<<'\n';return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';if(window) DestroyWindow(window);return 1; }
}
