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
CGraphicDevice::CGraphicDevice() : m_drawState(nullptr) {}
CGraphicDevice::~CGraphicDevice() { Destroy(); }
DWORD GetMaxTextureWidth() { return Renderer::graphicsCapabilities.maxTextureDimension; }
DWORD GetMaxTextureHeight() { return Renderer::graphicsCapabilities.maxTextureDimension; }

int CGraphicDevice::Create(Platform::NativeWindowHandle window,int width,int height,bool windowed,int,int)
{
    if(m_drawState || !window || width<=0 || height<=0 || !windowed) return CREATE_DEVICE;
    // ZiiNAN: Platform abstraction
    m_drawState=new CDrawState();
    ms_matrixStack.Clear();
    for(auto* matrix : {&ms_matIdentity,&ms_matWorld,&ms_matWorldView,&ms_matView,&ms_matProj,
        &ms_matInverseView,&ms_matInverseViewYAxis,&ms_matScreen0,&ms_matScreen1,&ms_matScreen2})
        Math::MatrixIdentity(matrix);
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
    if(!m_drawState) return false;
    if(!width || !height) return true; // Suspension does not discard CPU resource/state owners.
    ms_iWidth=width; ms_iHeight=height;
    ms_Viewport={0,0,width,height,0,1};
    m_drawState->SetViewport(&ms_Viewport);
    m_drawState->SetScissorRect(RECT{0,0,LONG(width),LONG(height)});
    ms_matScreen2._11=float(width)/2; ms_matScreen2._22=float(height)/2;
    return true;
}
void CGraphicDevice::Destroy()
{
    delete m_drawState; m_drawState=nullptr;
    ms_matrixStack.Clear();
}
