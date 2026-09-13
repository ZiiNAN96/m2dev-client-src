#pragma once
// ZiiNAN: Diligent floating text rendering; native TextTail projection/layout/draws are the oracle.
#include "EterPythonLib/PythonGraphic.h"
#include "EterLib/GrpMarkInstance.h"
#include "EterLib/ResourceManager.h"
#include "GameLib/MapType.h"
#include "UserInterface/PythonTextTail.h"

extern CGraphicText* ms_pFont;
namespace FloatingOracle
{
struct Owner : CGraphicObjectInstance {
    int GetType() const override { return ACTOR_OBJECT; }
    bool GetBoundingSphere(D3DXVECTOR3& p,float& radius) override { p=GetPosition(); radius=100; return true; }
    void OnRender() override {} void OnBlendRender() override {} void OnRenderToShadowMap() override {}
    void OnRenderShadow() override {} void OnRenderPCBlocker() override {}
    void OnUpdateCollisionData(const CStaticCollisionDataVector*) override {}
    void OnUpdateHeighInstance(CAttributeInstance*) override {}
    bool OnGetObjectHeight(float,float,float*) override { return false; }
};
struct Tails : CPythonTextTail {
    using CPythonTextTail::UpdateTextTail;
    TTextTail* Character(DWORD vid,Owner& owner,const char* name,float height,DWORD color) {
        auto* t=RegisterTextTail(vid,name,&owner,height,D3DXCOLOR(color));
        t->pTextInstance->SetOutline(true);
        t->pTextInstance->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
        m_CharacterTextTailMap.emplace(vid,t); m_CharacterTextTailList.push_back(t); return t;
    }
    void Info(DWORD vid,Owner& owner,bool expired) {
        auto* t=RegisterTextTail(vid,"Floating chat / info",&owner,200,D3DXCOLOR(0xffffc8c8));
        t->pTextInstance->SetOutline(true); t->LivingTime=expired ? 0 : 0xffffffff;
        t->bNameFlag=false; m_ChatTailMap.emplace(vid,t);
    }
    void Guild(TTextTail* t,CGraphicImage& atlas) {
        t->pMarkInstance=CGraphicMarkInstance::New();
        t->pMarkInstance->SetImageFileName(atlas.GetFileName()); t->pMarkInstance->Load(); t->pMarkInstance->SetIndex(1);
        auto* name=t->pGuildNameTextInstance=CGraphicTextInstance::New();
        name->SetTextPointer(ms_pFont); name->SetOutline(true);
        name->SetHorizonalAlign(CGraphicTextInstance::HORIZONTAL_ALIGN_CENTER);
        name->SetVerticalAlign(CGraphicTextInstance::VERTICAL_ALIGN_BOTTOM);
        name->SetValue("Guild name"); name->SetColor(0xffefd3ff); name->Update();
    }
    void Project() { UpdateShowingTextTail(); ArrangeTextTail(); }
    size_t Visible() const { return m_CharacterTextTailList.size()+m_ItemTextTailList.size(); }
    size_t Entries() const { return m_CharacterTextTailMap.size()+m_ItemTextTailMap.size()+m_ChatTailMap.size(); }
};
}
static void FloatingTextGpuChecks(LegacyProbe& screen,Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer; using namespace FloatingOracle;
    struct Graphic : CPythonGraphic {
        void Dimensions(unsigned w,unsigned h) { ms_iWidth=w; ms_iHeight=h; CGraphicBase::SetViewport(0,0,w,h,0,1); }
    };
    CPackManager packs; CResourceManager resources; Graphic graphic;
    resources.RegisterResourceNewFunctionPointer("tmp",[](const char* name)->CResource* { return new CGraphicImage(name); });
    Check(CFontManager::Instance().Initialize(),"floating native font runtime");
    DiligentTextRenderer text(modern); DiligentUIRenderer ui(modern);
    Check(text.Initialize() && ui.Initialize(),"floating existing binders");
    textRenderer=&text; uiRenderer=&ui;
    struct Font : CGraphicText {
        using CGraphicText::CGraphicText; void OnConstruct() override {} void OnSelfDestruct() override {}
    } font("Arial:18.fnt");
    font.Load(); Check(!font.IsEmpty(),"floating native font"); ms_pFont=&font;
    struct MarkFile {
        char name[MAX_PATH]{};
        MarkFile() {
            char folder[MAX_PATH]{}; Check(GetTempPathA(MAX_PATH,folder)!=0,"guild temp directory");
            Check(GetTempFileNameA(folder,"m10",0,name)!=0,"unique guild fixture file");
            const auto bytes=TerrainFixture::AlphaDDS();
            std::ofstream out(name,std::ios::binary); out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
            Check(bool(out),"guild atlas fixture bytes");
        }
        ~MarkFile() { if(*name) DeleteFileA(name); }
    } markFile;
    auto* atlasPointer=static_cast<CGraphicImage*>(resources.GetResourcePointer(markFile.name));
    Check(atlasPointer!=nullptr,"native guild atlas resource"); auto& atlas=*atlasPointer;
    atlas.Load(); Check(!atlas.IsEmpty(),"native guild atlas load");
    Owner owner; owner.SetPosition(0,0,0); owner.Show();
    Tails tails;
    // These checks also run without a live Actor; stale visible-list entries must never survive deletion.
    tails.RegisterItemTextTail(1,"Delete item",&owner); tails.ShowItemTextTail(1);
    tails.DeleteItemTextTail(1);
    Check(!tails.Visible() && !tails.Entries() && !font.GetReferenceCount(),"deleted tail leaves no visible pointer or font owner");
    tails.Character(2,owner,"Clear player",180,0xffffffff); tails.Info(2,owner,false);
    tails.Clear();
    Check(!tails.Visible() && !tails.Entries() && !font.GetReferenceCount(),"tail clear releases entries and font owners");
    auto* device=STATEMANAGER.GetDevice();
    const auto state=[&](D3DRENDERSTATETYPE t,DWORD v) {
        STATEMANAGER.SetRenderState(t,v); Check(SUCCEEDED(device->SetRenderState(t,v)),"floating native state baseline");
    };
    for(unsigned test=0;test<16;++test) {
        const unsigned width=test<12 ? 800 : std::array<unsigned,4>{800,1024,1920,1280}[test-12];
        const unsigned height=test<12 ? 600 : std::array<unsigned,4>{600,768,1080,720}[test-12];
        Check(legacy.Resize(width,height) && modern.Resize(width,height),"floating resize");
        graphic.Dimensions(width,height);
        if(test>=12) Check(modern.Resize(0,0) && !modern.BeginFrame() && modern.Resize(width,height),"floating minimize restore");
        STATEMANAGER.SetDefaultState(); STATEMANAGER.Restore();
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr); device->SetTexture(0,nullptr); device->SetTexture(1,nullptr);
        D3DVIEWPORT9 viewport={0,0,width,height,0,1}; device->SetViewport(&viewport);
        for(auto t:{D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_ALPHATESTENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_STENCILENABLE,D3DRS_SEPARATEALPHABLENDENABLE}) state(t,FALSE);
        state(D3DRS_ZENABLE,TRUE); state(D3DRS_ZWRITEENABLE,TRUE); state(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
        state(D3DRS_CULLMODE,D3DCULL_NONE); state(D3DRS_COLORWRITEENABLE,15); state(D3DRS_BLENDOP,D3DBLENDOP_ADD);
        state(D3DRS_FILLMODE,D3DFILL_SOLID);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,0);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        graphic.SetPositionCamera(0,0,120,test==2 ? 3000 : 1600,35,test==3 ? 180 : float(test*9));
        graphic.SetPerspective(30,float(width)/height,100,10000); graphic.Identity(); graphic.BuildViewFrustum();
        owner.Show(); owner.SetPosition(test==4 ? 10000 : 0,test==3 ? -5000 : 0,0);
        auto* t=tails.Character(10,owner,"Player / NPC / Mob",test==5 ? 600 : test==6 ? 280 : 180,0xffe0ff70);
        t->fDistanceFromPlayer=test==1 || test==2 ? 1800 : 0;
        if(test==7 || test>=12) {
            tails.AttachTitle(10,"Friendly",D3DXCOLOR(0xff40a0ff)); tails.AttachLevel(10,"Lv 54",D3DXCOLOR(0xff98ff33));
            tails.Guild(t,atlas);
        }
        tails.Info(10,owner,test==10);
        if(test==9) owner.Hide();
        if(test==8 || test>=12) {
            for(unsigned i=0;i<3;++i) { tails.RegisterItemTextTail(20+i,"Ground item",&owner); tails.ShowItemTextTail(20+i); }
            tails.SetItemTextTailOwner(20,"Owner");
        }
        tails.Project();
        const float projectedX=t->x,projectedY=t->y;
        Check(std::isfinite(projectedX) && std::isfinite(projectedY),"native projection finite");
        Check(legacy.BeginFrame() && modern.BeginFrame(),"floating frame");
        legacy.Clear({true,ClearColor{.13f,.25f,.39f,.4f}}); modern.Clear({true,ClearColor{.13f,.25f,.39f,.4f}});
        text.ResetFrame(); ui.ResetFrame(); uiFrame=true; graphic.SetInterfaceRenderState();
        if(test==1) { FloatingTextScope occluder; screen.SetDiffuseColor(0xff204080); screen.RenderBar2d(0,0,float(width)/2,float(height),-300); }
        if(test==11) { state(D3DRS_SCISSORTESTENABLE,TRUE); RECT r{int(width/2),0,int(width),int(height)}; STATEMANAGER.SetScissorRect(r); }
        tails.Render();
        Check(!floatingTextDepth && !text.Failed() && !ui.Failed(),"floating scope and deterministic bind");
        if(test==7 || test>=12) Check(ui.DrawCount()>0 && atlas.GetReferenceCount()==1,"guild mark submitted through native tail renderer");
        Check(t->x==projectedX && t->y==projectedY,"renderer leaves original screen position unchanged");
        legacy.EndFrame(); modern.EndFrame(); uiFrame=uiMode=false;
        const auto a=LegacyProbe::Read(width,height),b=BackendTestAccess::Read(modern,false),depth=BackendTestAccess::Read(modern,true);
        size_t errors=0,ink=0,depthPixels=0;
        for(size_t i=0;i<a.size();++i) {
            for(unsigned c=0;c<3;++c) if(std::abs(int((a[i]>>((2-c)*8))&255)-int((b[i]>>(c*8))&255))>4) ++errors;
            if((a[i]&0xffffff)!=(a[0]&0xffffff)) ++ink;
            if((depth[i]&0xffffff)!=0xffffff) ++depthPixels;
        }
        std::cout<<"Floating native case="<<test<<" size="<<width<<'x'<<height<<" xy="<<projectedX<<','<<projectedY
            <<" z="<<t->z<<" channel-errors="<<errors<<" ink="<<ink<<" depth-pixels="<<depthPixels<<'\n';
        SaveSplatReadback(a,width,height,false,"floating-case"+std::to_string(test)+"-d3d9");
        SaveSplatReadback(b,width,height,true,"floating-case"+std::to_string(test)+"-d3d11");
        Check(!errors,"native floating position/color/outline/box/depth parity");
        if(test!=3 && test!=4) Check(ink>0 && depthPixels>0,"visible floating text keeps native depth");
        tails.Clear(); text.ResetFrame(); ui.ResetFrame();
        Check(!tails.Visible() && !tails.Entries() && !font.GetReferenceCount() && !atlas.GetReferenceCount(),"floating map reset releases all logical owners");
        legacy.Present(); modern.Present();
    }
    tails.Destroy(); tails.Destroy(); ms_pFont=nullptr; font.Clear(); atlas.Clear(); text.ResetFrame();
    resources.DestroyDeletingList(); resources.Destroy();
    textRenderer=nullptr; uiRenderer=nullptr; text.Shutdown(); ui.Shutdown();
    Check(!text.LiveTextureCount() && !text.LiveBufferCount() && !ui.LiveTextureCount() && !ui.LiveBufferCount(),"floating graphics shutdown zero");
    std::cout<<"Native floating projection / original TextTail / delete-clear / resize / depth / shutdown: PASS\n";
}
