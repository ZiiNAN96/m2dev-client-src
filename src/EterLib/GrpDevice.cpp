#include "StdAfx.h"
#include "GrpDevice.h"
#include "Renderer/TerrainPresentation.h"
#include "EterBase/Stl.h"
#include <intrin.h>

bool CPU_HAS_SSE2 = false;
bool GRAPHICS_CAPS_CAN_NOT_DRAW_LINE = false;
bool GRAPHICS_CAPS_CAN_NOT_DRAW_SHADOW = true;
bool GRAPHICS_CAPS_HALF_SIZE_IMAGE = false;
bool GRAPHICS_CAPS_CAN_NOT_TEXTURE_ADDRESS_BORDER = false;
bool GRAPHICS_CAPS_SOFTWARE_TILING = false;

// ZiiNAN: Legacy D3D9 renderer removed from production path.
// This compatibility-named owner initializes only CPU camera/draw data.
CGraphicDevice::CGraphicDevice() : m_uBackBufferCount(0), m_pStateManager(nullptr) {}
CGraphicDevice::~CGraphicDevice() { Destroy(); }
void CGraphicDevice::InitBackBufferCount(UINT) {}
void CGraphicDevice::RegisterWarningString(UINT, const char*) {}
void CGraphicDevice::MoveWebBrowserRect(const RECT&) {}
void CGraphicDevice::EnableWebBrowserMode(const RECT&) {}
void CGraphicDevice::DisableWebBrowserMode() {}
DWORD GetMaxTextureWidth() { return Renderer::graphicsCapabilities.maxTextureDimension; }
DWORD GetMaxTextureHeight() { return Renderer::graphicsCapabilities.maxTextureDimension; }

int CGraphicDevice::Create(HWND window,int width,int height,bool windowed,int,int)
{
    if(m_pStateManager || !window || width<=0 || height<=0 || !windowed) return CREATE_DEVICE;
    ms_hWnd=window; ms_hDC=GetDC(window);
    m_pStateManager=new CRenderState();
    if(FAILED(D3DXCreateMatrixStack(0,&ms_lpd3dMatStack))) { Destroy(); return CREATE_DEVICE; }
    ms_lpd3dMatStack->LoadIdentity();
    for(auto* matrix : {&ms_matIdentity,&ms_matWorld,&ms_matWorldView,&ms_matView,&ms_matProj,
        &ms_matInverseView,&ms_matInverseViewYAxis,&ms_matScreen0,&ms_matScreen1,&ms_matScreen2})
        D3DXMatrixIdentity(matrix);
    ms_matScreen0._22=-1;
    ms_matScreen1._41=ms_matScreen1._42=1;
    ms_dwWavingEndTime=ms_dwFlashingEndTime=0;
    ms_bSupportDXT=true;
    ms_isLowTextureMemory=false;
    ms_isHighTextureMemory=true;
    int cpu[4]; __cpuid(cpu,1); CPU_HAS_SSE2=(cpu[3] & (1<<26))!=0;
    return ResizeBackBuffer(width,height) ? CREATE_OK : CREATE_DEVICE;
}
bool CGraphicDevice::ResizeBackBuffer(UINT width,UINT height)
{
    if(!m_pStateManager) return false;
    if(!width || !height) return true; // Suspension does not discard CPU resource/state owners.
    ms_iWidth=width; ms_iHeight=height;
    ms_Viewport={0,0,width,height,0,1};
    m_pStateManager->SetViewport(&ms_Viewport);
    m_pStateManager->SetScissorRect(RECT{0,0,LONG(width),LONG(height)});
    ms_matScreen2._11=float(width)/2; ms_matScreen2._22=float(height)/2;
    return true;
}
CGraphicDevice::EDeviceState CGraphicDevice::GetDeviceState()
{ return m_pStateManager ? DEVICESTATE_OK : DEVICESTATE_NULL; }
bool CGraphicDevice::Reset() { return false; } // No D3D9 lost-device recovery.
void CGraphicDevice::Destroy()
{
    delete m_pStateManager; m_pStateManager=nullptr;
    safe_release(ms_lpd3dMatStack);
    if(ms_hDC) ReleaseDC(ms_hWnd,ms_hDC);
    ms_hDC=nullptr; ms_hWnd=nullptr;
}
