#pragma once
// ZiiNAN: Exercise the original flying-trail simulation/geometry and native material bridge.
#include <deque>
#include "GameLib/FlyingData.h"
#include "GameLib/FlyTrace.h"
#include "EffectLib/EffectRenderBridge.h"

static void EffectRuntimeGpuChecks(LegacyProbe& screen,Renderer::LegacyD3D9Backend& legacy,Renderer::DiligentD3D11Backend& modern)
{
    using namespace Renderer;
    DiligentEffectRenderer effects(modern); Check(effects.Initialize(),"native trail renderer");
    effectRenderer=&effects; effectWorldFrame=true;
    CTimer::Instance().UseCustomTime();
    Check(legacy.Resize(320,240) && modern.Resize(320,240),"native trail resize");
    for(unsigned test=0;test<4;++test) {
        STATEMANAGER.SetDefaultState(); STATEMANAGER.Restore();
        // Earlier material oracles intentionally bypass the cache; synchronize the test boundary.
        auto* device=STATEMANAGER.GetDevice();
        device->SetTexture(0,nullptr); device->SetTexture(1,nullptr);
        device->SetVertexShader(nullptr); device->SetPixelShader(nullptr);
        device->SetFVF(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1);
        STATEMANAGER.SetRenderState(D3DRS_FOGENABLE,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_ZWRITEENABLE,test&1);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
        screen.SetPositionCamera(0,0,0,800,35,0); screen.SetPerspective(30,320.0f/240,1,3000); screen.BuildViewFrustum();
        CFlyingData::TFlyingAttachData data{};
        data.dwTailColor=0xa0ff9955; data.fTailLength=.8f; data.fTailSize=25; data.bRectShape=(test&2)!=0;
        CFlyTrace trail; trail.Create(data);
        for(float x:{-100.0f,0.0f,100.0f}) { CTimer::Instance().Advance(); trail.UpdateNewPosition({x,0,30}); }
        Check(legacy.BeginFrame() && modern.BeginFrame(),"native trail frame");
        legacy.Clear({true,ClearColor{.1f,.2f,.3f,1}}); modern.Clear({true,ClearColor{.1f,.2f,.3f,1}});
        ++effectFrameSerial; effects.ResetFrame(); trail.Render();
        Check(!effects.Failed() && effects.DrawCount(EffectPart::FlyTrace)==2 && effects.Vertices()==12,"native trail bridge submission");
        legacy.EndFrame(); modern.EndFrame();
        const auto a=LegacyProbe::Read(320,240),b=BackendTestAccess::Read(modern,false);
        size_t bad=0,visible=0;
        for(size_t i=0;i<a.size();++i) {
            if((b[i]&255)>40) ++visible;
            for(unsigned c=0;c<3;++c) if(std::abs(int((a[i]>>((2-c)*8))&255)-int((b[i]>>(c*8))&255))>4) ++bad;
        }
        std::cout<<"Native fly trace rect="<<data.bRectShape<<" inherited-depth-write="<<(test&1)<<" visible="<<visible<<" channel-errors="<<bad<<'\n';
        Check(visible>100 && bad<40,"native flying trace image parity");
        legacy.Present(); modern.Present();
        for(unsigned tick=0;tick<80;++tick) CTimer::Instance().Advance();
        trail.UpdateNewPosition({200,0,30}); effects.ResetFrame(); trail.Render();
        Check(effects.DrawCount(EffectPart::FlyTrace)==0,"original tail lifetime expires");
        trail.Destroy();
    }
    effectWorldFrame=false; effectRenderer=nullptr; effects.ResetFrame(); effects.Shutdown();
    Check(!effects.Failed() && effects.LiveTextureCount()==0 && effects.LiveBufferCount()==0,"native trail shutdown zero");
    std::cout<<"Native flying trail geometry / sorting / bridge / lifetime / shutdown: PASS\n";
}
