// ZiiNAN: CPU draw descriptions, nested state isolation and owners without any D3D9 device.
#include "EterLib/StdAfx.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/NativeResourceAudit.h"
#include "EterLib/NativeStateView.h"
#include "EterLib/NativeMaterialSnapshot.h"
#include "EterLib/Camera.h"
#include <iostream>
#include <stdexcept>
float CCamera::CAMERA_MAX_DISTANCE=2500.0f;
static void Check(bool condition,const char* error) { if(!condition) throw std::runtime_error(error); }
int main()
{
    HWND window=CreateWindowW(L"STATIC",L"Native state isolation test",WS_OVERLAPPEDWINDOW,0,0,320,240,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    try {
        CGraphicDevice graphics; CScreen screen;
        Check(window && graphics.Create(window,320,240)==CGraphicDevice::CREATE_OK,"device-free CPU context");
        Check(screen.Begin(),"CPU frame");
        Check(Renderer::compatibilityDeviceCreations==0,"no compatibility device");
        STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        STATEMANAGER.SetRenderState(D3DRS_COLORWRITEENABLE,15);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_NONE);
        STATEMANAGER.SetVertexShader(nullptr); STATEMANAGER.SetPixelShader(nullptr);
        D3DXMATRIX matrix; D3DXMatrixTranslation(&matrix,3,4,5);
        STATEMANAGER.SetTransform(Renderer::MatrixWorld,&matrix);
        D3DVIEWPORT9 viewport{5,7,160,100,0,1}; STATEMANAGER.SetViewport(&viewport);
        RECT clip{1,2,100,80}; STATEMANAGER.SetScissorRect(clip);
        D3DLIGHT9 light{}; light.Type=D3DLIGHT_POINT; light.Position.x=12; light.Range=100;
        STATEMANAGER.SetLight(1,&light); STATEMANAGER.LightEnable(1,TRUE);
        float constants[4]={3,4,5,6}; STATEMANAGER.SetVertexShaderConstant(4,constants,1);
        auto texture=Renderer::TextureResource::Dynamic(4,4,Renderer::TerrainTextureFormat::BGRA8);
        std::weak_ptr<Renderer::TextureResource> owner=texture;
        auto* identity=texture.get(); STATEMANAGER.SetTexture(0,TextureBinding(texture)); texture.reset();
        Renderer::EffectDraw before,after; std::string error;
        Check(CaptureNativeMaterial(before,error),"CPU material snapshot");
        // Nested CPU overrides cannot leak into the restored draw description.
        STATEMANAGER.SaveRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
        STATEMANAGER.SaveRenderState(D3DRS_CULLMODE,D3DCULL_CW);
        STATEMANAGER.SaveSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_MIRROR);
        STATEMANAGER.SaveTextureStageState(0,D3DTSS_COLOROP,D3DTOP_ADD);
        D3DXMATRIX poison; D3DXMatrixScaling(&poison,8,9,10);
        STATEMANAGER.SaveTransform(Renderer::MatrixWorld,&poison);
        STATEMANAGER.RestoreTransform(Renderer::MatrixWorld);
        STATEMANAGER.RestoreTextureStageState(0,D3DTSS_COLOROP);
        STATEMANAGER.RestoreSamplerState(0,D3DSAMP_ADDRESSU);
        STATEMANAGER.RestoreRenderState(D3DRS_CULLMODE);
        STATEMANAGER.RestoreRenderState(D3DRS_ALPHABLENDENABLE);
        Check(CaptureNativeMaterial(after,error),"poison-independent snapshot");
        Check(after.blend==before.blend && after.cull==before.cull && after.colorOp==before.colorOp &&
            after.sampler.addressU==before.sampler.addressU && after.sampler.min==before.sampler.min &&
            after.matrices.world==before.matrices.world,"native state leaked into CPU snapshot");
        NativeStateView view; D3DVIEWPORT9 actual{}; view.GetViewport(&actual); Check(actual.X==5 && actual.Width==160,"viewport");
        RECT actualClip{}; view.GetScissorRect(&actualClip); Check(actualClip.left==1 && actualClip.bottom==80,"scissor");
        BOOL enabled=FALSE; view.GetLightEnable(1,&enabled); Check(enabled,"light enable");
        D3DLIGHT9 actualLight{}; Check(SUCCEEDED(view.GetLight(1,&actualLight)) && actualLight.Position.x==12,"light definition");
        float actualConstants[4]{}; view.GetVertexShaderConstantF(4,actualConstants,1); Check(actualConstants[0]==3,"tree shader constants");
        Check(view.GetTextureBinding(0).source.get()==identity && !owner.expired(),"retained CPU texture snapshot");
        STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLELIST,0,1);
        STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,nullptr,24);
        STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,3,0,1);
        STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1);
        STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,3,1,nullptr,D3DFMT_INDEX16,nullptr,24);
        const auto calls=STATEMANAGER.GetNativeCounters();
        Check(calls.draws==0 && calls.states==0 && calls.textures==0 && calls.targets==0 && calls.suppressedDraws==5,"native execution escaped");
        STATEMANAGER.SetTexture(0,nullptr); Check(owner.expired(),"CPU texture released");
        screen.End(); Check(graphics.ResizeBackBuffer(640,480),"CPU viewport resize");
        view.GetViewport(&actual); Check(actual.Width==640 && actual.Height==480,"reset viewport");
        graphics.Destroy(); Check(Renderer::liveSourceTextures==0,"all CPU owners released"); DestroyWindow(window);
        std::cout<<"CPU state/texture/light/viewport/constants isolation, 5 suppressed draws, native calls zero, resize/shutdown: PASS\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; DestroyWindow(window); return 1; }
}
