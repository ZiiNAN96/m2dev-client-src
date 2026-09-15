#if defined(_DEBUG)
#include <crtdbg.h>
#endif
#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/Thing.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterGrnLib/ThingInstance.h"
#include "EterLib/Camera.h"
#include "EterLib/ResourceManager.h"
#include "PackLib/PackManager.h"
#include "AssetRuntime/Providers.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AnimationRuntime/AnimationRuntime.h"
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Renderer/AssetMaterialRenderData.h"
#include "Renderer/GraphicsConfig.h"
#include "Renderer/SkinningBenchmark.h"
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

float CCamera::CAMERA_MAX_DISTANCE=2500.f;
static void Check(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
#include "../Renderer/SkinningGpuReadback.h"
using namespace Renderer;
static CResource* NewThing(const char* path) {return new CGraphicThing(path);}
static void Save(const std::vector<std::uint8_t>& rgb,unsigned width,unsigned height,const std::string& name)
{
    const auto pitch=(width*3+3)&~3u;
    BITMAPFILEHEADER file{0x4d42,DWORD(54+pitch*height),0,0,54};
    BITMAPINFOHEADER info{};info.biSize=40;info.biWidth=LONG(width);info.biHeight=-LONG(height);info.biPlanes=1;info.biBitCount=24;
    std::ofstream output(name,std::ios::binary);output.write(reinterpret_cast<const char*>(&file),sizeof(file));output.write(reinterpret_cast<const char*>(&info),sizeof(info));
    std::vector<char> row(pitch);
    for(unsigned y=0;y<height;++y) {for(unsigned x=0;x<width;++x) for(unsigned c=0;c<3;++c) row[x*3+c]=static_cast<char>(rgb[(y*width+x)*3+2-c]);output.write(row.data(),row.size());}
    Check(bool(output),"native screenshot saved");
}
static void Deform(CGrannyModelInstance& actor,float x=0,float y=0)
{
    Math::Matrix world;Math::MatrixIdentity(&world);world._41=x;world._42=y;
    actor.Update(120);
    ActorInstanceSet set;set.instances[0]=&actor;set.gpuSkinning=true;
    {ActorDeformScope scope(set);actor.Deform(&world);}
    Check(actor.GetActorRenderData().ready&&actor.GetActorRenderData().gpuPrototype==actor.GetModel()->GetSkinningData()->HasSkinnedMeshes(),"normal actor chooses GPU skinning for deformable meshes");
    Check(actor.GetActorRenderData().vertices.empty(),"no production CPU vertex copy");
}
static void Draw(CGrannyModelInstance& actor,DiligentActorRenderer& renderer,const Math::Matrix& view,const Math::Matrix& projection)
{
    auto* model=actor.GetModel();auto& data=actor.GetActorRenderData();
    if(!data.geometry)data.geometry=renderer.CreateGeometry(*model->GetActorSource());
    Check(bool(data.geometry),"normal actor geometry");
    StaticObjectDraw draw;std::memcpy(draw.matrices.view.data(),&view,64);std::memcpy(draw.matrices.projection.data(),&projection,64);
    draw.ambient={.95f,.95f,.95f,1};draw.diffuse={0,0,0,1};draw.normalizeNormals=true;
    if(GetGraphicsRuntimeConfig().usePBR){draw.ambient={.18f,.18f,.18f,1};draw.diffuse={1.2f,1.2f,1.2f,1};draw.lightDirection={.4f,.5f,1,0};}
    auto& materials=actor.GetStaticObjectMaterialPalette();
    for(int m=0;m<model->GetMeshCount();++m) {
        const auto* mesh=model->GetMeshPointer(m);const auto* world=actor.GetStaticObjectWorldMatrix(m);
        std::memcpy(draw.matrices.world.data(),world,64);
        Math::Matrix normal=*world*view;Check(Math::MatrixInverse(&normal,nullptr,&normal)!=nullptr,"normal matrix invertible");Math::MatrixTranspose(&normal,&normal);std::memcpy(draw.normalTransform.data(),&normal,64);
        draw.baseVertex=mesh->GetVertexBasePosition();draw.vertexCount=mesh->GetVertexCount();
        for(auto* group=mesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode) {
            auto& material=materials.GetMaterialRef(group->mtrlIndex);ApplyAssetMaterial(material.GetAsset(),draw);
            if(GetGraphicsRuntimeConfig().usePBR)draw.material=material.GetRenderMaterial(renderer);
            auto* image=material.GetImagePointer(0);Check(image,"embedded image goes through normal texture cache");
            auto texture=image->GetAssetTexture(renderer);Check(bool(texture),"texture uploaded");
            draw.firstIndex=group->idxPos;draw.indexCount=group->triCount*3;renderer.Draw(&actor,data.geometry,texture,draw);
        }
    }
    Check(!renderer.Failed(),"normal Diligent actor draw");
}
static void Numeric(DiligentD3D11Backend& backend,CGrannyModelInstance& actor)
{
    const auto palette=actor.GetSkinningPalette();Check(bool(palette),"native bone palette");
    for(const auto& mesh:actor.GetModel()->GetSkinningData()->meshes) if(mesh) {
        auto vertices=mesh->vertices;
        for(auto& vertex:vertices) for(unsigned k=0;k<4;++k) vertex.indices[k]=static_cast<std::uint8_t>(mesh->meshToSourceSkeleton[vertex.indices[k]]);
        const auto gpu=BackendTestAccess::Skin(backend,vertices,*palette);
        for(std::size_t v=0;v<vertices.size();++v) for(unsigned component=0;component<3;++component) {
            double expectedPosition=0,expectedNormal=0;const auto& vertex=vertices[v];
            for(unsigned k=0;k<4;++k) {
                const auto& matrix=palette->matrices[vertex.indices[k]];const double weight=vertex.weights[k]/255.0;
                double position=matrix[12+component],normal=0;
                for(unsigned axis=0;axis<3;++axis) {position+=vertex.position[axis]*matrix[axis*4+component];normal+=vertex.normal[axis]*matrix[axis*4+component];}
                expectedPosition+=position*weight;expectedNormal+=normal*weight;
            }
            Check(std::abs(gpu[v*2][component]-expectedPosition)<.002&&std::abs(gpu[v*2+1][component]-expectedNormal)<.0001,"GPU position/normal parity against double reference math");
        }
    }
}
int main(int argc,char** argv)
{
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    std::cout << std::unitbuf;
    HWND window=nullptr;
    try {
        Check(argc==2||(argc==3&&std::string_view(argv[2])=="--modern"),"character fixture path and optional --modern");
        if(argc==3){Graphics::GraphicsSettings s;s.style=Graphics::GraphicsStyle::Modern;ApplyGraphicsRuntimeConfig(Graphics::Resolve(s,1));}
        CPackManager packs;CResourceManager resources;
        for(const auto extension:AssetRuntime::ModelExtensions())resources.RegisterResourceNewFunctionPointer(extension.data(),NewThing);
        window=CreateWindowW(L"STATIC",L"F5-X shared production actor proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend;Check(window&&backend.Initialize({window,960,640}),"native Diligent backend");
        {
            DiligentActorRenderer renderer(backend);Check(renderer.Initialize(),"production actor renderer");actorRenderer=&renderer;actorWorldFrame=true;
            {
                CGraphicThing::TRef thing(resources.GetResourcePointer(argv[1]));Check(!thing.IsNull()&&!thing->IsEmpty(),"native ResourceManager GLB character");
                CGraphicThing::TRef shared(resources.GetResourcePointer(argv[1]));Check(thing.GetPointer()==shared.GetPointer(),"one loaded model document");
                auto* model=thing->GetModelPointer(0);Check(model&&thing->GetMotionCount()==6,"normal model and six motions");
                CGraphicThingInstance mapping;for(unsigned i=0;i<6;++i)mapping.RegisterMotionThing(100+i,thing.GetPointer(),i);
                for(unsigned i=0;i<6;++i)Check(mapping.GetMotionClipIndex(100+i)==int(i),"normal motion mapping retains source clip IDs");
                std::vector<std::unique_ptr<CGrannyModelInstance>> actors;
                for(int i=0;i<20;++i) {auto actor=std::make_unique<CGrannyModelInstance>();actor->SetMainModelPointer(model,nullptr);Check(!actor->IsEmpty(),"normal actor instance");actors.push_back(std::move(actor));}
                const auto counts=AnimationRuntime::GetLifetimeCounts();Check(counts.skeletons==1&&counts.clips==6,"all instances share immutable skeleton and clips");
                Math::Vector3 eye(280,-510,255),target(0,0,95),up(0,0,1);Math::Matrix view,projection;
                Math::MatrixLookAtRH(&view,&eye,&target,&up);Math::MatrixPerspectiveFovRH(&projection,.60f,1.5f,1,3000);
                for(int clip=0;clip<6;++clip) {
                    auto& actor=*actors[0];actor.SetLocalTime(0);actor.SetMotionPointer(thing->GetMotionPointer(clip),0,clip<3?0:1,1);
                    actor.SetLocalTime(thing->GetMotionPointer(clip)->GetDuration()*(clip==5?1.f:.3f));
                    Check(backend.BeginFrame(),"native frame");renderer.ResetFrame();++actorFrameSerial;backend.Clear({true,ClearColor{.04f,.05f,.07f,1}});
                    Deform(actor);Draw(actor,renderer,view,projection);Check(renderer.DrawCount()==2,"both material primitives draw");
                    BackendTestAccess::ValidateUploadedPalette(backend,*actor.GetSkinningPalette());
                    std::vector<std::uint8_t> rgb;unsigned w{},h{};Check(backend.CaptureRGB(rgb,w,h),"native readback");
                    Check(std::count_if(rgb.begin(),rgb.end(),[](auto v){return v>55;})>2000,"textured character is visible");Save(rgb,w,h,"f5x-clip-"+std::to_string(clip)+".bmp");
                    backend.EndFrame();backend.Present();renderer.ReleaseBindings();Numeric(backend,actor);
                }
                eye={900,-1400,1000};target={0,160,80};Math::MatrixLookAtRH(&view,&eye,&target,&up);Math::MatrixPerspectiveFovRH(&projection,.70f,1.5f,1,4000);
                Check(backend.BeginFrame(),"multi-instance frame");renderer.ResetFrame();++actorFrameSerial;backend.Clear({true,ClearColor{.04f,.05f,.07f,1}});
                for(int i=0;i<20;++i) {
                    auto& actor=*actors[i];actor.SetLocalTime(0);actor.SetMotionPointer(thing->GetMotionPointer(i%6),0,i%6<3?0:1,1);actor.SetLocalTime(.08f*i);
                    Deform(actor,(i%5-2)*160.f,(i/5)*190.f);Draw(actor,renderer,view,projection);
                }
                std::cout<<"Multi-instance draws="<<renderer.DrawCount()<<" geometry wrappers="<<livePrototypeGeometry
                    <<" shared mesh buffers="<<livePrototypeStaticMeshes<<" textures="<<renderer.LiveTextureCount()<<'\n';
                Check(renderer.DrawCount()==40&&livePrototypeStaticMeshes==1&&renderer.LiveTextureCount()==(argc==3?2u:1u),"20 actors share immutable mesh buffers and texture, all primitives draw");
                std::vector<std::uint8_t> rgb;unsigned w{},h{};Check(backend.CaptureRGB(rgb,w,h),"20 actors native readback");Save(rgb,w,h,"f5x-20-actors.bmp");
                backend.EndFrame();backend.Present();renderer.ReleaseBindings();
                {
                    const auto path=(std::filesystem::path(argv[1]).parent_path()/"hand_prop.glb").string();
                    CGraphicThing::TRef propThing(resources.GetResourcePointer(path.c_str()));Check(!propThing.IsNull()&&!propThing->IsEmpty(),"rigid prop loads normally");
                    CGrannyModelInstance prop;prop.SetMainModelPointer(propThing->GetModelPointer(0),nullptr);
                    const auto binding=AssetRuntime::ResolveAttachment(*model->GetAssetHandle().Get()->skeleton,12,AssetRuntime::AttachmentKind::Weapon);
                    Check(bool(binding),"stable RightHand attachment binding");prop.SetParentModelInstance(actors[0].get(),binding.bone);
                    eye={280,-510,255};target={0,0,95};Math::MatrixLookAtRH(&view,&eye,&target,&up);
                    Check(backend.BeginFrame(),"attachment frame");renderer.ResetFrame();++actorFrameSerial;backend.Clear({true,ClearColor{.04f,.05f,.07f,1}});
                    actors[0]->SetLocalTime(0);actors[0]->SetMotionPointer(thing->GetMotionPointer(3),0,1,1);actors[0]->SetLocalTime(.2f);
                    Deform(*actors[0]);Deform(prop);Draw(*actors[0],renderer,view,projection);Draw(prop,renderer,view,projection);
                    const auto* expected=actors[0]->GetBoneMatrixPointer(12);const auto* actual=prop.GetBoneMatrixPointer(0);
                    Check(expected&&actual,"attachment world matrices available");
                    for(unsigned k=0;k<16;++k)Check(std::abs(expected[k]-actual[k])<.001,"rigid prop follows animated hand matrix");
                    Check(backend.CaptureRGB(rgb,w,h),"attachment image readback");Save(rgb,w,h,"f5x-hand-attachment.bmp");
                    backend.EndFrame();backend.Present();renderer.ReleaseBindings();prop.Clear();propThing.Clear();
                }
                Check(SetWindowPos(window,nullptr,0,0,720,480,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE,"owned native window resize");
                Check(backend.Resize(720,480),"resize backend");
                ShowWindow(window,SW_MINIMIZE);Check(IsIconic(window)!=FALSE,"owned native window minimized");
                Check(backend.Resize(0,0),"minimized backend suspended");
                ShowWindow(window,SW_RESTORE);Check(IsIconic(window)==FALSE,"owned native window restored");
                Check(SetWindowPos(window,nullptr,0,0,960,640,SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE)!=FALSE&&backend.Resize(960,640),"restored native window and backend size");
                Check(backend.BeginFrame(),"post-restore animated actor frame");renderer.ResetFrame();++actorFrameSerial;
                backend.Clear({true,ClearColor{.04f,.05f,.07f,1}});
                actors[0]->SetLocalTime(.4f);Deform(*actors[0]);Draw(*actors[0],renderer,view,projection);
                Check(backend.CaptureRGB(rgb,w,h)&&w==960&&h==640,"post-restore readback");Save(rgb,w,h,"f5x-window-restored.bmp");
                backend.EndFrame();backend.Present();renderer.ReleaseBindings();
                std::cout<<"NativeWindow resize=720x480 minimized=1 restored=1 final=960x640 postRestoreAnimatedDraw=PASS\n";
                BackendTestAccess::Validate(backend);
                mapping.Clear();actors.clear();shared.Clear();thing.Clear();
            }
            resources.DestroyDeletingList();resources.Destroy();renderer.ReleaseBindings();
            renderer.ResetFrame();
            Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0&&livePrototypeGeometry==0&&livePrototypePalettes==0,"GPU geometry/palettes/textures released");
            actorRenderer=nullptr;actorWorldFrame=false;
        }
        backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(AssetRuntime::liveDocuments==0&&AssetRuntime::liveAnimationInstances==0&&AssetRuntime::liveMeshBindings==0&&
            AnimationRuntime::GetLifetimeCounts().clips==0&&AnimationRuntime::GetLifetimeCounts().skeletons==0,"all GLB and animation lifetime counts zero");
        Check(skinningCpuCalls==0&&skinningFallbacks==0&&liveSkinMeshes==0&&liveBoneRemaps==0&&liveBonePalettes==0,"CPU deformation=0 fallback=0 skin resources=0");
        if(argc==3)Check(pbrDraws>0 && liveMaterialRuntimeObjects==0 && livePBRBindings==0 && livePBRPipelines==0,"PBR used and every material owner released");
        std::cout<<"PASS F5-X native normal actor/GPU renderer, 6 clips, 20 actors, readbacks, numeric parity, cache sharing, lifecycle, resources=0\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';if(window)DestroyWindow(window);return 1;}
}
