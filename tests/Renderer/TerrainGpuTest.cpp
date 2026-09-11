#include "EterLib/StdAfx.h"
#include "EterBase/Timer.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/Camera.h"
#include "EterLib/StateManager.h"
#include "EterLib/LegacyD3D9Backend.h"
#include "GameLib/TerrainPatch.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Renderer/DiligentTerrainRenderer.h"
#include "Renderer/TerrainPresentation.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>

float CCamera::CAMERA_MAX_DISTANCE = 2500.0f;
static void Check(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }
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
class LegacyProbe : public CScreen
{
public:
    Renderer::TerrainMatrices Matrices()
    {
        Renderer::TerrainMatrices result;
        D3DXMATRIX world;
        D3DXMatrixIdentity(&world);
        memcpy(result.world.data(), &world, 64);
        memcpy(result.view.data(), &ms_matView, 64);
        memcpy(result.projection.data(), &ms_matProj, 64);
        return result;
    }
    static std::vector<uint32_t> Read(uint32_t width, uint32_t height)
    {
        IDirect3DSurface9 *source = nullptr, *staging = nullptr;
        Check(SUCCEEDED(ms_lpd3dDevice->GetRenderTarget(0, &source)), "legacy target");
        D3DSURFACE_DESC desc;
        source->GetDesc(&desc);
        Check(SUCCEEDED(ms_lpd3dDevice->CreateOffscreenPlainSurface(width, height, desc.Format, D3DPOOL_SYSTEMMEM, &staging, nullptr)), "legacy staging");
        Check(SUCCEEDED(ms_lpd3dDevice->GetRenderTargetData(source, staging)), "legacy readback");
        D3DLOCKED_RECT mapped;
        Check(SUCCEEDED(staging->LockRect(&mapped, nullptr, D3DLOCK_READONLY)), "legacy map");
        std::vector<uint32_t> pixels(width * height);
        for (uint32_t y = 0; y < height; ++y)
            memcpy(pixels.data() + y * width, static_cast<const char*>(mapped.pBits) + y * mapped.Pitch, width * 4);
        staging->UnlockRect(); staging->Release(); source->Release();
        return pixels;
    }
};

int main()
{
    using namespace Renderer;
    WNDCLASSW wc{};
    wc.hInstance = GetModuleHandleW(nullptr); wc.lpfnWndProc = DefWindowProcW; wc.lpszClassName = L"TerrainGPUParity";
    RegisterClassW(&wc);
    HWND legacyWindow = CreateWindowW(wc.lpszClassName, L"Legacy", WS_OVERLAPPEDWINDOW, 0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
    HWND modernWindow = CreateWindowW(wc.lpszClassName, L"Diligent", WS_OVERLAPPEDWINDOW, 0,0,640,480,nullptr,nullptr,wc.hInstance,nullptr);
    int result = 0;
    try
    {
        CTimer timer;
        CCameraManager cameras;
        CGraphicDevice device;
        LegacyProbe screen;
        LegacyD3D9Backend legacy(device, screen);
        DiligentD3D11Backend modern;
        Check(legacy.Initialize({legacyWindow,640,480}) && modern.Initialize({modernWindow,640,480}), "devices");
        {
            DiligentTerrainRenderer terrain(modern);
            Check(terrain.Initialize(), "terrain PSOs/shaders");
            terrainRenderer = &terrain;
            // Small controlled fixture, not a replacement map loader or camera.
            HardwareTransformPatch_SSourceVertex vertices[289];
            for (int y=0; y<17; ++y) for (int x=0; x<17; ++x)
                vertices[y*17+x] = {D3DXVECTOR3(float(x*200),float(-y*200),0), D3DXVECTOR3(0,0,1)};
            const uint16_t indices[]{0,272,16,16,272,288};
            auto ib = terrain.UploadIndices(indices, 6);
            auto strip = terrain.UploadIndices(indices, 3);
            Check(ib && strip, "terrain indices");
            const bool oldSoftware = CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE;
            CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE = false;
            CTerrainPatch patch;
            patch.BuildTerrainVertexBuffer(vertices); // The real upload bridge.
            std::weak_ptr<TerrainBuffer> lifetime = patch.terrainGeometry;
            Check(!lifetime.expired(), "patch upload handle");
            uint32_t width=640, height=480;
            for (int pose=0; pose<4; ++pose)
            {
                if (pose==2) { width=800; height=600; Check(legacy.Resize(width,height)&&modern.Resize(width,height), "resize"); }
                if (pose==3) { Check(modern.Resize(0,0)&&!modern.BeginFrame(), "minimize"); Check(modern.Resize(width,height), "restore"); }
                screen.SetPositionCamera(1600,-1600,0,5000,45,float(pose*70));
                screen.SetPerspective(30,float(width)/height,100,25600);
                auto matrices=screen.Matrices();
                Check(legacy.BeginFrame() && modern.BeginFrame(), "begin frame");
                legacy.Clear({true,ClearColor{0,0,0,1}}); modern.Clear({true,ClearColor{0,0,0,1}});
                terrain.ResetFrame(); terrain.BeginTerrain(matrices,true);
                terrain.DrawTerrain(patch.terrainGeometry,ib,6,false);
                // Deliberately repeat a strip at equal depth: LESS_EQUAL, both PSO variants.
                terrain.DrawTerrain(patch.terrainGeometry,strip,3,true);
                D3DXMATRIX identity; D3DXMatrixIdentity(&identity);
                STATEMANAGER.SetTransform(D3DTS_WORLD,&identity);
                STATEMANAGER.SetFVF(D3DFVF_XYZ|D3DFVF_NORMAL);
                STATEMANAGER.SetRenderState(D3DRS_LIGHTING,FALSE);
                STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
                STATEMANAGER.SetRenderState(D3DRS_ALPHATESTENABLE,FALSE);
                STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,FALSE);
                STATEMANAGER.SetRenderState(D3DRS_TEXTUREFACTOR,0xffffffff);
                STATEMANAGER.SetTexture(0,nullptr); STATEMANAGER.SetTexture(1,nullptr);
                STATEMANAGER.SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);
                STATEMANAGER.SetTextureStageState(0,D3DTSS_COLORARG1,D3DTA_TFACTOR);
                STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
                STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,289,2,indices,D3DFMT_INDEX16,vertices,24);
                legacy.EndFrame(); modern.EndFrame();
                Check(!terrain.Failed() && terrain.DrawCount()==2, "terrain draws");
                const auto d9=LegacyProbe::Read(width,height);
                const auto d11=BackendTestAccess::Read(modern,false);
                const auto depth=BackendTestAccess::Read(modern,true);
                size_t covered=0, disagreement=0;
                for(size_t i=0;i<d11.size();++i)
                {
                    const bool a=(d9[i]&0xffffff)!=0, b=(d11[i]&0xffffff)!=0;
                    covered+=b; disagreement+=(a!=b);
                    if(b) Check((depth[i]&0xffffff)<0xffffff, "terrain wrote depth");
                }
                Check(covered > width*height/10, "visible front faces");
                // D3D9's half-pixel rasterization may move the outer silhouette one pixel.
                Check(disagreement < (width+height)*4, "D3D9/D3D11 silhouette parity");
                if (pose == 0)
                {
                    HardwareTransformPatch_SSourceVertex nearerVertices[289];
                    memcpy(nearerVertices, vertices, sizeof(vertices));
                    for (auto& vertex : nearerVertices) vertex.kPosition.z += 300;
                    CTerrainPatch nearer;
                    nearer.BuildTerrainVertexBuffer(nearerVertices);
                    const auto overlap = [&](bool nearFirst)
                    {
                        Check(modern.BeginFrame(), "overlap begin");
                        modern.Clear({true,ClearColor{0,0,0,1}});
                        terrain.ResetFrame(); terrain.BeginTerrain(matrices,true);
                        terrain.DrawTerrain(nearFirst ? nearer.terrainGeometry : patch.terrainGeometry,ib,6,false);
                        terrain.DrawTerrain(nearFirst ? patch.terrainGeometry : nearer.terrainGeometry,ib,6,false);
                        modern.EndFrame();
                        Check(!terrain.Failed(), "overlap draws");
                        return BackendTestAccess::Read(modern,true);
                    };
                    const auto nearFirst = overlap(true), farFirst = overlap(false);
                    Check(nearFirst == farFirst, "depth independent of submission order");
                    const auto center = size_t(height/2)*width+width/2;
                    Check((nearFirst[center]&0xffffff) < (depth[center]&0xffffff), "near patch occludes far patch");
                    std::cout << "Overlapping patches / depth order: PASS\n";
                }
                std::cout << "pose=" << pose << " covered=" << covered << " edge-differences=" << disagreement << " draws=2 PASS\n";
                legacy.Present(); modern.Present();
            }
            patch.Clear(); Check(lifetime.expired(), "patch releases geometry");
            CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE = oldSoftware;
            terrainRenderer = nullptr;
        }
        modern.Shutdown();
        // Exercise the actual game presentation owner, not only its backend.
        // No window-style changes are needed in the production client for this test.
        {
            auto presentation = CreateTerrainPresentation(modernWindow,640,480);
            Check(presentation && terrainRenderer, "presentation binds terrain bridge");
            Check(presentation->BeginFrame() && presentation->Present(), "login frame without terrain");
            const float triangle[3][6]{{0,0,0,0,0,1},{0,-3200,0,0,0,1},{3200,0,0,0,0,1}};
            float vertices[289][6]{};
            memcpy(vertices,triangle,sizeof(triangle));
            const uint16_t indices[]{0,1,2};
            auto vb=terrainRenderer->UploadVertices(vertices,289,24);
            auto ib=terrainRenderer->UploadIndices(indices,3);
            Check(vb && ib,"presentation geometry");
            for (const auto size : {std::pair{640u,480u},std::pair{800u,600u},std::pair{320u,240u},std::pair{1024u,768u}})
            {
                Check(presentation->Resize(size.first,size.second),"presentation resize");
                screen.SetPositionCamera(1600,-1600,0,5000,45,0);
                screen.SetPerspective(30,float(size.first)/size.second,100,25600);
                Check(presentation->BeginFrame(),"presentation begin");
                terrainRenderer->BeginTerrain(screen.Matrices(),true);
                terrainRenderer->DrawTerrain(vb,ib,3,false);
                Check(presentation->Present(),"presentation terrain present");
            }
            Check(presentation->Resize(0,0),"presentation minimize");
            Check(presentation->Resize(640,480),"presentation restore");
            Check(presentation->BeginFrame() && presentation->Present(),"terrain to login transition");
            vb.reset(); ib.reset(); // Map handles must die before the device owner.
            presentation.reset();
            Check(!terrainRenderer,"presentation unbinds terrain bridge");
            std::cout << "Presentation resize / suspend / resume / shutdown: PASS\n";
        }
        legacy.Shutdown();
    }
    catch(const std::exception& e) { terrainRenderer=nullptr; std::cerr << e.what() << '\n'; result=1; }
    DestroyWindow(modernWindow); DestroyWindow(legacyWindow); UnregisterClassW(wc.lpszClassName,wc.hInstance);
    return result;
}
