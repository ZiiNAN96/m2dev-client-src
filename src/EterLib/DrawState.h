/******************************************************************************

  Copyright (C) 1999, 2000 NVIDIA Corporation

  This file is provided without support, instruction, or implied warranty of any
  kind.  NVIDIA makes no guarantee of its fitness for a particular purpose and is
  not liable under any circumstances for any damages or loss whatsoever arising
  from the use or inability to use this file or items derived from it.

  CPU draw-description storage, derived from the original state manager.
  Save/Restore scopes preserve producer semantics. Only CPU draw data is stored.

******************************************************************************/

#ifndef __CPU_DRAW_STATE_H
#define __CPU_DRAW_STATE_H

#include "Renderer/DrawStateTypes.h"
#include "../Renderer/MatrixState.h"
#include "Renderer/DrawStateTypes.h"
#include "Math/Math.h"

#include <vector>
#include <stack>

#include "TextureBinding.h"
#include <cstdint>

#include "EterBase/Singleton.h"


static const DWORD DRAWSTATE_MAX_RENDERSTATES = 256;
static const DWORD DRAWSTATE_MAX_TEXTURESTATES = 128;
static const DWORD DRAWSTATE_MAX_STAGES = 8;
static const DWORD DRAWSTATE_MAX_VCONSTANTS = 96;
static const DWORD DRAWSTATE_MAX_TRANSFORMSTATES = Renderer::MatrixSlotCount;


// ZiiNAN: Removed final D3D9 compile-time dependency. CPU material state only.
class DrawStateData
{
public:
    void ResetState() {
        for(auto& matrix:m_Matrices) Math::MatrixIdentity(&matrix);
    }
    DWORD m_RenderStates[DRAWSTATE_MAX_RENDERSTATES]{};
    DWORD m_TextureStates[DRAWSTATE_MAX_STAGES][DRAWSTATE_MAX_TEXTURESTATES]{};
    DWORD m_SamplerStates[DRAWSTATE_MAX_STAGES][DRAWSTATE_MAX_TEXTURESTATES]{};
    Math::Matrix m_Matrices[DRAWSTATE_MAX_TRANSFORMSTATES];
    Renderer::MaterialValues m_material{};
};
class CDrawState : public CSingleton<CDrawState>
{
public:
	CDrawState();
	virtual ~CDrawState();

	void	SetDefaultState();

	bool	BeginScene();
	void	EndScene();

	// Material
	void	SaveMaterial();
	void	SaveMaterial(const Renderer::MaterialValues* pMaterial);
	void	RestoreMaterial();
	void	SetMaterial(const Renderer::MaterialValues* pMaterial);
	void	GetMaterial(Renderer::MaterialValues* pMaterial);

	void	SetLight(DWORD index, CONST Renderer::LightValues* pLight);
	void	GetLight(DWORD index, Renderer::LightValues* pLight);

	// Scissor Rect
	void	SetScissorRect(const RECT& c_rRect);
	void	GetScissorRect(RECT* pRect);

	// Renderstates
	void	SaveRenderState(Renderer::RenderStateKey Type, DWORD dwValue);
	void	RestoreRenderState(Renderer::RenderStateKey Type);
	void	SetRenderState(Renderer::RenderStateKey Type, DWORD Value);
	void	GetRenderState(Renderer::RenderStateKey Type, DWORD* pdwValue);

	// Textures
	void	SaveTexture(DWORD dwStage, TextureBinding texture);
	void	RestoreTexture(DWORD dwStage);
	void	SetTexture(DWORD dwStage, TextureBinding texture);

	// Texture stage states
	void	SaveTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type, DWORD dwValue);
	void	RestoreTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type);
	void	SetTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type, DWORD dwValue);
	void	GetTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type, DWORD* pdwValue);
	void	SetBestFiltering(DWORD dwStage); // if possible set anisotropy filtering, or use trilinear

	// Sampler states
	void	SaveSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type, DWORD dwValue);
	void	RestoreSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type);
	void	SetSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type, DWORD dwValue);
	void	GetSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type, DWORD* pdwValue);
	// Vertex Declaration
	// *** These states are cached, but not protected from multiple sends of the same value.
	// Transform
	void SaveTransform(Renderer::MatrixSlot Transform, const Math::Matrix* pMatrix);
	void RestoreTransform(Renderer::MatrixSlot Transform);
	// Don't cache-check the transform.  To much to do
	void SetTransform(Renderer::MatrixSlot Type, const Math::Matrix* pMatrix);
	void GetTransform(Renderer::MatrixSlot Type, Math::Matrix* pMatrix);

	// SetVertexConstants
	void SetVertexConstants(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount);
	DWORD GetRenderState(Renderer::RenderStateKey Type);

    // CPU material/lighting/sampler values; no device cache or native dispatch.
    TextureBinding GetTextureBinding(DWORD stage) const { return m_textureBindings[stage]; }
    HRESULT SetViewport(const Math::Viewport* viewport);
    HRESULT LightEnable(DWORD index,BOOL enabled);


private:
    friend class DrawStateView;
    void InitializeDrawDefaults();
    Math::Viewport m_viewport{};
    RECT m_scissor{};
    Renderer::LightValues m_lights[8]{};
    BOOL m_lightEnabled[8]{};
    bool m_lightValid[8]{};
    float m_vertexConstants[96][4]{};
    TextureBinding m_textureBindings[8];

private:

	DrawStateData	m_CurrentState;

	bool				m_bScene;
	DWORD				m_dwBestMinFilter;
	DWORD				m_dwBestMagFilter;

	std::vector<DWORD>						m_RenderStateStack[DRAWSTATE_MAX_RENDERSTATES];
	std::vector<DWORD>						m_SamplerStateStack[DRAWSTATE_MAX_STAGES][DRAWSTATE_MAX_TEXTURESTATES];
	std::vector<DWORD>						m_TextureStageStateStack[DRAWSTATE_MAX_STAGES][DRAWSTATE_MAX_TEXTURESTATES];
	std::vector<Math::Matrix>					m_TransformStack[DRAWSTATE_MAX_TRANSFORMSTATES];
	std::vector<TextureBinding>		m_TextureStack[DRAWSTATE_MAX_STAGES];
	std::vector<Renderer::MaterialValues>				m_MaterialStack;

};

// Producer-local CPU state; renderers consume immutable snapshots.
#define DRAWSTATE (CDrawState::Instance())

#endif
