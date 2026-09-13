///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeForestRenderer Class
//
//	(c) 2003 IDV, Inc.
//
//	This class is provided to illustrate one way to incorporate
//	SpeedTreeRT into an OpenGL application.  All of the SpeedTreeRT
//	calls that must be made on a per tree basis are done by this class.
//	Calls that apply to all trees (i.e. static SpeedTreeRT functions)
//	are made in the functions in main.cpp.
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement.
//
//      Copyright (c) 2001-2003 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		1233 Washington St. Suite 610
//		Columbia, SC 29201
//		Voice: (803) 799-1699
//		Fax:   (803) 931-0320
//		Web:   http://www.idvinc.com

#include "StdAfx.h"

#include <stdio.h>
#include "Renderer/DrawStateTypes.h"
#include "Math/Math.h"

#include "EterBase/Timer.h"
#include "EterLib/DrawState.h"
#include "EterLib/Camera.h"

#include "SpeedTreeForestRenderer.h"
#include "SpeedTreeConfig.h"
#include "TreeVertexData.h"

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeForestRenderer::CSpeedTreeForestRenderer

CSpeedTreeForestRenderer::CSpeedTreeForestRenderer()
{
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeForestRenderer::~CSpeedTreeForestRenderer

CSpeedTreeForestRenderer::~CSpeedTreeForestRenderer()
{
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeForestRenderer::InitVertexShaders


bool CSpeedTreeForestRenderer::InitializeLighting()
{

	const float c_afLightPosition[4] = { -0.707f, -0.300f, 0.707f, 0.0f };
	const float	c_afLightAmbient[4] = { 0.5f, 0.5f, 0.5f, 1.0f };
	const float	c_afLightDiffuse[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	const float	c_afLightSpecular[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	float afLight1[] =
	{
		c_afLightPosition[0], c_afLightPosition[1], c_afLightPosition[2],	// pos
		c_afLightDiffuse[0], c_afLightDiffuse[1], c_afLightDiffuse[2],		// diffuse
		c_afLightAmbient[0], c_afLightAmbient[1], c_afLightAmbient[2],		// ambient
		c_afLightSpecular[0], c_afLightSpecular[1], c_afLightSpecular[2],	// specular
		c_afLightPosition[3],												// directional flag
		1.0f, 0.0f, 0.0f													// attenuation (constant, linear, quadratic)
	};

	CSpeedTreeRT::SetNumWindMatrices(c_nNumWindMatrices);

	CSpeedTreeRT::SetLightAttributes(0, afLight1);	
	CSpeedTreeRT::SetLightState(0, true);
	return true;
}

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeForestRenderer::UploadWindMatrix

void CSpeedTreeForestRenderer::UploadWindMatrix(UINT uiLocation, const float* pMatrix) const
{
	DRAWSTATE.SetVertexConstants(uiLocation, pMatrix, 4);
}

void CSpeedTreeForestRenderer::UpdateCompundMatrix(const Math::Vector3& c_rEyeVec, const Math::Matrix& c_rmatView, const Math::Matrix& c_rmatProj)
{
    // setup composite matrix for shader
	Math::Matrix matBlend;
	Math::MatrixIdentity(&matBlend);

	Math::Matrix matBlendShader;
	Math::MatrixMultiply(&matBlendShader, &c_rmatView, &c_rmatProj);

	float afDirection[3];
	afDirection[0] = matBlendShader.m[0][2];
	afDirection[1] = matBlendShader.m[1][2];
	afDirection[2] = matBlendShader.m[2][2];
	CSpeedTreeRT::SetCamera(c_rEyeVec, afDirection);

	Math::MatrixTranspose(&matBlendShader, &matBlendShader);
	DRAWSTATE.SetVertexConstants(c_nVertexShader_CompoundMatrix, &matBlendShader, 4);
}

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeForestRenderer::Render

void CSpeedTreeForestRenderer::Render(unsigned long ulRenderBitVector)
{
    // ZiiNAN: Diligent SpeedTree rendering integration; exclude shadow/minimap passes.
    Renderer::TreeDrawScope treeScope(!(ulRenderBitVector & (Forest_RenderToShadow | Forest_RenderToMiniMap)));
	UpdateSystem(CTimer::Instance().GetCurrentSecond());

	if (m_pMainTreeMap.empty())
		return;

	if (!(ulRenderBitVector & Forest_RenderToShadow) && !(ulRenderBitVector & Forest_RenderToMiniMap))
		UpdateCompundMatrix(CCameraManager::Instance().GetCurrentCamera()->GetEye(), ms_matView, ms_matProj);

	DWORD dwLightState = DRAWSTATE.GetRenderState(Renderer::StateLighting);
	DWORD dwColorVertexState = DRAWSTATE.GetRenderState(Renderer::StateColorVertex);
	DWORD dwFogVertexMode = DRAWSTATE.GetRenderState(Renderer::StateFogVertexMode);

#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
	DRAWSTATE.SetRenderState(Renderer::StateLighting, TRUE);
#else
	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
	DRAWSTATE.SetRenderState(Renderer::StateColorVertex, TRUE);
#endif

	TTreeMap::iterator itor;
	UINT uiCount;
	
	itor = m_pMainTreeMap.begin();

	while (itor != m_pMainTreeMap.end())
	{
		auto pMainTree = (itor++)->second;
		auto ppInstances = pMainTree->GetInstances(uiCount);

		for (auto it : ppInstances)
			it->Advance();
	}

	DRAWSTATE.SetVertexConstants(c_nVertexShader_Light,	m_afLighting, 3);
	DRAWSTATE.SetVertexConstants(c_nVertexShader_Fog, m_afFog, 1);

	if (ulRenderBitVector & Forest_RenderToShadow)
	{
		//DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpDisable);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);
	}
	else
	{
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);
		DRAWSTATE.SetSamplerState(0, Renderer::SamplerMinFilter,	Renderer::FilterLinear);
		DRAWSTATE.SetSamplerState(0, Renderer::SamplerMagFilter,	Renderer::FilterLinear);
		DRAWSTATE.SetSamplerState(0, Renderer::SamplerMipFilter,	Renderer::FilterLinear);

		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpModulate);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
		DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressWrap);
		DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressWrap);
	}

	DRAWSTATE.SaveRenderState(Renderer::StateAlphaTestEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreater);
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullCw);

	// set up fog if it is enabled
	if (DRAWSTATE.GetRenderState(Renderer::StateFogEnable))
	{
		#ifdef WRAPPER_USE_GPU_WIND
			DRAWSTATE.SetRenderState(Renderer::StateFogVertexMode, Renderer::FogNone); // GPU needs to work on all cards
		#endif
	}

	// choose fixed function pipeline or custom shader for fronds and branches

	// render branches
	if (ulRenderBitVector & Forest_RenderBranches)
	{
		itor = m_pMainTreeMap.begin();

		while (itor != m_pMainTreeMap.end())
		{
			auto pMainTree = (itor++)->second;
			auto ppInstances = pMainTree->GetInstances(uiCount);
			
			pMainTree->SetupBranchForTreeType();

			for (UINT i = 0; i < uiCount; ++i)
				if (ppInstances[i]->isShow())
					ppInstances[i]->RenderBranches();
		}
	}

	// set render states
	DRAWSTATE.SetRenderState(Renderer::StateCullMode, Renderer::CullNone);

	// render fronds
	if (ulRenderBitVector & Forest_RenderFronds)
	{
		itor = m_pMainTreeMap.begin();

		while (itor != m_pMainTreeMap.end())
		{
			auto pMainTree = (itor++)->second;
			auto ppInstances = pMainTree->GetInstances(uiCount);

			pMainTree->SetupFrondForTreeType();

			for (auto it : ppInstances)
			{
				if (it->isShow())
					it->RenderFronds();
			}
		}
	}

	// render leaves
	if (ulRenderBitVector & Forest_RenderLeaves)
	{

		if (DRAWSTATE.GetRenderState(Renderer::StateFogEnable))
		{
			#if defined WRAPPER_USE_GPU_WIND || defined WRAPPER_USE_GPU_LEAF_PLACEMENT
				DRAWSTATE.SetRenderState(Renderer::StateFogVertexMode, Renderer::FogNone);
			#endif
		}

		if (ulRenderBitVector & Forest_RenderToShadow || ulRenderBitVector & Forest_RenderToMiniMap)
		{
			DRAWSTATE.SetRenderState(Renderer::StateAlphaFunc, Renderer::CompareNotEqual);
			DRAWSTATE.SaveRenderState(Renderer::StateAlphaRef, 0x00000000);
		}

		itor = m_pMainTreeMap.begin();

		while (itor != m_pMainTreeMap.end())
		{
			auto pMainTree = (itor++)->second;
			auto ppInstances = pMainTree->GetInstances(uiCount);

			pMainTree->SetupLeafForTreeType();

			for (auto it : ppInstances)
			{
				if (it->isShow())
					it->RenderLeaves();
			}
		}

		while (itor != m_pMainTreeMap.end())

		if (ulRenderBitVector & Forest_RenderToShadow || ulRenderBitVector & Forest_RenderToMiniMap)
		{
			DRAWSTATE.SetRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreater);
			DRAWSTATE.RestoreRenderState(Renderer::StateAlphaRef);
		}
	}

	// render billboards
	#ifndef WRAPPER_NO_BILLBOARD_MODE
		if (ulRenderBitVector & Forest_RenderBillboards)
		{
			DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
			DRAWSTATE.SetRenderState(Renderer::StateColorVertex, FALSE);

			itor = m_pMainTreeMap.begin();

			while (itor != m_pMainTreeMap.end())
			{
				auto pMainTree = (itor++)->second;
				auto ppInstances = pMainTree->GetInstances(uiCount);

				pMainTree->SetupLeafForTreeType();

				for (auto it : ppInstances)
				{
					if (it->isShow())
						it->RenderBillboards();
				}
			}
		}
	#endif

	DRAWSTATE.SetRenderState(Renderer::StateLighting, dwLightState);
	DRAWSTATE.SetRenderState(Renderer::StateColorVertex, dwColorVertexState);
	DRAWSTATE.SetRenderState(Renderer::StateFogVertexMode, dwFogVertexMode);

	// 셀프섀도우로 쓰는 TextureStage 1의 COLOROP와 ALPHAOP를 꺼줘야 다음 렌더링 할 놈들이
	// 제대로 나온다. (안그러면 검게 나올 가능성이..)
	if (!(ulRenderBitVector & Forest_RenderToShadow))
	{
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	}

	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaTestEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaFunc);
	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
}

