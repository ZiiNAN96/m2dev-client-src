#include "StdAfx.h"
#include "DrawState.h"
#include "GrpLightManager.h"

//#define DrawState_Assert(a) if (!(a)) puts("assert"#a)
#define DrawState_Assert(a) assert(a)

void CDrawState::InitializeDrawDefaults()
{
    // ZiiNAN: Legacy D3D9 renderer removed from production path.
    // Explicit CPU defaults. No device exists to seed or restore from.
    for (auto& value : m_CurrentState.m_RenderStates) value=0;
    m_CurrentState.m_RenderStates[Renderer::StateTextureFactor]=0xffffffff;
    m_CurrentState.m_RenderStates[Renderer::StateBlendOpAlpha]=Renderer::BlendOpAdd;
    m_CurrentState.m_RenderStates[Renderer::StateSrcBlendAlpha]=Renderer::BlendOne;
    m_CurrentState.m_RenderStates[Renderer::StateDestBlendAlpha]=Renderer::BlendZero;
    for (DWORD stage=0; stage<8; ++stage) {
        for (auto& value : m_CurrentState.m_TextureStates[stage]) value=0;
        for (auto& value : m_CurrentState.m_SamplerStates[stage]) value=0;
        m_CurrentState.m_TextureStates[stage][Renderer::StageColorArg0]=Renderer::ArgCurrent;
        m_CurrentState.m_TextureStates[stage][Renderer::StageAlphaArg0]=Renderer::ArgCurrent;
        m_CurrentState.m_TextureStates[stage][Renderer::StageResultArg]=Renderer::ArgCurrent;
        m_CurrentState.m_SamplerStates[stage][Renderer::SamplerAddressW]=Renderer::AddressWrap;
        m_CurrentState.m_SamplerStates[stage][Renderer::SamplerMaxAnisotropy]=1;
        m_lightEnabled[stage]=FALSE; m_lightValid[stage]=false; m_lights[stage]={};
    }
}

HRESULT CDrawState::SetViewport(const Math::Viewport* viewport)
{
    if(!viewport || !viewport->Width || !viewport->Height || viewport->MinZ>viewport->MaxZ) return E_INVALIDARG;
    m_viewport=*viewport;
    return S_OK;
}
HRESULT CDrawState::LightEnable(DWORD index,BOOL enabled)
{
    if(index>=8) return E_INVALIDARG;
    m_lightEnabled[index]=enabled;
    return S_OK;
}








void CDrawState::SetLight(DWORD index, CONST Renderer::LightValues* pLight)
{
	assert(index < 8);

    m_lights[index]=*pLight; m_lightValid[index]=true;
}

void CDrawState::GetLight(DWORD index, Renderer::LightValues* pLight)
{
	assert(index < 8);
	*pLight = m_lights[index];
}

void CDrawState::SetScissorRect(const RECT& c_rRect)
{
    m_scissor=c_rRect;
}

void CDrawState::GetScissorRect(RECT* pRect)
{
    *pRect=m_scissor;
}

bool CDrawState::BeginScene()
{
    m_bScene=true; return true;
}

void CDrawState::EndScene()
{
    m_bScene=false;
}

CDrawState::CDrawState()
{
    m_bScene=false;
    m_dwBestMinFilter=m_dwBestMagFilter=Renderer::FilterAnisotropic;
    SetDefaultState();
}

CDrawState::~CDrawState()
{

}

void CDrawState::SetBestFiltering(DWORD dwStage)
{
	SetSamplerState(dwStage, Renderer::SamplerMinFilter, m_dwBestMinFilter);
	SetSamplerState(dwStage, Renderer::SamplerMagFilter, m_dwBestMagFilter);
	SetSamplerState(dwStage, Renderer::SamplerMipFilter, Renderer::FilterLinear);
}



void CDrawState::SetDefaultState()
{
    for(auto& binding:m_textureBindings) binding={};
	m_CurrentState.ResetState();
    InitializeDrawDefaults();

	for (auto& stack : m_RenderStateStack)
		stack.clear();

	for (auto& stageStacks : m_SamplerStateStack)
		for (auto& stack : stageStacks)
			stack.clear();

	for (auto& stageStacks : m_TextureStageStateStack)
		for (auto& stack : stageStacks)
			stack.clear();

	for (auto& stack : m_TransformStack)
		stack.clear();

	for (auto& stack : m_TextureStack)
		stack.clear();

	m_MaterialStack.clear();

	m_bScene = false;

	Math::Matrix matIdentity;
	Math::MatrixIdentity(&matIdentity);

	SetTransform(Renderer::MatrixWorld, &matIdentity);
	SetTransform(Renderer::MatrixView, &matIdentity);
	SetTransform(Renderer::MatrixProjection, &matIdentity);

	Renderer::MaterialValues DefaultMat;
	ZeroMemory(&DefaultMat, sizeof(Renderer::MaterialValues));

	DefaultMat.Diffuse.r = 1.0f;
	DefaultMat.Diffuse.g = 1.0f;
	DefaultMat.Diffuse.b = 1.0f;
	DefaultMat.Diffuse.a = 1.0f;
	DefaultMat.Ambient.r = 1.0f;
	DefaultMat.Ambient.g = 1.0f;
	DefaultMat.Ambient.b = 1.0f;
	DefaultMat.Ambient.a = 1.0f;
	DefaultMat.Emissive.r = 0.0f;
	DefaultMat.Emissive.g = 0.0f;
	DefaultMat.Emissive.b = 0.0f;
	DefaultMat.Emissive.a = 0.0f;
	DefaultMat.Specular.r = 0.0f;
	DefaultMat.Specular.g = 0.0f;
	DefaultMat.Specular.b = 0.0f;
	DefaultMat.Specular.a = 0.0f;
	DefaultMat.Power = 0.0f;

	SetMaterial(&DefaultMat);

	SetRenderState(Renderer::StateDiffuseMaterialSource, Renderer::MaterialMaterial);
	SetRenderState(Renderer::StateSpecularMaterialSource, Renderer::MaterialMaterial);
	SetRenderState(Renderer::StateAmbientMaterialSource, Renderer::MaterialMaterial);
	SetRenderState(Renderer::StateEmissiveMaterialSource, Renderer::MaterialMaterial);
	SetRenderState(Renderer::StateLastPixel, TRUE);
	SetRenderState(Renderer::StateAlphaRef, 1);
	SetRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreaterEqual);
	SetRenderState(Renderer::StateFogStart, 0);
	SetRenderState(Renderer::StateFogEnd, 0);
	SetRenderState(Renderer::StateFogDensity, 0);
	SetRenderState(Renderer::StateStencilWriteMask, 0xFFFFFFFF);
	SetRenderState(Renderer::StateAmbient, 0x00000000);
	SetRenderState(Renderer::StateLocalViewer, TRUE);
	SetRenderState(Renderer::StateNormalizeNormals, FALSE);
	SetRenderState(Renderer::StateVertexBlend, Renderer::VertexBlendDisable);
	SetRenderState(Renderer::StateClipPlaneEnable, 0);
	SetRenderState(Renderer::StateScissorTestEnable, FALSE);
	SetRenderState(Renderer::StateMultisampleAntialias, TRUE);
	SetRenderState(Renderer::StateMultisampleMask, 0xFFFFFFFF);
	SetRenderState(Renderer::StatePatchEdgeStyle, Renderer::PatchEdgeContinuous);
	SetRenderState(Renderer::StateIndexedVertexBlendEnable, FALSE);
	SetRenderState(Renderer::StateColorWriteEnable, 0xFFFFFFFF);
	SetRenderState(Renderer::StateFillMode, Renderer::FillSolid);
	SetRenderState(Renderer::StateShadeMode, Renderer::ShadeGouraud);
	SetRenderState(Renderer::StateCullMode, Renderer::CullCw);
	SetRenderState(Renderer::StateAlphaBlendEnable, FALSE);
	SetRenderState(Renderer::StateBlendOp, Renderer::BlendOpAdd);
	SetRenderState(Renderer::StateSrcBlend, Renderer::BlendSrcAlpha);
	SetRenderState(Renderer::StateDestBlend, Renderer::BlendInvSrcAlpha);
	SetRenderState(Renderer::StateFogEnable, FALSE);
	SetRenderState(Renderer::StateFogColor, 0xFF000000);
	// MR-14: Fog update by Alaric
	SetRenderState(Renderer::StateFogTableMode, Renderer::FogNone);
	// MR-14: -- END OF -- Fog update by Alaric
	SetRenderState(Renderer::StateFogVertexMode, Renderer::FogLinear);
	SetRenderState(Renderer::StateRangeFogEnable, FALSE);
	SetRenderState(Renderer::StateZEnable, TRUE);
	SetRenderState(Renderer::StateZFunc, Renderer::CompareLessEqual);
	SetRenderState(Renderer::StateZWriteEnable, TRUE);
	SetRenderState(Renderer::StateDitherEnable, TRUE);
	SetRenderState(Renderer::StateStencilEnable, FALSE);
	SetRenderState(Renderer::StateAlphaTestEnable, FALSE);
	SetRenderState(Renderer::StateClipping, TRUE);
	SetRenderState(Renderer::StateLighting, FALSE);
	SetRenderState(Renderer::StateSpecularEnable, FALSE);
	SetRenderState(Renderer::StateColorVertex, FALSE);
	SetRenderState(Renderer::StateWrap0, 0);
	SetRenderState(Renderer::StateWrap1, 0);
	SetRenderState(Renderer::StateWrap2, 0);
	SetRenderState(Renderer::StateWrap3, 0);
	SetRenderState(Renderer::StateWrap4, 0);
	SetRenderState(Renderer::StateWrap5, 0);
	SetRenderState(Renderer::StateWrap6, 0);
	SetRenderState(Renderer::StateWrap7, 0);

	SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpModulate);
	SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgCurrent);
	SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(0, Renderer::StageAlphaArg2, Renderer::ArgCurrent);
	SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg1);

	SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(1, Renderer::StageAlphaArg2, Renderer::ArgDiffuse);

	SetTextureStageState(2, Renderer::StageColorOp, Renderer::TextureOpDisable);
	SetTextureStageState(2, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(2, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	SetTextureStageState(2, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	SetTextureStageState(2, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(2, Renderer::StageAlphaArg2, Renderer::ArgDiffuse);

	SetTextureStageState(3, Renderer::StageColorOp, Renderer::TextureOpDisable);
	SetTextureStageState(3, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(3, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	SetTextureStageState(3, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	SetTextureStageState(3, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(3, Renderer::StageAlphaArg2, Renderer::ArgDiffuse);

	SetTextureStageState(4, Renderer::StageColorOp, Renderer::TextureOpDisable);
	SetTextureStageState(4, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(4, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	SetTextureStageState(4, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	SetTextureStageState(4, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(4, Renderer::StageAlphaArg2, Renderer::ArgDiffuse);

	SetTextureStageState(5, Renderer::StageColorOp, Renderer::TextureOpDisable);
	SetTextureStageState(5, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(5, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	SetTextureStageState(5, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	SetTextureStageState(5, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(5, Renderer::StageAlphaArg2, Renderer::ArgDiffuse);

	SetTextureStageState(6, Renderer::StageColorOp, Renderer::TextureOpDisable);
	SetTextureStageState(6, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(6, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	SetTextureStageState(6, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	SetTextureStageState(6, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(6, Renderer::StageAlphaArg2, Renderer::ArgDiffuse);

	SetTextureStageState(7, Renderer::StageColorOp, Renderer::TextureOpDisable);
	SetTextureStageState(7, Renderer::StageColorArg1, Renderer::ArgTexture);
	SetTextureStageState(7, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	SetTextureStageState(7, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	SetTextureStageState(7, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	SetTextureStageState(7, Renderer::StageAlphaArg2, Renderer::ArgDiffuse);

	SetTextureStageState(0, Renderer::StageTexCoordIndex, 0);
	SetTextureStageState(1, Renderer::StageTexCoordIndex, 1);
	SetTextureStageState(2, Renderer::StageTexCoordIndex, 2);
	SetTextureStageState(3, Renderer::StageTexCoordIndex, 3);
	SetTextureStageState(4, Renderer::StageTexCoordIndex, 4);
	SetTextureStageState(5, Renderer::StageTexCoordIndex, 5);
	SetTextureStageState(6, Renderer::StageTexCoordIndex, 6);
	SetTextureStageState(7, Renderer::StageTexCoordIndex, 7);

	SetSamplerState(0, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(0, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(0, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(1, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(1, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(1, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(2, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(2, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(2, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(3, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(3, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(3, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(4, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(4, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(4, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(5, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(5, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(5, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(6, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(6, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(6, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(7, Renderer::SamplerMinFilter, Renderer::FilterLinear);
	SetSamplerState(7, Renderer::SamplerMagFilter, Renderer::FilterLinear);
	SetSamplerState(7, Renderer::SamplerMipFilter, Renderer::FilterLinear);

	SetSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressWrap);
	SetSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressWrap);
	SetSamplerState(2, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(2, Renderer::SamplerAddressV, Renderer::AddressWrap);
	SetSamplerState(3, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(3, Renderer::SamplerAddressV, Renderer::AddressWrap);
	SetSamplerState(4, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(4, Renderer::SamplerAddressV, Renderer::AddressWrap);
	SetSamplerState(5, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(5, Renderer::SamplerAddressV, Renderer::AddressWrap);
	SetSamplerState(6, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(6, Renderer::SamplerAddressV, Renderer::AddressWrap);
	SetSamplerState(7, Renderer::SamplerAddressU, Renderer::AddressWrap);
	SetSamplerState(7, Renderer::SamplerAddressV, Renderer::AddressWrap);

	SetTextureStageState(0, Renderer::StageTextureTransformFlags, 0);
	SetTextureStageState(1, Renderer::StageTextureTransformFlags, 0);
	SetTextureStageState(2, Renderer::StageTextureTransformFlags, 0);
	SetTextureStageState(3, Renderer::StageTextureTransformFlags, 0);
	SetTextureStageState(4, Renderer::StageTextureTransformFlags, 0);
	SetTextureStageState(5, Renderer::StageTextureTransformFlags, 0);
	SetTextureStageState(6, Renderer::StageTextureTransformFlags, 0);
	SetTextureStageState(7, Renderer::StageTextureTransformFlags, 0);

	SetTexture(0, NULL);
	SetTexture(1, NULL);
	SetTexture(2, NULL);
	SetTexture(3, NULL);
	SetTexture(4, NULL);
	SetTexture(5, NULL);
	SetTexture(6, NULL);
	SetTexture(7, NULL);


	Math::Vector4 av4Null[DRAWSTATE_MAX_VCONSTANTS];
	memset(av4Null, 0, sizeof(av4Null));
	SetVertexConstants(0, av4Null, DRAWSTATE_MAX_VCONSTANTS);
}

// Material
void CDrawState::SaveMaterial()
{
	m_MaterialStack.push_back(m_CurrentState.m_material);
}

void CDrawState::SaveMaterial(const Renderer::MaterialValues* pMaterial)
{
	m_MaterialStack.push_back(m_CurrentState.m_material);
	SetMaterial(pMaterial);
}

void CDrawState::RestoreMaterial()
{
	SetMaterial(&m_MaterialStack.back());
	m_MaterialStack.pop_back();
}

void CDrawState::SetMaterial(const Renderer::MaterialValues* pMaterial)
{
	m_CurrentState.m_material = *pMaterial;
}

void CDrawState::GetMaterial(Renderer::MaterialValues* pMaterial)
{
	// Set the renderstate and remember it.
	*pMaterial = m_CurrentState.m_material;
}

// Renderstates
DWORD CDrawState::GetRenderState(Renderer::RenderStateKey Type)
{
	return m_CurrentState.m_RenderStates[Type];
}







void CDrawState::SaveRenderState(Renderer::RenderStateKey Type, DWORD dwValue)
{
	m_RenderStateStack[Type].push_back(m_CurrentState.m_RenderStates[Type]);
	SetRenderState(Type, dwValue);
}

void CDrawState::RestoreRenderState(Renderer::RenderStateKey Type)
{
#ifdef _DEBUG
	if (m_RenderStateStack[Type].empty())
	{
		// ZiiNAN: 64-bit safety cleanup
		Tracef(" CDrawState::SaveRenderState - This render state was not saved [%u]\n", static_cast<unsigned>(Type));
		DrawState_Assert(!" This render state was not saved!");
	}
#endif _DEBUG

	SetRenderState(Type, m_RenderStateStack[Type].back());
	m_RenderStateStack[Type].pop_back();
}

void CDrawState::SetRenderState(Renderer::RenderStateKey Type, DWORD Value)
{
	if (m_CurrentState.m_RenderStates[Type] == Value)
		return;

	m_CurrentState.m_RenderStates[Type] = Value;
}

void CDrawState::GetRenderState(Renderer::RenderStateKey Type, DWORD* pdwValue)
{
	*pdwValue = m_CurrentState.m_RenderStates[Type];
}

// Textures
void CDrawState::SaveTexture(DWORD dwStage, TextureBinding texture)
{
	m_TextureStack[dwStage].push_back(m_textureBindings[dwStage]);
	SetTexture(dwStage, std::move(texture));
}

void CDrawState::RestoreTexture(DWORD dwStage)
{
	SetTexture(dwStage, m_TextureStack[dwStage].back());
	m_TextureStack[dwStage].pop_back();
}

void CDrawState::SetTexture(DWORD dwStage, TextureBinding texture)
{
    assert(dwStage<8);
    m_textureBindings[dwStage]=TextureBinding(std::move(texture.source));
}



// Texture stage states
void CDrawState::SaveTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type, DWORD dwValue)
{
	m_TextureStageStateStack[dwStage][Type].push_back(m_CurrentState.m_TextureStates[dwStage][Type]);
	SetTextureStageState(dwStage, Type, dwValue);
}

void CDrawState::RestoreTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type)
{
#ifdef _DEBUG
	if (m_TextureStageStateStack[dwStage][Type].empty())
	{
		Tracef(" CDrawState::RestoreTextureStageState - This texture stage state was not saved [%u, %u]\n",
			static_cast<unsigned>(dwStage), static_cast<unsigned>(Type));
		DrawState_Assert(!" This texture stage state was not saved!");
	}
#endif _DEBUG
	SetTextureStageState(dwStage, Type, m_TextureStageStateStack[dwStage][Type].back());
	m_TextureStageStateStack[dwStage][Type].pop_back();
}

void CDrawState::SetTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type, DWORD dwValue)
{
	if (m_CurrentState.m_TextureStates[dwStage][Type] == dwValue)
		return;

	m_CurrentState.m_TextureStates[dwStage][Type] = dwValue;
}

void CDrawState::GetTextureStageState(DWORD dwStage, Renderer::TextureStageKey Type, DWORD* pdwValue)
{
	*pdwValue = m_CurrentState.m_TextureStates[dwStage][Type];
}

// Sampler states
void CDrawState::SaveSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type, DWORD dwValue)
{
	m_SamplerStateStack[dwStage][Type].push_back(m_CurrentState.m_SamplerStates[dwStage][Type]);
	SetSamplerState(dwStage, Type, dwValue);
}
void CDrawState::RestoreSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type)
{
#ifdef _DEBUG
	if (m_SamplerStateStack[dwStage][Type].empty())
	{
		Tracenf(" CDrawState::RestoreTextureStageState - This texture stage state was not saved [%d, %d]\n", dwStage, Type);
		DrawState_Assert(!" This texture stage state was not saved!");
	}
#endif _DEBUG
	SetSamplerState(dwStage, Type, m_SamplerStateStack[dwStage][Type].back());
	m_SamplerStateStack[dwStage][Type].pop_back();
}
void CDrawState::SetSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type, DWORD dwValue)
{
	if (m_CurrentState.m_SamplerStates[dwStage][Type] == dwValue)
		return;
	m_CurrentState.m_SamplerStates[dwStage][Type] = dwValue;
}
void CDrawState::GetSamplerState(DWORD dwStage, Renderer::SamplerStateKey Type, DWORD* pdwValue)
{
	*pdwValue = m_CurrentState.m_SamplerStates[dwStage][Type];
}

// Vertex Shader








// Vertex Processing


// Vertex Declaration









// Pixel Shader








// *** These states are cached, but not protected from multiple sends of the same value.
// Transform
void CDrawState::SaveTransform(Renderer::MatrixSlot Type, const Math::Matrix* pMatrix)
{
	m_TransformStack[Type].push_back(m_CurrentState.m_Matrices[Type]);
	SetTransform(Type, pMatrix);
}

void CDrawState::RestoreTransform(Renderer::MatrixSlot Type)
{
#ifdef _DEBUG
	if (m_TransformStack[Type].empty())
	{
		Tracef(" CDrawState::RestoreTransform - This transform was not saved [%u]\n", static_cast<unsigned>(Type));
		DrawState_Assert(!" This render state was not saved!");
	}
#endif _DEBUG

	SetTransform(Type, &m_TransformStack[Type].back());
	m_TransformStack[Type].pop_back();
}

// Don't cache-check the transform.  To much to do
void CDrawState::SetTransform(Renderer::MatrixSlot Type, const Math::Matrix* pMatrix)
{
    m_CurrentState.m_Matrices[Type] = *pMatrix;
}

void CDrawState::GetTransform(Renderer::MatrixSlot Type, Math::Matrix* pMatrix)
{
	*pMatrix = m_CurrentState.m_Matrices[Type];
}

void CDrawState::SetVertexConstants(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount)
{
    if(dwRegister<=96 && dwConstantCount<=96-dwRegister) memcpy(m_vertexConstants[dwRegister],pConstantData,dwConstantCount*16);
}























