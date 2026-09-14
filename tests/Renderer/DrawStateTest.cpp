// ZiiNAN: CPU draw descriptions, nested state isolation and resource owners.
#include "EterLib/StdAfx.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/SourceResourceAudit.h"
#include "EterLib/DrawStateView.h"
#include "EterLib/MaterialStateSnapshot.h"
#include "EterLib/Camera.h"
#include <iostream>
#include <stdexcept>
float CCamera::CAMERA_MAX_DISTANCE=2500.0f;
static void Check(bool condition,const char* error) { if(!condition) throw std::runtime_error(error); }
int main()
{
    HWND window=CreateWindowW(L"STATIC",L"CPU draw state isolation test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    try {
        CGraphicDevice graphics; CScreen screen;
        Check(window && graphics.Create({window},320,240)==CGraphicDevice::CREATE_OK,"device-free CPU context");
        Check(screen.Begin(),"CPU frame");
        DRAWSTATE.SetRenderState(Renderer::StateAlphaBlendEnable,FALSE);
        DRAWSTATE.SetRenderState(Renderer::StateCullMode,Renderer::CullNone);
        DRAWSTATE.SetRenderState(Renderer::StateColorWriteEnable,15);
        DRAWSTATE.SetTextureStageState(0,Renderer::StageTexCoordIndex,0);
        DRAWSTATE.SetTextureStageState(1,Renderer::StageColorOp,Renderer::TextureOpDisable);
        DRAWSTATE.SetSamplerState(0,Renderer::SamplerAddressU,Renderer::AddressClamp);
        DRAWSTATE.SetSamplerState(0,Renderer::SamplerMinFilter,Renderer::FilterNone);
        Math::Matrix matrix; Math::MatrixTranslation(&matrix,3,4,5);
        DRAWSTATE.SetTransform(Renderer::MatrixWorld,&matrix);
        Math::Viewport viewport{5,7,160,100,0,1}; DRAWSTATE.SetViewport(&viewport);
        RECT clip{1,2,100,80}; DRAWSTATE.SetScissorRect(clip);
        Renderer::LightValues light{}; light.Type=Renderer::LightPoint; light.Position.x=12; light.Range=100;
        DRAWSTATE.SetLight(1,&light); DRAWSTATE.LightEnable(1,TRUE);
        float constants[4]={3,4,5,6}; DRAWSTATE.SetVertexConstants(4,constants,1);
        auto texture=Renderer::TextureResource::Dynamic(4,4,Renderer::TerrainTextureFormat::BGRA8);
        std::weak_ptr<Renderer::TextureResource> owner=texture;
        auto* identity=texture.get(); DRAWSTATE.SetTexture(0,TextureBinding(texture)); texture.reset();
        Renderer::EffectDraw before,after; std::string error;
        Check(CaptureMaterialState(before,error),"CPU material snapshot");
        // Nested CPU overrides cannot leak into the restored draw description.
        DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable,TRUE);
        DRAWSTATE.SaveRenderState(Renderer::StateCullMode,Renderer::CullCw);
        DRAWSTATE.SaveSamplerState(0,Renderer::SamplerAddressU,Renderer::AddressMirror);
        DRAWSTATE.SaveTextureStageState(0,Renderer::StageColorOp,Renderer::TextureOpAdd);
        Math::Matrix poison; Math::MatrixScaling(&poison,8,9,10);
        DRAWSTATE.SaveTransform(Renderer::MatrixWorld,&poison);
        DRAWSTATE.RestoreTransform(Renderer::MatrixWorld);
        DRAWSTATE.RestoreTextureStageState(0,Renderer::StageColorOp);
        DRAWSTATE.RestoreSamplerState(0,Renderer::SamplerAddressU);
        DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
        DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
        Check(CaptureMaterialState(after,error),"poison-independent snapshot");
        Check(after.blend==before.blend && after.cull==before.cull && after.colorOp==before.colorOp &&
            after.sampler.addressU==before.sampler.addressU && after.sampler.min==before.sampler.min &&
            after.matrices.world==before.matrices.world,"CPU state leaked into CPU snapshot");
        DrawStateView view; Math::Viewport actual{}; view.GetViewport(&actual); Check(actual.X==5 && actual.Width==160,"viewport");
        RECT actualClip{}; view.GetScissorRect(&actualClip); Check(actualClip.left==1 && actualClip.bottom==80,"scissor");
        BOOL enabled=FALSE; view.GetLightEnable(1,&enabled); Check(enabled,"light enable");
        Renderer::LightValues actualLight{}; Check(SUCCEEDED(view.GetLight(1,&actualLight)) && actualLight.Position.x==12,"light definition");
        float actualConstants[4]{}; view.GetVertexConstants(4,actualConstants,1); Check(actualConstants[0]==3,"tree shader constants");
        Check(view.GetTextureBinding(0).source.get()==identity && !owner.expired(),"retained CPU texture snapshot");
        DRAWSTATE.SetTexture(0,nullptr); Check(owner.expired(),"CPU texture released");
        screen.End(); Check(graphics.ResizeBackBuffer(640,480),"CPU viewport resize");
        view.GetViewport(&actual); Check(actual.Width==640 && actual.Height==480,"reset viewport");
        graphics.Destroy(); Check(Renderer::liveSourceTextures==0,"all CPU owners released"); DestroyWindow(window);
        std::cout<<"CPU state/texture/light/viewport/constants isolation, resize/shutdown: PASS\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; DestroyWindow(window); return 1; }
}
