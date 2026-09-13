// ZiiNAN: GPU skinning parity validation
#define main B4CoverageMain
#include "SkinningCoverageTest.cpp"
#undef main

int main(int argc,char** argv)
{
    HWND window=nullptr;
    try {
        Check(argc==2,"Original asset root required");
        Check(startupSkinningMode==PrototypeSkinningMode::GPU,"B6 production default; forced CPU fallback retained");
        CPackManager packs;CResourceManager resources;Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        window=CreateWindowW(L"STATIC",L"B5-X fallback parity",WS_OVERLAPPEDWINDOW,0,0,512,512,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend;Check(window && backend.Initialize({window,512,512}),"Fallback test backend");
        startupSkinningMode=PrototypeSkinningMode::GPUPrototype;
        {
            DiligentActorRenderer renderer(backend);Check(renderer.Initialize(),"Existing pipelines");actorRenderer=&renderer;actorWorldFrame=true;
            const uint8_t pixel[]={180,220,120,255};TerrainTextureData data{1,1,TerrainTextureFormat::RGBA8,{{pixel,4,4}}};
            auto texture=renderer.UploadTexture(data);Check(bool(texture),"Fallback comparison texture");
            {
                const auto base=std::string(argv[1])+"/Monster/ymir work/monster/wolf/";
                Asset asset(base+"wolf.gr2");Clip clip(base+"20.gr2");
                for(unsigned cycle=0;cycle<8;++cycle) {
                    Frame(backend,renderer);Pair reference;reference.Set(asset);FallbackInstance actor;actor.SetMainModelPointer(asset.model,nullptr);
                    const auto time=.13f+cycle*.11f;reference.Motion(clip,time);
                    actor.SetMotionPointer(&clip.motion,0);actor.SetLocalTime(time);actor.Update(120);
                    const auto world=World();reference.Deform(world,ActorPart::Body,ActorCategory::Mob);
                    ActorInstanceSet targets;targets.category=ActorCategory::Mob;targets.instances[0]=&actor;targets.gpuSkinning=true;
                    {ActorDeformScope scope(targets);actor.Deform(&world);}
                    Check(actor.GetActorRenderData().gpuPrototype,"Initial GPU pose ready");
                    const auto previous=std::weak_ptr<StaticObjectGeometry>(actor.GetActorRenderData().geometry);
                    const auto cpuFrames=prototypeCpuFrames;actor.RejectRemap();
                    {ActorDeformScope scope(targets);actor.Deform(&world);}
                    Check(!actor.GetActorRenderData().gpuPrototype && actor.GetActorRenderData().ready && previous.expired(),"Fallback never retains stale GPU handle");
                    Check(prototypeCpuFrames==cpuFrames+1,"Exactly one native fallback deformation");
                    Check(actor.GetActorRenderData().vertices==reference.cpu.GetActorRenderData().vertices,"Fallback vertices/normals exactly match current CPU pose");
                    const auto draw=Camera(reference);uint32_t ac=0,bc=0;
                    auto expected=Image(reference.cpu,backend,renderer,texture,draw,ActorPart::Body,ActorCategory::Mob,ac);
                    auto actual=Image(actor,backend,renderer,texture,draw,ActorPart::Body,ActorCategory::Mob,bc);
                    Check(ac && ac==bc && actual==expected,"Fallback raster exactly matches native CPU reference");
                    actor.RestoreRemap();{ActorDeformScope scope(targets);actor.Deform(&world);}
                    Check(actor.GetActorRenderData().gpuPrototype,"Recovery returns to GPU without CPU/GPU mixed state");
                    auto recovered=Image(actor,backend,renderer,texture,draw,ActorPart::Body,ActorCategory::Mob,bc);
                    auto gpuReference=Image(reference.gpu,backend,renderer,texture,draw,ActorPart::Body,ActorCategory::Mob,ac);
                    Check(ac && ac==bc && recovered==gpuReference,"Recovered raster exactly matches fresh GPU instance");
                    BackendTestAccess::ValidateUploadedPalette(backend,*reference.gpu.GetSkinningPalette());
                    actor.Clear();reference.cpu.Clear();reference.gpu.Clear();Finish(backend,renderer);
                    Check(!livePrototypeGeometry && !livePrototypeStaticMeshes && !livePrototypePalettes && !renderer.LiveGeometryCount(),"Fallback cycle frees all GPU and CPU geometry");
                    std::cout<<"FALLBACK_PARITY cycle="<<cycle<<" cpuVertexDelta=0 cpuPixelDelta=0 recoveredGpuPixelDelta=0 owners=0\n";
                }
            }
            texture.reset();renderer.ReleaseBindings();Check(!renderer.LiveTextureCount(),"Fallback texture released");
            actorRenderer=nullptr;actorWorldFrame=false;actorDeformTargets={};
        }
        resources.Destroy();BackendTestAccess::Validate(backend);backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(!warnings && !skinSidecarFailures && !liveSkinMeshes && !liveBoneRemaps && !liveBonePalettes,"No warnings or leaked source owners");
        std::cout<<"PASS B5-X fallback cycles=8 intentionalFallbacks="<<prototypeCpuFrames<<" errors=0 resources=0\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL fallback parity: "<<error.what()<<'\n';if(window) DestroyWindow(window);return 1;}
}
