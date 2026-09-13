#pragma once
// ZiiNAN: Diligent floating text rendering; real .mse/DDS/particle simulation, no animation substitute.
#include "EffectLib/EffectManager.h"
#include "EterLib/ResourceManager.h"
#include <filesystem>

static void DamageRuntimeGpuChecks(LegacyProbe& screen,Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer;
    CPackManager packs; CResourceManager resources; CEffectManager manager;
    resources.RegisterResourceNewFunctionPointer("dds",[](const char* name)->CResource* { return new CGraphicImage(name); });
    DiligentEffectRenderer effects(modern); Check(effects.Initialize(),"damage existing effect binder");
    struct Capture : IEffectRenderer {
        DiligentEffectRenderer& renderer;
        float position=0,alpha=0; uint32_t draws=0;
        explicit Capture(DiligentEffectRenderer& r):renderer(r) {}
        TerrainTexturePtr UploadTexture(const TerrainTextureData& d) override { return renderer.UploadTexture(d); }
        void ReportFailure() override { renderer.ReportFailure(); }
        void Draw(const EffectVertex* v,uint32_t n,const TerrainTexturePtr& t,const EffectDraw& d,EffectPart p) override {
            ++draws; position=v[0].position[0]+v[0].position[2]; alpha=d.factor[3]; renderer.Draw(v,n,t,d,p);
        }
    } capture(effects);
    effectRenderer=&capture; effectWorldFrame=true;
    CTimer::Instance().UseCustomTime();
    const auto assetRoot=std::filesystem::path(__FILE__).parent_path().parent_path().parent_path().parent_path()/
        "m2dev-client/assets/Effect/ymir work/effect/affect/damagevalue";
    auto* device=STATEMANAGER.GetDevice();
    Check(legacy.Resize(400,300) && modern.Resize(400,300),"damage resize");
    unsigned caseIndex=0;
    // Repeat the same assets after expiry: pooled effect owners and weak texture entries must be reusable.
    for(const char* assetName:{"target","damage","damage_1","miss","target_miss","target","target","damage","damage_1"}) {
        const std::string path=(assetRoot/(std::string(assetName)+".mse")).generic_string();
        Check(std::filesystem::exists(path),"original damage .mse asset exists");
        DWORD crc=0; Check(manager.RegisterEffect2(path.c_str(),&crc,false),"original damage script load");
        std::vector<int> ids;
        const bool miss=std::string(assetName).find("miss")!=std::string::npos;
        for(unsigned digit=0;digit<(miss ? 1u : 3u);++digit) {
            if(!miss) {
                const char* prefix=std::string(assetName)=="target" ? "target_" : "damage_";
                const std::string file=(assetRoot/(std::string(prefix)+std::to_string(std::array<unsigned,3>{1,2,9}[digit])+".dds")).generic_string();
                manager.SetEffectTextures(crc,{file});
            }
            ids.push_back(manager.CreateEffect(crc,{-30.0f*digit,0,25},{0,0,0}));
        }
        std::vector<float> positions,alphas; size_t visibleFrames=0;
        for(unsigned tick=0;tick<80;++tick) {
            CTimer::Instance().Advance(); manager.Update();
            if(tick<40 ? tick%2!=0 : tick%10!=0) continue;
            STATEMANAGER.SetDefaultState(); STATEMANAGER.Restore();
            device->SetVertexShader(nullptr); device->SetPixelShader(nullptr); device->SetTexture(0,nullptr); device->SetTexture(1,nullptr);
            const auto state=[&](D3DRENDERSTATETYPE t,DWORD v) { STATEMANAGER.SetRenderState(t,v); device->SetRenderState(t,v); };
            for(auto t:{D3DRS_LIGHTING,D3DRS_FOGENABLE,D3DRS_SCISSORTESTENABLE,D3DRS_STENCILENABLE,D3DRS_SEPARATEALPHABLENDENABLE}) state(t,FALSE);
            state(D3DRS_COLORWRITEENABLE,15); state(D3DRS_FILLMODE,D3DFILL_SOLID); state(D3DRS_BLENDOP,D3DBLENDOP_ADD);
            state(D3DRS_ZENABLE,TRUE); state(D3DRS_ZWRITEENABLE,TRUE); state(D3DRS_ZFUNC,D3DCMP_LESSEQUAL);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
            STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,0);
            STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
            STATEMANAGER.SetSamplerState(0,D3DSAMP_MIPFILTER,D3DTEXF_NONE);
            screen.SetPositionCamera(0,0,200,2000,35,0); screen.SetPerspective(30,400.f/300,1,6000); screen.BuildViewFrustum();
            D3DVIEWPORT9 viewport={0,0,400,300,0,1}; device->SetViewport(&viewport);
            Check(legacy.BeginFrame() && modern.BeginFrame(),"damage frame");
            legacy.Clear({true,ClearColor{.1f,.2f,.3f,1}}); modern.Clear({true,ClearColor{.1f,.2f,.3f,1}});
            ++effectFrameSerial; effects.ResetFrame(); capture.draws=0; manager.Render();
            Check(!effects.Failed(),"damage native binding");
            if(capture.draws) { positions.push_back(capture.position); alphas.push_back(capture.alpha); }
            legacy.EndFrame(); modern.EndFrame();
            const auto a=LegacyProbe::Read(400,300),b=BackendTestAccess::Read(modern,false),z=BackendTestAccess::Read(modern,true);
            size_t errors=0,ink=0,depthErrors=0;
            for(size_t i=0;i<a.size();++i) {
                for(unsigned c=0;c<3;++c) if(std::abs(int((a[i]>>((2-c)*8))&255)-int((b[i]>>(c*8))&255))>4) ++errors;
                if((a[i]&0xffffff)!=(a[0]&0xffffff)) ++ink;
                if((z[i]&0xffffff)!=0xffffff) ++depthErrors;
            }
            if(ink) ++visibleFrames;
            std::cout<<"Damage native asset="<<assetName<<" tick="<<tick<<" draws="<<capture.draws<<" ink="<<ink
                <<" channel-errors="<<errors<<" depth-errors="<<depthErrors<<" alpha="<<capture.alpha<<'\n';
            Check(!errors && !depthErrors,"original damage pixels and no depth write");
            if(tick==12) {
                SaveSplatReadback(a,400,300,false,"damage-"+std::string(assetName)+"-d3d9");
                SaveSplatReadback(b,400,300,true,"damage-"+std::string(assetName)+"-d3d11");
            }
            ++caseIndex; legacy.Present(); modern.Present();
        }
        Check(visibleFrames>=2 && positions.size()>=2,"damage visibly animated");
        Check(*std::max_element(positions.begin(),positions.end())-*std::min_element(positions.begin(),positions.end())>1,"native movement/scale changes");
        Check(*std::max_element(alphas.begin(),alphas.end())-*std::min_element(alphas.begin(),alphas.end())>.05f,"native damage fade changes");
        for(int id:ids) Check(!manager.IsAliveEffect(id),"native damage expires");
        effects.ResetFrame(); Check(!effects.LiveTextureCount(),"expired damage owners release SRVs");
    }
    manager.Destroy(); resources.DestroyDeletingList(); resources.Destroy();
    effectWorldFrame=false; effectRenderer=nullptr; effects.ResetFrame(); effects.Shutdown();
    Check(!effectRuntime.instances && !effectRuntime.systems && !effectRuntime.particles && !effects.LiveTextureCount() && !effects.LiveBufferCount(),"damage lifetime shutdown zero");
    std::cout<<"Damage real scripts/textures/overrides/animation/fade/expiry: PASS cases="<<caseIndex<<'\n';
}
