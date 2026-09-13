// ZiiNAN: Poison the native device after startup; CPU snapshots must remain unchanged.
#include "EterLib/StdAfx.h"
#include "EterLib/GrpDevice.h"
#include "EterLib/GrpScreen.h"
#include "EterLib/LegacyD3D9Backend.h"
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
        CGraphicDevice graphics; CScreen screen; Renderer::LegacyD3D9Backend backend(graphics,screen);
        Check(window && backend.Initialize({window,320,240}),"native initialization");
        auto* device=STATEMANAGER.GetDevice();
        STATEMANAGER.EnableDiligentRendering();
        Check(backend.BeginFrame(),"CPU frame");
        STATEMANAGER.SetRenderState(D3DRS_ALPHABLENDENABLE,FALSE);
        STATEMANAGER.SetRenderState(D3DRS_CULLMODE,D3DCULL_NONE);
        STATEMANAGER.SetRenderState(D3DRS_COLORWRITEENABLE,15);
        STATEMANAGER.SetTextureStageState(0,D3DTSS_TEXCOORDINDEX,0);
        STATEMANAGER.SetTextureStageState(1,D3DTSS_COLOROP,D3DTOP_DISABLE);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_CLAMP);
        STATEMANAGER.SetSamplerState(0,D3DSAMP_MINFILTER,D3DTEXF_NONE);
        STATEMANAGER.SetVertexShader(nullptr); STATEMANAGER.SetPixelShader(nullptr);
        D3DXMATRIX matrix; D3DXMatrixTranslation(&matrix,3,4,5);
        STATEMANAGER.SetTransform(D3DTS_WORLD,&matrix);
        D3DVIEWPORT9 viewport{5,7,160,100,0,1}; STATEMANAGER.SetViewport(&viewport);
        RECT clip{1,2,100,80}; STATEMANAGER.SetScissorRect(clip);
        D3DLIGHT9 light{}; light.Type=D3DLIGHT_POINT; light.Position.x=12; light.Range=100;
        STATEMANAGER.SetLight(1,&light); STATEMANAGER.LightEnable(1,TRUE);
        float constants[4]={3,4,5,6}; STATEMANAGER.SetVertexShaderConstant(4,constants,1);
        Microsoft::WRL::ComPtr<IDirect3DTexture9> texture;
        Check(SUCCEEDED(device->CreateTexture(4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,nullptr)),"texture owner");
        auto* identity=texture.Get(); STATEMANAGER.SetTexture(0,identity); texture.Reset();
        Renderer::EffectDraw before,after; std::string error;
        Check(CaptureNativeMaterial(before,error),"CPU material snapshot");
        device->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE); device->SetRenderState(D3DRS_CULLMODE,D3DCULL_CW);
        device->SetSamplerState(0,D3DSAMP_ADDRESSU,D3DTADDRESS_MIRROR);
        device->SetTextureStageState(0,D3DTSS_COLOROP,D3DTOP_ADD);
        D3DXMATRIX poison; D3DXMatrixScaling(&poison,8,9,10); device->SetTransform(D3DTS_WORLD,&poison);
        D3DVIEWPORT9 poisonViewport{0,0,320,240,0,1}; device->SetViewport(&poisonViewport);
        RECT poisonClip{0,0,320,240}; device->SetScissorRect(&poisonClip);
        device->LightEnable(1,FALSE); float zeros[4]{}; device->SetVertexShaderConstantF(4,zeros,1);
        Check(CaptureNativeMaterial(after,error),"poison-independent snapshot");
        Check(after.blend==before.blend && after.cull==before.cull && after.colorOp==before.colorOp &&
            after.sampler.addressU==before.sampler.addressU && after.sampler.min==before.sampler.min &&
            after.matrices.world==before.matrices.world,"native state leaked into CPU snapshot");
        NativeStateView view; D3DVIEWPORT9 actual{}; view.GetViewport(&actual); Check(actual.X==5 && actual.Width==160,"viewport");
        RECT actualClip{}; view.GetScissorRect(&actualClip); Check(actualClip.left==1 && actualClip.bottom==80,"scissor");
        BOOL enabled=FALSE; view.GetLightEnable(1,&enabled); Check(enabled,"light enable");
        D3DLIGHT9 actualLight{}; Check(SUCCEEDED(view.GetLight(1,&actualLight)) && actualLight.Position.x==12,"light definition");
        float actualConstants[4]{}; view.GetVertexShaderConstantF(4,actualConstants,1); Check(actualConstants[0]==3,"tree shader constants");
        Microsoft::WRL::ComPtr<IDirect3DBaseTexture9> bound; view.GetTexture(0,&bound); Check(bound.Get()==identity,"retained texture snapshot");
        STATEMANAGER.DrawPrimitive(D3DPT_TRIANGLELIST,0,1);
        STATEMANAGER.DrawPrimitiveUP(D3DPT_TRIANGLELIST,1,nullptr,24);
        STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,3,0,1);
        STATEMANAGER.DrawIndexedPrimitive(D3DPT_TRIANGLELIST,0,0,3,0,1);
        STATEMANAGER.DrawIndexedPrimitiveUP(D3DPT_TRIANGLELIST,0,3,1,nullptr,D3DFMT_INDEX16,nullptr,24);
        const auto calls=STATEMANAGER.GetNativeCounters();
        Check(calls.draws==0 && calls.states==0 && calls.textures==0 && calls.targets==0 && calls.suppressedDraws==5,"native execution escaped");
        STATEMANAGER.ForgetDiligentTexture(identity); bound.Reset();
        backend.EndFrame(); Check(backend.Resize(640,480),"CPU state reset/resize");
        view.GetViewport(&actual); Check(actual.Width==640 && actual.Height==480,"reset viewport");
        backend.Shutdown(); DestroyWindow(window);
        std::cout<<"CPU state/texture/light/viewport/constants isolation, 5 suppressed draws, native calls zero, resize/shutdown: PASS\n";
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; DestroyWindow(window); return 1; }
}
