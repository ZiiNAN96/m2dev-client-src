#pragma once
#include "StateManager.h"

// ZiiNAN: Diligent snapshots read CPU values; Legacy/parity tests still read the device.
class NativeStateView
{
    CStateManager& s=STATEMANAGER;
public:
    HRESULT GetRenderState(D3DRENDERSTATETYPE t,DWORD* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetRenderState(t,v); *v=s.m_CurrentState.m_RenderStates[t]; return S_OK; }
    HRESULT GetTextureStageState(DWORD stage,D3DTEXTURESTAGESTATETYPE t,DWORD* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetTextureStageState(stage,t,v); *v=s.m_CurrentState.m_TextureStates[stage][t]; return S_OK; }
    HRESULT GetSamplerState(DWORD stage,D3DSAMPLERSTATETYPE t,DWORD* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetSamplerState(stage,t,v); *v=s.m_CurrentState.m_SamplerStates[stage][t]; return S_OK; }
    HRESULT GetTransform(D3DTRANSFORMSTATETYPE t,D3DMATRIX* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetTransform(t,v); *v=s.m_CurrentState.m_Matrices[t]; return S_OK; }
    HRESULT GetMaterial(D3DMATERIAL9* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetMaterial(v); *v=s.m_CurrentState.m_D3DMaterial; return S_OK; }
    HRESULT GetTexture(DWORD stage,IDirect3DBaseTexture9** v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetTexture(stage,v); return s.m_textureOwners[stage].CopyTo(v); }
    HRESULT GetVertexShader(IDirect3DVertexShader9** v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetVertexShader(v); return s.m_vertexShaderOwner.CopyTo(v); }
    HRESULT GetPixelShader(IDirect3DPixelShader9** v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetPixelShader(v); return s.m_pixelShaderOwner.CopyTo(v); }
    HRESULT GetViewport(D3DVIEWPORT9* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetViewport(v); *v=s.m_viewport; return S_OK; }
    HRESULT GetScissorRect(RECT* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetScissorRect(v); *v=s.m_scissor; return S_OK; }
    HRESULT GetLight(DWORD index,D3DLIGHT9* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetLight(index,v); if(index>=8 || !s.m_lightValid[index]) return D3DERR_INVALIDCALL; *v=s.m_lights[index]; return S_OK; }
    HRESULT GetLightEnable(DWORD index,BOOL* v) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetLightEnable(index,v); if(index>=8) return D3DERR_INVALIDCALL; *v=s.m_lightEnabled[index]; return S_OK; }
    HRESULT GetVertexShaderConstantF(UINT start,float* v,UINT count) const
    { if(!s.m_diligentRendering) return s.GetDevice()->GetVertexShaderConstantF(start,v,count); if(start>96 || count>96-start) return D3DERR_INVALIDCALL; memcpy(v,s.m_vertexConstants[start],count*16); return S_OK; }
};
