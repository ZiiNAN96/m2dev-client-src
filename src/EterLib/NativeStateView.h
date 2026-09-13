#pragma once
#include "StateManager.h"

// ZiiNAN: CPU draw-description view; no native device queries or owners.
class NativeStateView
{
    CStateManager& s=STATEMANAGER;
public:
    HRESULT GetRenderState(D3DRENDERSTATETYPE t,DWORD* v) const
    { *v=s.m_CurrentState.m_RenderStates[t]; return S_OK; }
    HRESULT GetTextureStageState(DWORD stage,D3DTEXTURESTAGESTATETYPE t,DWORD* v) const
    { *v=s.m_CurrentState.m_TextureStates[stage][t]; return S_OK; }
    HRESULT GetSamplerState(DWORD stage,D3DSAMPLERSTATETYPE t,DWORD* v) const
    { *v=s.m_CurrentState.m_SamplerStates[stage][t]; return S_OK; }
    HRESULT GetTransform(Renderer::MatrixSlot t,D3DMATRIX* v) const
    { *v=s.m_CurrentState.m_Matrices[t]; return S_OK; }
    HRESULT GetMaterial(D3DMATERIAL9* v) const
    { *v=s.m_CurrentState.m_D3DMaterial; return S_OK; }
    TextureBinding GetTextureBinding(DWORD stage) const
    {
        return s.GetTextureBinding(stage);
    }
    HRESULT GetTexture(DWORD stage,IDirect3DBaseTexture9** v) const
    { *v=nullptr; return S_OK; }
    HRESULT GetVertexShader(IDirect3DVertexShader9** v) const
    { *v=nullptr; return S_OK; }
    HRESULT GetPixelShader(IDirect3DPixelShader9** v) const
    { *v=nullptr; return S_OK; }
    HRESULT GetViewport(D3DVIEWPORT9* v) const
    { *v=s.m_viewport; return S_OK; }
    HRESULT GetScissorRect(RECT* v) const
    { *v=s.m_scissor; return S_OK; }
    HRESULT GetLight(DWORD index,D3DLIGHT9* v) const
    { if(index>=8 || !s.m_lightValid[index]) return D3DERR_INVALIDCALL; *v=s.m_lights[index]; return S_OK; }
    HRESULT GetLightEnable(DWORD index,BOOL* v) const
    { if(index>=8) return D3DERR_INVALIDCALL; *v=s.m_lightEnabled[index]; return S_OK; }
    HRESULT GetVertexShaderConstantF(UINT start,float* v,UINT count) const
    { if(start>96 || count>96-start) return D3DERR_INVALIDCALL; memcpy(v,s.m_vertexConstants[start],count*16); return S_OK; }
};
