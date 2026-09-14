// ZiiNAN: GPU skinning production path — isolated, animated production-path measurements.
#define main CoverageReferenceMain
#include "SkinningCoverageTest.cpp"
#undef main
#include <iomanip>
#include "EterLib/GrpDevice.h"

struct BenchmarkActor {
    CGrannyModelInstance instance;
    ActorCategory category;
    Math::Matrix world;
};
struct BenchmarkSample {
    double cpuSkinUs,prepUs,deformUs,renderUs,cpuFrameUs,wallFrameUs;
    uint64_t cpuCalls,cpuVertices,boneBytes,vertexBytes,uploads,draws,gpuCalls;
};
static StaticObjectDraw BenchmarkCamera()
{
    const Math::Vector3 eye(0,-3000,2200),center(0,0,80),up(0,0,1);
    Math::Matrix view,projection;Math::MatrixLookAtRH(&view,&eye,&center,&up);
    Math::MatrixOrthoRH(&projection,2800,1575,1,8000);
    StaticObjectDraw draw;memcpy(draw.matrices.view.data(),&view,64);memcpy(draw.matrices.projection.data(),&projection,64);
    draw.ambient={.6f,.6f,.6f,1};draw.diffuse={.4f,.4f,.4f,1};draw.lightDirection={.3f,.4f,.8660254f,0};
    draw.normalizeNormals=true;draw.cull=StaticObjectCull::None;return draw;
}
int main(int argc,char** argv)
{
    HWND window=nullptr;
    try {
        Check(argc==3 || argc==4,"asset root, CSV path, optional --quick required");
        const std::string root=argv[1];const bool quick=argc==4 && std::string(argv[3])=="--quick";
        Check(argc==3 || quick,"known benchmark arguments");
        const unsigned warmup=quick?4:120,frames=quick?12:600,rounds=quick?1:3;
        const auto output=std::filesystem::path(argv[2]);std::filesystem::create_directories(output.parent_path());
        std::ofstream csv(output);Check(bool(csv),"benchmark CSV");csv<<std::setprecision(12);
        csv<<"round,actors,mode,frame,cpu_skin_us,gpu_prep_us,deform_us,render_cpu_us,cpu_frame_us,wall_frame_us,gpu_frame_us,cpu_calls,cpu_vertices,bone_bytes,vertex_bytes,vertex_updates,draws,gpu_deforms,static_meshes,palettes\n";
        CPackManager packs;CResourceManager resources;Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        window=CreateWindowW(L"STATIC",L"B6-X isolated benchmark",WS_OVERLAPPEDWINDOW,0,0,1280,720,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend;Check(window && backend.Initialize({window,1280,720}),"benchmark backend");
        CGraphicDevice graphics;Check(graphics.Create({window},1280,720)==CGraphicDevice::CREATE_OK,"Normal CPU capabilities and draw-state initialization");
        extern bool CPU_HAS_SSE2;std::cout<<"Native CPU_HAS_SSE2="<<CPU_HAS_SSE2<<'\n';
        BackendTestAccess::Describe(backend);startupSkinningMode=PrototypeSkinningMode::GPUPrototype;
        {
            DiligentActorRenderer renderer(backend);Check(renderer.Initialize(),"both native pipelines");actorRenderer=&renderer;actorWorldFrame=true;
            const uint8_t checker[]={220,180,100,255,90,170,230,255,110,210,110,255,240,220,160,255};
            auto texture=renderer.UploadTexture({2,2,TerrainTextureFormat::RGBA8,{{checker,sizeof(checker),8}}});Check(bool(texture),"benchmark texture");
            std::vector<std::unique_ptr<Asset>> assets;std::vector<std::unique_ptr<Clip>> clips;
            for(const auto& paths:std::initializer_list<std::pair<const char*,const char*>>{
                {"PC/ymir work/pc/warrior/warrior_novice.gr2","PC/ymir work/pc/warrior/general/run.gr2"},
                {"NPC/ymir work/npc/goods/goods.gr2","NPC/ymir work/npc/goods/wait.gr2"},
                {"Monster/ymir work/monster/wolf/wolf.gr2","Monster/ymir work/monster/wolf/03.gr2"},
                {"NPC/ymir work/npc/horse/horse_normal.gr2","NPC/ymir work/npc/horse/03.gr2"}}) {
                assets.push_back(std::make_unique<Asset>(root+"/"+paths.first));clips.push_back(std::make_unique<Clip>(root+"/"+paths.second));
                std::cout<<"ASSET "<<paths.first<<" deformVertices="<<assets.back()->model->GetDeformVertexCount()<<'\n';
            }
            const auto camera=BenchmarkCamera();
            for(unsigned round=0;round<rounds;++round) for(unsigned count:{1,10,25,50,100}) for(unsigned order=0;order<2;++order) {
                const bool gpu=(order+round)%2!=0;startupSkinningMode=gpu?PrototypeSkinningMode::GPUPrototype:PrototypeSkinningMode::CPU;
                std::vector<std::unique_ptr<BenchmarkActor>> actors;
                uint64_t expectedDraws=0;
                for(unsigned i=0;i<count;++i) {
                    auto actor=std::make_unique<BenchmarkActor>();actor->instance.SetMainModelPointer(assets[i%4]->model,nullptr);
                    actor->instance.SetMotionPointer(&clips[i%4]->motion,0);
                    actor->category=std::array{ActorCategory::Player,ActorCategory::Npc,ActorCategory::Mob,ActorCategory::Mount}[i%4];
                    Math::MatrixIdentity(&actor->world);actor->world._41=(int(i%10)-4.5f)*230;actor->world._42=(int(i/10)-4.5f)*125;
                    actors.push_back(std::move(actor));
                    auto* model=assets[i%4]->model;
                    for(int m=0;m<model->GetMeshCount();++m)
                        for(auto type:{CGrannyMaterial::TYPE_DIFFUSE_PNT,CGrannyMaterial::TYPE_BLEND_PNT})
                            for(auto* group=model->GetMeshPointer(m)->GetTriGroupNodeList(type);group;group=group->pNextTriGroupNode) ++expectedDraws;
                }
                std::vector<Diligent::RefCntAutoPtr<Diligent::IQuery>> queries;
                for(unsigned f=0;f<frames;++f) queries.push_back(BackendTestAccess::CreateTiming(backend));
                std::vector<BenchmarkSample> samples;samples.reserve(frames);
                for(unsigned f=0;f<warmup+frames;++f) {
                    const bool measured=f>=warmup;
                    const auto start=PrototypeClock::now();Check(backend.BeginFrame(),"timed frame");++actorFrameSerial;renderer.ResetFrame();
                    if(measured) BackendTestAccess::StartTiming(backend,queries[f-warmup]);
                    backend.Clear({true,ClearColor{.08f,.1f,.13f,1}});
                    const auto cpu=prototypeCpuFrames,verts=prototypeCpuBytes,bones=prototypeBoneBytes,gpuFrames=prototypeFrames;
                    const auto allCpu=skinningCpuCalls;
                    const auto skin=prototypeCpuSkinUs,prep=prototypePrepareUs;const auto deform=PrototypeClock::now();
                    for(unsigned i=0;i<count;++i) {
                        auto& actor=*actors[i];actor.instance.SetLocalTime(float(f)/60.f+float(i%7)*.13f);actor.instance.Update(120);
                        ActorInstanceSet targets;targets.category=actor.category;targets.instances[0]=&actor.instance;targets.gpuSkinning=true;
                        ActorDeformScope scope(targets);actor.instance.Deform(&actor.world);
                        Check(actor.instance.GetActorRenderData().ready && actor.instance.GetActorRenderData().gpuPrototype==gpu,"No silent fallback");
                    }
                    const auto deformUs=PrototypeMicroseconds(deform);const auto renderStart=PrototypeClock::now();
                    for(auto& actor:actors) Draw(actor->instance,renderer,texture,camera,ActorPart::Body,actor->category);
                    const auto renderUs=PrototypeMicroseconds(renderStart);
                    Check(renderer.DrawCount()==expectedDraws,"CPU/GPU draw counts equal the original material-group count");
                    if(measured) BackendTestAccess::EndTiming(backend,queries[f-warmup]);
                    backend.EndFrame();const auto cpuFrameUs=PrototypeMicroseconds(start);
                    BackendTestAccess::PresentUnthrottled(backend);const auto wallUs=PrototypeMicroseconds(start);
                    if(measured) samples.push_back({prototypeCpuSkinUs-skin,prototypePrepareUs-prep,deformUs,renderUs,cpuFrameUs,wallUs,
                        prototypeCpuFrames-cpu,(prototypeCpuBytes-verts)/32,prototypeBoneBytes-bones,renderer.BytesUploaded(),renderer.Uploads(),renderer.DrawCount(),prototypeFrames-gpuFrames});
                    Check(gpu ? skinningCpuCalls==allCpu && prototypeCpuFrames==cpu && renderer.Uploads()==0 : prototypeFrames==gpuFrames,"GPU performs no CPU deformation/upload anywhere");
                }
                const auto shared=livePrototypeStaticMeshes.load(),palettes=livePrototypePalettes.load();
                Check(!gpu || (shared==std::min(count,4u) && palettes==count),"Asset meshes shared; exactly one palette per actor");
                BackendTestAccess::Idle(backend);
                for(unsigned f=0;f<frames;++f) {
                    Diligent::QueryDataDuration duration;Check(queries[f]->GetData(&duration,sizeof(duration),true) && duration.Frequency,"Valid GPU timing");
                    const auto& s=samples[f];csv<<round<<','<<count<<','<<(gpu?"gpu":"cpu")<<','<<f<<','<<s.cpuSkinUs<<','<<s.prepUs<<','<<s.deformUs<<','<<s.renderUs<<','<<s.cpuFrameUs<<','<<s.wallFrameUs<<','
                        <<double(duration.Duration)*1e6/double(duration.Frequency)<<','<<s.cpuCalls<<','<<s.cpuVertices<<','<<s.boneBytes<<','<<s.vertexBytes<<','<<s.uploads<<','<<s.draws<<','<<s.gpuCalls<<','<<shared<<','<<palettes<<'\n';
                }
                actors.clear();renderer.ReleaseBindings();
                Check(!livePrototypeGeometry && !livePrototypeStaticMeshes && !livePrototypePalettes && !renderer.LiveGeometryCount(),"Block resources released");
                csv.flush();std::cout<<"BLOCK round="<<round<<" actors="<<count<<" mode="<<(gpu?"gpu":"cpu")<<" samples="<<samples.size()<<std::endl;
            }
            texture.reset();renderer.ReleaseBindings();Check(!renderer.LiveTextureCount(),"texture released");actorRenderer=nullptr;actorWorldFrame=false;
        }
        resources.Destroy();BackendTestAccess::Validate(backend);backend.Shutdown();graphics.Destroy();DestroyWindow(window);window=nullptr;
        Check(!warnings && !liveSkinMeshes && !liveBoneRemaps && !liveBonePalettes && !skinSidecarFailures,"Source owners/failures zero");
        std::cout<<"PASS B6-X benchmark owners=0 failures=0\n";return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';if(window) DestroyWindow(window);return 1; }
}
