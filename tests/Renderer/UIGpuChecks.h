#pragma once
// ZiiNAN: Original D3D9 UI geometry and original Image/SubImage ownership are the oracle.
#include "Renderer/DiligentUIRenderer.h"
#include "EterLib/UIRenderBridge.h"
#include "EterLib/GrpImageInstance.h"
#include "EterLib/GrpExpandedImageInstance.h"
#include "EterLib/GrpSubImage.h"
#include "PackLib/PackManager.h"

static void UIGpuChecks(LegacyProbe& screen,Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer;
    CPackManager packs;
    struct File {
        char name[MAX_PATH]{};
        File(const std::vector<uint8_t>& bytes) {
            char folder[MAX_PATH]{}; Check(GetTempPathA(MAX_PATH,folder)!=0,"UI temp directory");
            Check(GetTempFileNameA(folder,"m9u",0,name)!=0,"UI unique fixture file");
            std::ofstream out(name,std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
            Check(bool(out),"UI fixture bytes");
        }
        ~File() { if(*name) DeleteFileA(name); }
    } file(TerrainFixture::AlphaDDS());
    // Stack fixtures suppress only ResourceManager's delayed deletion, not Load/Clear or the UI cache.
    struct Image : CGraphicImage { using CGraphicImage::CGraphicImage; void OnSelfDestruct() override {} } image(file.name);
    struct Sub : CGraphicSubImage {
        using CGraphicSubImage::CGraphicSubImage;
        using CGraphicSubImage::SetImagePointer;
        void OnConstruct() override {} void OnSelfDestruct() override {}
    } sub("UI-sub-fixture");
    image.Load(); Check(!image.IsEmpty(),"native UI original image load");
    sub.SetImagePointer(&image); sub.SetRectPosition(4,3,25,27);
    DiligentUIRenderer ui(modern); Check(ui.Initialize(),"UI renderer initialization");
    uiRenderer=&ui;
    auto* device=STATEMANAGER.GetDevice();
    const auto state=[&](D3DRENDERSTATETYPE type,DWORD value) { STATEMANAGER.SetRenderState(type,value); };
    const auto stage=[&](D3DTEXTURESTAGESTATETYPE type,DWORD value) { STATEMANAGER.SetTextureStageState(0,type,value); };
    for(unsigned test=0;test<32;++test) {
        const unsigned width=test>=28 ? std::array<unsigned,4>{800,1024,1920,1280}[test-28] : 320;
        const unsigned height=test>=28 ? std::array<unsigned,4>{600,768,1080,720}[test-28] : 240;
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"UI resize");
        if(test%8==0) Check(modern.Resize(0,0) && !modern.BeginFrame() && modern.Resize(width,height),"UI suspend restore");
        STATEMANAGER.SetDefaultState(); STATEMANAGER.Restore();
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
        device->SetTexture(0,nullptr); device->SetTexture(1,nullptr);
        state(D3DRS_LIGHTING,FALSE); state(D3DRS_FOGENABLE,FALSE);
        state(D3DRS_ZENABLE,FALSE); state(D3DRS_ZWRITEENABLE,FALSE); state(D3DRS_CULLMODE,D3DCULL_NONE);
        state(D3DRS_ALPHABLENDENABLE,TRUE); state(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); state(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        state(D3DRS_BLENDOP,D3DBLENDOP_ADD); state(D3DRS_ALPHATESTENABLE,FALSE); state(D3DRS_MULTISAMPLEANTIALIAS,FALSE);
        state(D3DRS_SCISSORTESTENABLE,FALSE);
        stage(D3DTSS_COLOROP,D3DTOP_MODULATE); stage(D3DTSS_COLORARG1,D3DTA_TEXTURE); stage(D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        stage(D3DTSS_ALPHAOP,D3DTOP_MODULATE); stage(D3DTSS_ALPHAARG1,D3DTA_TEXTURE); stage(D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        stage(D3DTSS_TEXCOORDINDEX,0); stage(D3DTSS_TEXTURETRANSFORMFLAGS,0);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        for(auto filter:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER}) STATEMANAGER.SetSamplerState(0,filter,test==9 ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP); STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        D3DXMATRIX identity; D3DXMatrixIdentity(&identity);
        STATEMANAGER.SetTransform(D3DTS_WORLD,&identity); STATEMANAGER.SetTransform(D3DTS_VIEW,&identity);
        screen.SetOrtho2D(float(width),float(height),1000);
        Check(legacy.BeginFrame() && modern.BeginFrame(),"UI frame");
        legacy.Clear({true,ClearColor{.1f,.2f,.3f,1}}); modern.Clear({true,ClearColor{.1f,.2f,.3f,1}});
        ui.ResetFrame(); uiFrame=uiMode=true;
        if(test==21) { D3DVIEWPORT9 viewport={31,23,width-62,height-46,0,1}; Check(SUCCEEDED(device->SetViewport(&viewport)),"UI native subviewport"); }
        if(test>=10 && test<=14) {
            state(D3DRS_SCISSORTESTENABLE,TRUE); RECT rect{39,28,test==14 ? 40 : 110,83}; STATEMANAGER.SetScissorRect(rect);
        }
        if(test==8) { state(D3DRS_ALPHATESTENABLE,TRUE); state(D3DRS_ALPHAFUNC,D3DCMP_GREATER); state(D3DRS_ALPHAREF,127); }
        if(test==0) {
            CGraphicImageInstance original; original.SetImagePointer(&image); original.SetPosition(25,19); original.Render();
        } else if(test==16) {
            screen.RenderGradationBar2d(13,17,159,126,0xa0ff8833,0x504477ff);
        } else if(test==17) {
            screen.SetDiffuseColor(0xffffffff); screen.RenderBox2d(10,10,100,75);
        } else if(test==18 || test==19) {
            screen.SetDiffuseColor(0xffffffff); screen.RenderLine2d(10,10,test==18 ? 123 : 10,120);
            screen.RenderLine2d(20,25,167,25);
        } else if(test==20) {
            const EffectVertex fan[]={{{100,100,0},0x80000000},{{100,40,0},0x80000000},{{40,40,0},0x80000000},
                {{40,100,0},0x80000000},{{40,160,0},0x80000000},{{100,160,0},0x80000000}};
            stage(D3DTSS_COLOROP,D3DTOP_SELECTARG1); stage(D3DTSS_COLORARG1,D3DTA_DIFFUSE);
            stage(D3DTSS_ALPHAOP,D3DTOP_SELECTARG1); stage(D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
            STATEMANAGER.SetTexture(0,nullptr); STATEMANAGER.SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
            const auto result=STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLEFAN,4,fan,24);
            UIRenderBridge::Submit(fan,6,UIRenderBridge::Primitive::Fan,nullptr,result);
        } else {
            CGraphicExpandedImageInstance original; original.SetImagePointer(test==15 ? static_cast<CGraphicImage*>(&sub) : &image);
            original.SetPosition(test==3 ? 150 : 25,19); original.SetScale(test==3 ? -3 : 4,3);
            if(test==2) { original.SetOrigin(70,70); original.SetRotation(28); }
            if(test==4) original.SetDiffuseColor(.4f,.7f,1,.45f);
            if(test==5) original.SetRenderingRect(.1f,.2f,-.3f,-.25f);
            if(test==6) original.SetRenderingMode(CGraphicExpandedImageInstance::RENDERING_MODE_SCREEN);
            if(test==7) original.SetRenderingMode(CGraphicExpandedImageInstance::RENDERING_MODE_MODULATE);
            original.Render();
            if(test>=22) {
                state(D3DRS_SCISSORTESTENABLE,TRUE); RECT clipped{50,45,100,95}; STATEMANAGER.SetScissorRect(clipped);
                screen.SetDiffuseColor(0x8080ff30); screen.RenderBar2d(30,30,140,130);
                state(D3DRS_SCISSORTESTENABLE,FALSE);
                original.SetPosition(float(width-140),float(height-110)); original.SetDiffuseColor(.8f,1,.6f,.75f); original.Render();
            }
        }
        Check(!ui.Failed() && ui.DrawCount()>0,"native UI hooks submit");
        legacy.EndFrame(); modern.EndFrame(); uiFrame=uiMode=false;
        const auto native=LegacyProbe::Read(width,height),diligent=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
        uint64_t errors=0,depthErrors=0; double difference=0;
        for(size_t i=0;i<native.size();++i) {
            for(unsigned c=0;c<3;++c) { const int delta=std::abs(int((native[i]>>((2-c)*8))&255)-int((diligent[i]>>(c*8))&255)); difference+=delta; if(delta>4) ++errors; }
            if((depth[i]&0xffffff)!=0xffffff) ++depthErrors;
        }
        std::cout<<"UI native case="<<test<<" size="<<width<<'x'<<height<<" mean="<<difference/(native.size()*3)
                 <<" channel-errors="<<errors<<" depth-errors="<<depthErrors<<'\n';
        if(test<22 || test>=28) {
            SaveSplatReadback(native,width,height,false,"ui-case"+std::to_string(test)+"-d3d9");
            SaveSplatReadback(diligent,width,height,true,"ui-case"+std::to_string(test)+"-d3d11");
        }
        Check(!errors && !depthErrors,"UI pixel alignment/blend/scissor/depth parity");
    }
    Check(image.GetUITexture(ui)==sub.GetUITexture(ui) && ui.LiveTextureCount()==1,"shared original subimage atlas");
    auto weak=std::weak_ptr<TerrainTexture>(image.GetUITexture(ui));
    sub.Clear(); image.Clear(); ui.ResetFrame();
    Check(weak.expired() && ui.LiveTextureCount()==0,"original UI image clear releases atlas handle");
    uiRenderer=nullptr; ui.Shutdown();
    Check(!ui.LiveTextureCount() && !ui.LiveBufferCount() && !ui.Failed(),"UI shutdown resources zero");
    std::cout<<"UI native primitives / original image owners / viewport / scissor / resize / shutdown: PASS\n";
}
