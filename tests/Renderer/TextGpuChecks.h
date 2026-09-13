#pragma once
// ZiiNAN: Diligent text rendering integration; real original FT/atlas/batch draws are the oracle.
#include "Renderer/DiligentTextRenderer.h"
#include "EterLib/FontManager.h"
#include "EterLib/GrpTextInstance.h"
#include "EterLib/IME.h"

static void TextGpuChecks(LegacyProbe& screen,Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer;
    CPackManager packs;
    Check(CFontManager::Instance().Initialize(),"existing FreeType initialize");
    DiligentTextRenderer text(modern); DiligentUIRenderer ui(modern);
    Check(text.Initialize() && ui.Initialize(),"text/UI renderers");
    textRenderer=&text; uiRenderer=&ui;
    struct Font : CGraphicText {
        using CGraphicText::CGraphicText;
        void OnConstruct() override {} void OnSelfDestruct() override {}
    } font("Arial:18.fnt"),italic("Arial:23i.fnt"),bold("Arial Bold:25.fnt"),large("Arial:48.fnt");
    font.Load(); italic.Load(); bold.Load(); large.Load();
    Check(!font.IsEmpty() && !italic.IsEmpty() && !bold.IsEmpty() && !large.IsEmpty(),"native font names/size/styles");
    auto* device=STATEMANAGER.GetDevice();
    const auto state=[&](D3DRENDERSTATETYPE t,DWORD v) {
        STATEMANAGER.SetRenderState(t,v);
        Check(SUCCEEDED(device->SetRenderState(t,v)),"explicit native text test state");
    };
    const auto stage=[&](D3DTEXTURESTAGESTATETYPE t,DWORD v) { STATEMANAGER.SetTextureStageState(0,t,v); };
    for(unsigned test=0;test<28;++test) {
        unsigned width=640,height=480;
        if(test>=24) { width=std::array<unsigned,4>{800,1024,1920,640}[test-24]; height=std::array<unsigned,4>{600,768,1080,480}[test-24]; }
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"text live-atlas resize");
        if(test>=24) Check(modern.Resize(0,0) && !modern.BeginFrame() && modern.Resize(width,height),"text minimize/restore");
        STATEMANAGER.SetDefaultState(); STATEMANAGER.Restore();
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
        device->SetTexture(0,nullptr); device->SetTexture(1,nullptr);
        D3DVIEWPORT9 full={0,0,width,height,0,1}; Check(SUCCEEDED(device->SetViewport(&full)),"text full viewport baseline");
        state(D3DRS_LIGHTING,FALSE); state(D3DRS_FOGENABLE,FALSE);
        state(D3DRS_ZENABLE,FALSE); state(D3DRS_ZWRITEENABLE,FALSE); state(D3DRS_CULLMODE,D3DCULL_NONE);
        state(D3DRS_ALPHABLENDENABLE,TRUE); state(D3DRS_SRCBLEND,D3DBLEND_SRCALPHA); state(D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA);
        state(D3DRS_BLENDOP,D3DBLENDOP_ADD); state(D3DRS_ALPHATESTENABLE,FALSE);
        state(D3DRS_COLORWRITEENABLE,15); state(D3DRS_SCISSORTESTENABLE,FALSE);
        stage(D3DTSS_COLOROP,D3DTOP_MODULATE); stage(D3DTSS_COLORARG1,D3DTA_TEXTURE); stage(D3DTSS_COLORARG2,D3DTA_DIFFUSE);
        stage(D3DTSS_ALPHAOP,D3DTOP_MODULATE); stage(D3DTSS_ALPHAARG1,D3DTA_TEXTURE); stage(D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
        stage(D3DTSS_TEXCOORDINDEX,0); stage(D3DTSS_TEXTURETRANSFORMFLAGS,0);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        for(auto f:{D3DSAMP_MINFILTER,D3DSAMP_MAGFILTER}) STATEMANAGER.SetSamplerState(0,f,test==7 ? D3DTEXF_LINEAR : D3DTEXF_POINT);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP); STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSV,D3DTADDRESS_CLAMP);
        D3DXMATRIX identity; D3DXMatrixIdentity(&identity);
        STATEMANAGER.SetTransform(D3DTS_WORLD,&identity); STATEMANAGER.SetTransform(D3DTS_VIEW,&identity);
        screen.SetOrtho2D(float(width),float(height),1000);
        Check(legacy.BeginFrame() && modern.BeginFrame(),"text frame");
        legacy.Clear({true,ClearColor{.13f,.25f,.39f,.4f}}); modern.Clear({true,ClearColor{.13f,.25f,.39f,.4f}});
        text.ResetFrame(); ui.ResetFrame(); uiFrame=uiMode=true;
        CGraphicTextInstance original;
        original.SetColor(0xffffffff); // Native widgets initialize this explicitly as well.
        original.SetTextPointer(test==8 ? &italic : test==9 ? &bold : test==18 ? &large : &font);
        original.SetPosition(30,45);
        std::string value="Login: Metin2 0123456789 AV ffi";
        if(test==10) value="\xc3\x84\xc3\x96\xc3\x9c \xc3\xa4\xc3\xb6\xc3\xbc \xc3\x9f \xc3\xa9 \xe2\x82\xac";
        if(test==11) value="|cffff8030Red|r normal |cff4080ffBlue|r";
        if(test==12) value="This existing text wraps across several lines without a new layout engine.";
        if(test==16) value="";
        if(test==17) value="     ";
        if(test==18) {
            std::wstring wide; for(wchar_t c=33;c<1000;++c) wide+=c;
            const int size=WideCharToMultiByte(CP_UTF8,0,wide.data(),int(wide.size()),nullptr,0,nullptr,nullptr);
            value.resize(size); WideCharToMultiByte(CP_UTF8,0,wide.data(),int(wide.size()),value.data(),size,nullptr,nullptr);
            original.SetMultiLine(true); original.SetLimitWidth(580);
        }
        if(test==1 || test==2 || test==3 || test>=24) original.ShowOutLine();
        if(test==2) original.SetOutLineColor(0xffd03070);
        if(test==3) { original.SetColor(0x304080f0); original.SetOutLineColor(0x1030c050); }
        if(test==4) { original.SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER); original.SetPosition(300,120); }
        if(test==5) { original.SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_RIGHT); original.SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM); original.SetPosition(400,130); }
        if(test==6 || test==21) {
            state(D3DRS_SCISSORTESTENABLE,TRUE); RECT r{65,49,240,62}; STATEMANAGER.SetScissorRect(r);
        }
        if(test==7) original.SetPosition(30.35f,45.7f);
        if(test==12) { original.SetMultiLine(true); original.SetLimitWidth(125); }
        if(test==13) original.SetSecret(true);
        if(test==14 || test==16 || test==21) { CIME::Clear(); original.ShowCursor(); }
        if(test==15 || test==21) original.SetSelection(2,9);
        if(test==19) { state(D3DRS_ALPHATESTENABLE,TRUE); state(D3DRS_ALPHAREF,90); state(D3DRS_ALPHAFUNC,D3DCMP_GREATER); }
        if(test==20) { D3DVIEWPORT9 v={15,21,width-30,height-42,0,1}; Check(SUCCEEDED(device->SetViewport(&v)),"text subviewport"); }
        original.SetValue(value.c_str()); original.Update();
        if(test==18) Check(large.GetFontTexturePointer()->CheckTextureIndex(1),"real font multiple atlas pages");
        if(test==22) {
            CGraphicTextInstance chat; chat.SetColor(0xffffffff); chat.SetTextPointer(&font); chat.SetPosition(40,85);
            chat.SetChatValue("Tester","|cff20ff70Chat|r system text"); chat.Update(); chat.Render();
            screen.SetDiffuseColor(0x8030c070); screen.RenderBar2d(20,100,240,140);
        }
        RECT coarse{0,50,300,200}; original.Render(test==23 ? &coarse : nullptr);
        if(test==23) { original.SetPosition(30,80); original.Render(&coarse); }
        if(test==22) { original.SetPosition(35,110); original.Render(); }
        Check(!text.Failed() && !ui.Failed() && text.DrawCount()>0,"original text hooks submit");
        DWORD mask=0; device->GetRenderState(D3DRS_COLORWRITEENABLE,&mask); Check(mask==15,"native text mask restore");
        legacy.EndFrame(); modern.EndFrame(); uiFrame=uiMode=false;
        const auto a=LegacyProbe::Read(width,height),b=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
        uint64_t errors=0,depthErrors=0,alphaErrors=0,ink=0; double difference=0;
        for(size_t i=0;i<a.size();++i) {
            for(unsigned c=0;c<3;++c) { const int delta=std::abs(int((a[i]>>((2-c)*8))&255)-int((b[i]>>(c*8))&255)); difference+=delta; if(delta>4) ++errors; }
            if((depth[i]&0xffffff)!=0xffffff) ++depthErrors;
            if(test!=22 && (b[i]>>24)!=102) ++alphaErrors;
            if((a[i]&0xffffff)!=(a[0]&0xffffff)) ++ink;
        }
        std::cout<<"Text native case="<<test<<" size="<<width<<'x'<<height<<" mean="<<difference/(a.size()*3)
            <<" channel-errors="<<errors<<" depth-errors="<<depthErrors<<" alpha-errors="<<alphaErrors<<" ink="<<ink<<'\n';
        SaveSplatReadback(a,width,height,false,"text-case"+std::to_string(test)+"-d3d9");
        SaveSplatReadback(b,width,height,true,"text-case"+std::to_string(test)+"-d3d11");
        Check(!errors && !depthErrors && !alphaErrors,"native glyph/outline/alpha/cursor/clipping parity");
        if(test!=17) Check(ink>0,"real visible text/caret pixels");
        if(test==16) { int w=0,h=0; original.GetTextSize(&w,&h);
            Check(text.DrawCount()==1,"empty editline forwards native caret draw");
            std::cout<<"Empty native editline size="<<w<<'x'<<h<<" caret-pixels="<<ink<<'\n'; }
    }
    // Existing font page updates replace only GPU output, not glyph addresses or UV/metrics.
    auto* atlas=font.GetFontTexturePointer(); auto* glyph=atlas->GetCharacterInfomation(L'A');
    atlas->SelectTexture(glyph->index);
    std::weak_ptr<TerrainTexture> old=atlas->GetTextTexture(atlas->GetD3DTexture());
    atlas->GetCharacterInfomation(L'\u03a9'); atlas->UpdateTexture(); text.ResetFrame();
    Check(glyph==atlas->GetCharacterInfomation(L'A') && old.expired(),"atlas update preserves existing glyph cache and releases replaced handle");
    font.Clear(); italic.Clear(); bold.Clear(); large.Clear(); text.ResetFrame();
    Check(!text.LiveTextureCount(),"font owners release all text pages");
    textRenderer=nullptr; uiRenderer=nullptr; text.Shutdown(); ui.Shutdown();
    Check(!text.LiveBufferCount() && !text.LiveTextureCount() && !text.Failed(),"text shutdown zero");
    CFontManager::Instance().Destroy();
    std::cout<<"Text real FreeType/atlas/LCD/outline/caret/UI/resize/lifetime: PASS\n";
}
