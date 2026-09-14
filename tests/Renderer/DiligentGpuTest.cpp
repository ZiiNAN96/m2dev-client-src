// ZiiNAN: Production GPU regression without a D3D9 reference device.
#include "EterLib/StdAfx.h"
#include "EterBase/Timer.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/Camera.h"
#include "EterLib/SourceResourceAudit.h"
#include "EterLib/TerrainTextureLoader.h"
#include "EterLib/MaterialStateSnapshot.h"
#include "GameLib/TerrainPatch.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Renderer/DiligentTerrainRenderer.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentTreeRenderer.h"
#include "Renderer/DiligentWorldRenderer.h"
#include "Renderer/DiligentUIRenderer.h"
#include "Renderer/DiligentTextRenderer.h"
#include "Renderer/TerrainPresentation.h"
#include "Renderer/Diagnostics.h"
#include "Platform/PlatformWindow.h"
#include "TerrainTextureFixtures.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
typedef struct _object PyObject;
#include "UserInterface/PythonSystem.h"
float CCamera::CAMERA_MAX_DISTANCE=2500.0f;
int CPythonSystem::GetFogLevel() { return 2; }
static void Check(bool condition,const char* message) { if(!condition) throw std::runtime_error(message); }
namespace Renderer
{
class BackendTestAccess
{
public:
    static std::vector<uint32_t> Read(DiligentD3D11Backend& backend, bool depth)
    {
        using namespace Diligent;
        auto& s = *backend.m_impl;
        auto* source = (depth ? s.swapChain->GetDepthBufferDSV() : s.swapChain->GetCurrentBackBufferRTV())->GetTexture();
        auto desc = source->GetDesc();
        desc.Name = "Terrain test readback";
        desc.Usage = USAGE_STAGING;
        desc.BindFlags = BIND_NONE;
        desc.CPUAccessFlags = CPU_ACCESS_READ;
        desc.MiscFlags = MISC_TEXTURE_FLAG_NONE;
        RefCntAutoPtr<ITexture> staging;
        s.device->CreateTexture(desc, nullptr, &staging);
        Check(staging != nullptr, "staging texture");
        CopyTextureAttribs copy;
        copy.pSrcTexture = source;
        copy.pDstTexture = staging;
        copy.SrcTextureTransitionMode = copy.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        s.context->CopyTexture(copy);
        s.context->WaitForIdle();
        MappedTextureSubresource mapped;
        s.context->MapTextureSubresource(staging, 0, 0, MAP_READ, MAP_FLAG_NONE, nullptr, mapped);
        Check(mapped.pData != nullptr, "map readback");
        std::vector<uint32_t> pixels(desc.Width * desc.Height);
        for (uint32_t y = 0; y < desc.Height; ++y)
            memcpy(pixels.data() + y * desc.Width, static_cast<const char*>(mapped.pData) + y * mapped.Stride, desc.Width * 4);
        s.context->UnmapTextureSubresource(staging, 0, 0);
        return pixels;
    }
};
}

class CameraProbe : public CScreen {
public:
    Renderer::TerrainMatrices Matrices() {
        Renderer::TerrainMatrices result;
        Math::Matrix identity; Math::MatrixIdentity(&identity);
        memcpy(result.world.data(),&identity,64);
        memcpy(result.view.data(),&ms_matView,64);
        memcpy(result.projection.data(),&ms_matProj,64);
        return result;
    }
};
#include "ActorGpuChecks.h"
#include "ActorStateIsolationChecks.h"

static void WorldMaterialChecks(Renderer::DiligentD3D11Backend& backend)
{
    using namespace Renderer;
    const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    DiligentWorldRenderer world(backend); DiligentUIRenderer ui(backend); DiligentTextRenderer text(backend);
    Check(world.Initialize() && ui.Initialize() && text.Initialize(),"world/UI/text pipelines");
    const auto bytes=TerrainFixture::AlphaDDS();
    auto image=LoadTerrainTextureMemory(bytes.data(),bytes.size(),world);
    Check(bool(image),"world CPU texture upload");
    EffectVertex quad[4]={{{-.8f,.8f,.3f},0xffffffff,{0,0}},{{-.8f,-.8f,.3f},0xffffffff,{0,1}},
        {{.8f,.8f,.3f},0xffffffff,{1,0}},{{.8f,-.8f,.3f},0xffffffff,{1,1}}};
    for(unsigned cycle=0;cycle<6;++cycle) {
        Check(backend.Resize(0,0) && !backend.BeginFrame() && backend.Resize(160,120),"world suspension/restore");
        EffectDraw draw; draw.matrices={identity,identity,identity}; draw.textureTransform=identity;
        draw.blend=cycle%2; draw.depthWrite=!draw.blend; draw.alphaTest=cycle==2; draw.alphaReference=128;
        draw.fog=cycle==3 ? 3 : 0; draw.fogParameters={0,1,1,0}; draw.fogColor={.2f,.3f,.4f,1};
        Check(backend.BeginFrame(),"world begin"); backend.Clear({true,ClearColor{0,0,0,1}});
        world.ResetFrame(); world.Draw(quad,4,image,draw,WorldPart(cycle));
        Check(!world.Failed() && world.DrawCount(WorldPart(cycle))==1,"world material submission");
        auto pixels=BackendTestAccess::Read(backend,false);
        const auto visible=std::count_if(pixels.begin(),pixels.end(),[](uint32_t value){return (value&0xffffff)!=0;});
        Check(visible>500,"world material visible pixels");
        backend.EndFrame(); backend.Present();
    }
    EffectDraw overlay; overlay.ui=true; overlay.depthTest=overlay.depthWrite=false;
    overlay.matrices={identity,identity,identity}; overlay.textureTransform=identity;
    Math::Matrix projection; Math::MatrixOrthoOffCenterRH(&projection,0,160,120,0,0,1000);
    memcpy(overlay.matrices.projection.data(),&projection,64);
    overlay.viewport={0,0,160,120}; overlay.textured=false; overlay.blend=false;
    overlay.scissor=true; overlay.clip={40,30,80,70}; overlay.colorOp=overlay.alphaOp=2;
    overlay.colorArg1=overlay.alphaArg1=0; // diffuse
    EffectVertex rect[4]={{{20,20,0},0xff00ff00,{}},{{20,90,0},0xff00ff00,{}},
        {{110,20,0},0xff00ff00,{}},{{110,90,0},0xff00ff00,{}}};
    Check(backend.BeginFrame(),"UI/text begin"); backend.Clear({true,ClearColor{0,0,0,1}});
    ui.Draw(rect,4,{},overlay); text.Draw(rect,4,{},overlay);
    Check(!ui.Failed() && !text.Failed() && ui.DrawCount()==1 && text.DrawCount()==1,"UI/text draw");
    const auto pixels=BackendTestAccess::Read(backend,false);
    Check((pixels[50*160+50]&0xffffff)==0x00ff00 && (pixels[20*160+20]&0xffffff)==0,"UI/text scissor pixels");
    backend.EndFrame(); backend.Present();
    world.ReleaseBindings(); image.reset();
    world.Shutdown(); ui.Shutdown(); text.Shutdown();
    Check(!world.LiveTextureCount() && !world.LiveBufferCount() && !ui.LiveBufferCount() &&
        !ui.LiveTextureCount() && !text.LiveTextureCount() && !text.LiveBufferCount(),"world/UI/text shutdown");
    std::cout<<"All six world parts, alpha/blend/fog, UI/text scissor pixels, resize and lifetime: PASS\n";
}

int main()
{
    using namespace Renderer;
    Platform::PlatformWindow platformWindow;
    Platform::WindowCreateInfo windowInfo;
    windowInfo.title="Device-free production GPU test";
    const bool windowCreated=platformWindow.Create(windowInfo);
    platformWindow.SetSize(640,480);
    const auto window=platformWindow.GetNativeHandle().value;
    try {
        // Camera.cpp already owns the process camera singleton, including in Debug.
        CTimer timer; CGraphicDevice graphics; CameraProbe screen;
        DiligentD3D11Backend backend;
        Check(windowCreated && window && backend.Initialize({window,640,480}) &&
              graphics.Create({window},640,480)==CGraphicDevice::CREATE_OK,"Diligent-only context");
        screen.SetPositionCamera(1600,-1600,0,5000,45,0);
        screen.SetPerspective(30,640.f/480.f,100,25600);
        {
            DiligentTerrainRenderer terrain(backend); Check(terrain.Initialize(),"terrain pipelines");
            terrainRenderer=&terrain;
            HardwareTransformPatch_SSourceVertex vertices[289];
            for(int y=0;y<17;++y) for(int x=0;x<17;++x)
                vertices[y*17+x]={Math::Vector3(float(x*200),float(-y*200),0),Math::Vector3(0,0,1)};
            const uint16_t indices[]{0,272,16,16,272,288};
            auto ib=terrain.UploadIndices(indices,6);
            CTerrainPatch patch; const bool oldSoftware=CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE;
            CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE=false; patch.BuildTerrainVertexBuffer(vertices);
            std::weak_ptr<TerrainBuffer> lifetime=patch.terrainGeometry;
            for(int pose=0;pose<4;++pose) {
                const unsigned w=pose%2?800:640,h=pose%2?600:480;
                Check(backend.Resize(0,0) && !backend.BeginFrame() && backend.Resize(w,h),"terrain suspend/resize");
                graphics.ResizeBackBuffer(w,h); screen.SetPositionCamera(1600,-1600,0,5000,45,float(pose*70));
                screen.SetPerspective(30,float(w)/h,100,25600);
                Check(backend.BeginFrame(),"terrain begin"); backend.Clear({true,ClearColor{0,0,0,1}});
                terrain.ResetFrame(); terrain.BeginTerrain(screen.Matrices(),true);
                terrain.DrawTerrain(patch.terrainGeometry,ib,6,false);
                Check(!terrain.Failed() && terrain.DrawCount()==1,"real terrain producer submission");
                auto color=BackendTestAccess::Read(backend,false),depth=BackendTestAccess::Read(backend,true);
                size_t covered=0;
                for(size_t i=0;i<color.size();++i) if(color[i]&0xffffff) { ++covered; Check((depth[i]&0xffffff)<0xffffff,"terrain depth"); }
                Check(covered>w*h/10,"terrain camera silhouette");
                backend.EndFrame(); backend.Present();
                if (pose == 0)
                {
                    HardwareTransformPatch_SSourceVertex nearerVertices[289];
                    memcpy(nearerVertices, vertices, sizeof(vertices));
                    for (auto& vertex : nearerVertices) vertex.kPosition.z += 300;
                    CTerrainPatch nearer;
                    nearer.BuildTerrainVertexBuffer(nearerVertices);
                    const auto overlap = [&](bool nearFirst)
                    {
                        Check(backend.BeginFrame(), "overlap begin");
                        backend.Clear({true,ClearColor{0,0,0,1}});
                        terrain.ResetFrame(); terrain.BeginTerrain(screen.Matrices(),true);
                        terrain.DrawTerrain(nearFirst ? nearer.terrainGeometry : patch.terrainGeometry,ib,6,false);
                        terrain.DrawTerrain(nearFirst ? patch.terrainGeometry : nearer.terrainGeometry,ib,6,false);
                        backend.EndFrame();
                        Check(!terrain.Failed(), "overlap draws");
                        return BackendTestAccess::Read(backend,true);
                    };
                    const auto nearFirst = overlap(true), farFirst = overlap(false);
                    Check(nearFirst == farFirst, "depth independent of submission order");
                    const auto center = size_t(h/2)*w+w/2;
                    Check((nearFirst[center]&0xffffff) < (depth[center]&0xffffff), "near patch occludes far patch");
                    std::cout << "Overlapping patches / depth order: PASS\n";
                }
            }
            patch.Clear(); ib.reset(); Check(lifetime.expired(),"terrain unload");
            CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE=oldSoftware; terrainRenderer=nullptr;
        }
        ActorLifetimeChecks(screen,backend);
        ActorStateIsolationChecks(backend); // Existing 720 permutations and all 12 PSOs, unchanged.
        WorldMaterialChecks(backend);
        backend.Shutdown();
        // Exercise the actual game presentation owner, not only its backend.
        {
            const bool previousDiagnostics=verboseDiagnostics;
            verboseDiagnostics=true;
            auto presentation = CreateTerrainPresentation(platformWindow,640,480);
            Check(presentation && terrainRenderer, "presentation binds terrain bridge");
            Check(actorRenderer && !actorWorldFrame,"ZiiNAN: actor owner initialized outside a world frame");
            Check(presentation->BeginFrame() && presentation->Present(), "login frame without terrain");
            const float triangle[3][6]{{0,0,0,0,0,1},{0,-3200,0,0,0,1},{3200,0,0,0,0,1}};
            float vertices[289][6]{};
            memcpy(vertices,triangle,sizeof(triangle));
            const uint16_t indices[]{0,1,2};
            auto vb=terrainRenderer->UploadVertices(vertices,289,24);
            auto ib=terrainRenderer->UploadIndices(indices,3);
            Check(vb && ib,"presentation geometry");
            const auto image=TerrainFixture::DDS();
            auto texture=LoadTerrainTextureMemory(image.data(),image.size(),*terrainRenderer);
            Check(texture!=nullptr,"presentation texture");
            std::array<float,16> uv{}; uv[0]=1.0f/640; uv[5]=-1.0f/640;
            bool previousTerrain=false; // ZiiNAN: World eligibility must survive terrain frame reset.
            for (const auto size : {std::pair{640u,480u},std::pair{800u,600u},std::pair{320u,240u},std::pair{1024u,768u}})
            {
                Check(presentation->Resize(size.first,size.second),"presentation resize");
                screen.SetPositionCamera(1600,-1600,0,5000,45,0);
                screen.SetPerspective(30,float(size.first)/size.second,100,25600);
                Check(presentation->BeginFrame(),"presentation begin");
                Check(actorWorldFrame==previousTerrain,"ZiiNAN: actor world frame after terrain reset");
                terrainRenderer->BeginTerrain(screen.Matrices(),true,&uv);
                terrainRenderer->DrawTerrain(vb,ib,3,false,texture);
                Check(presentation->Present(),"presentation terrain present");
                Check(!actorWorldFrame,"ZiiNAN: no actor uploads outside frame"); previousTerrain=true;
            }
            Check(presentation->Resize(0,0),"presentation minimize");
            Check(presentation->Resize(640,480),"presentation restore");
            Check(presentation->BeginFrame(),"textured restore begin");
            terrainRenderer->BeginTerrain(screen.Matrices(),true,&uv);
            terrainRenderer->DrawTerrain(vb,ib,3,false,texture);
            Check(presentation->Present(),"textured restore present");
            std::weak_ptr<TerrainTexture> textureLifetime=texture;
            terrainRenderer->ReleaseTexture(texture);
            Check(textureLifetime.expired(),"presentation texture release before shutdown");
            Check(presentation->BeginFrame() && presentation->Present(),"terrain to login transition");
            const auto readLog=[](const char* path) {
                std::ifstream file(path);
                return std::string(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
            };
            Check(readLog("renderer-failure.log").find("ERROR")==std::string::npos,"successful draws do not log failures");
            Check(presentation->BeginFrame(),"diagnostic failure frame begin");
            terrainRenderer->BeginTerrain(screen.Matrices(),false);
            terrainRenderer->BeginTerrain(screen.Matrices(),false); // Retain the first failure once.
            Check(!presentation->Present(),"renderer diagnostic preserves failed present");
            const auto failureLog=readLog("renderer-failure.log");
            const auto firstFailure=failureLog.find("ERROR implementation=DiligentTerrainRenderer.cpp");
            Check(firstFailure!=std::string::npos && failureLog.find("legacy terrain state mismatch")!=std::string::npos &&
                failureLog.find("ERROR",firstFailure+1)==std::string::npos,"renderer logs first failure reason once");
            const auto presentationLog=readLog("terrain-renderer.log");
            Check(presentationLog.find("failed_terrain=1 failed_objects=0 failed_actors=0 failed_trees=0 failed_effects=0 failed_world=0 failed_ui=0 failed_text=0")!=std::string::npos,
                "failed present logs exact subsystem flags before periodic snapshot");
            vb.reset(); ib.reset(); // Map handles must die before the device owner.
            presentation.reset();
            verboseDiagnostics=previousDiagnostics;
            Check(!terrainRenderer,"presentation unbinds terrain bridge");
            Check(!actorRenderer && !actorWorldFrame,"ZiiNAN: presentation unbinds actor bridge");
            std::cout << "Textured presentation resize / suspend / resume / shutdown: PASS\n";
            std::cout << "Renderer first failure reason / subsystem diagnostic: PASS\n";
        }
        Check(!activePresentation && !terrainRenderer && !actorRenderer && !treeRenderer &&
              !worldRenderer && !effectRenderer && !uiRenderer && !textRenderer,"all production owners released");
        graphics.Destroy();
        Check(liveSourceTextures==0 && liveSourceBuffers==0,"CPU owners released");
        WriteSourceResourceAudit(std::cout);
        platformWindow.Destroy(); return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; platformWindow.Destroy(); return 1; }
}
