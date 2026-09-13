#pragma once
#include "DrawState.h"

// ZiiNAN: CPU draw-description view; no native device queries or owners.
class DrawStateView
{
    CDrawState& s=DRAWSTATE;
public:
    HRESULT GetRenderState(Renderer::RenderStateKey t,DWORD* v) const
    { *v=s.m_CurrentState.m_RenderStates[t]; return S_OK; }
    HRESULT GetTextureStageState(DWORD stage,Renderer::TextureStageKey t,DWORD* v) const
    { *v=s.m_CurrentState.m_TextureStates[stage][t]; return S_OK; }
    HRESULT GetSamplerState(DWORD stage,Renderer::SamplerStateKey t,DWORD* v) const
    { *v=s.m_CurrentState.m_SamplerStates[stage][t]; return S_OK; }
    HRESULT GetTransform(Renderer::MatrixSlot t,Math::Matrix* v) const
    { *v=s.m_CurrentState.m_Matrices[t]; return S_OK; }
    HRESULT GetMaterial(Renderer::MaterialValues* v) const
    { *v=s.m_CurrentState.m_material; return S_OK; }
    TextureBinding GetTextureBinding(DWORD stage) const
    {
        return s.GetTextureBinding(stage);
    }



    HRESULT GetViewport(Math::Viewport* v) const
    { *v=s.m_viewport; return S_OK; }
    HRESULT GetScissorRect(RECT* v) const
    { *v=s.m_scissor; return S_OK; }
    HRESULT GetLight(DWORD index,Renderer::LightValues* v) const
    { if(index>=8 || !s.m_lightValid[index]) return E_INVALIDARG; *v=s.m_lights[index]; return S_OK; }
    HRESULT GetLightEnable(DWORD index,BOOL* v) const
    { if(index>=8) return E_INVALIDARG; *v=s.m_lightEnabled[index]; return S_OK; }
    HRESULT GetVertexConstants(UINT start,float* v,UINT count) const
    { if(start>96 || count>96-start) return E_INVALIDARG; memcpy(v,s.m_vertexConstants[start],count*16); return S_OK; }
};
