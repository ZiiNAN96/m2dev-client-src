///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper Class
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
//

#pragma warning(disable:4786)

///////////////////////////////////////////////////////////////////////  
//	Include Files
#include "StdAfx.h"

#include <stdlib.h>
#include <stdio.h>
#include "EterBase/Debug.h"
#include "EterBase/Timer.h"
#include "EterBase/Filename.h"
#include "EterLib/ResourceManager.h"
#include "EterLib/Camera.h"
#include "EterLib/DrawState.h"

#include "SpeedTreeConfig.h"
#include "SpeedTreeForestRenderer.h"
#include "SpeedTreeWrapper.h"
#include "TreeVertexData.h"
#include "TreeRenderBridge.h" // ZiiNAN: Diligent SpeedTree rendering integration

#include <filesystem>
#include "Vegetation/VegetationRenderer.h"

using namespace std;

bool CSpeedTreeWrapper::ms_bSelfShadowOn = true;

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::CSpeedTreeWrapper
CSpeedTreeWrapper::CSpeedTreeWrapper() :
m_pSpeedTree(new CSpeedTreeRT),
m_bIsInstance(false),
m_pInstanceOf(NULL),
m_pGeometryCache(NULL),
m_usNumLeafLods(0),
m_unBranchVertexCount(0),
m_unFrondVertexCount(0),
m_pTextureInfo(NULL)
{
	// set initial position
	m_afPos[0] = m_afPos[1] = m_afPos[2] = 0.0f;
	
	m_pSpeedTree->SetWindStrength(1.0f);
	m_pSpeedTree->SetLocalMatrices(0, 4);
}



void CSpeedTreeWrapper::OnRenderPCBlocker()
{
    Renderer::TreeDrawScope treeScope(true);

	

	CSpeedTreeForestRenderer::Instance().UpdateSystem(ELTimer_GetMSec() / 1000.0f);
	
	m_pSpeedTree->SetLodLevel(1.0f);
	//Advance();
	
	CSpeedTreeForestRenderer::Instance().UpdateCompundMatrix(CCameraManager::Instance().GetCurrentCamera()->GetEye(), ms_matView, ms_matProj);

	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpModulate);

	DWORD dwLighting = DRAWSTATE.GetRenderState(Renderer::StateLighting);
	DWORD dwFogEnable = DRAWSTATE.GetRenderState(Renderer::StateFogEnable);
	DWORD dwAlphaBlendEnable = DRAWSTATE.GetRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
	DRAWSTATE.SaveRenderState(Renderer::StateColorVertex, TRUE);
    DRAWSTATE.SetRenderState(Renderer::StateAlphaBlendEnable, TRUE);
    DRAWSTATE.SaveRenderState(Renderer::StateAlphaTestEnable, TRUE);
    DRAWSTATE.SaveRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreater);
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullCw);
	DRAWSTATE.SetRenderState(Renderer::StateFogEnable, FALSE);
	
	// choose fixed function pipeline or custom shader for fronds and branches
	
// 	SetupBranchForTreeType();
	{
		// update the branch geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry);
		

#endif
		
		TextureBinding textureBinding;
		
		// set texture map
		if ((textureBinding = m_BranchImageInstance.GetTextureReference().GetTextureBinding()))
			DRAWSTATE.SetTexture(0, textureBinding);
		
		if (m_pGeometryCache->m_sBranches.m_usVertexCount > 0)
		{
			// activate the branch vertex buffer
			// set the index buffer
		}
	}

	RenderBranches();
	
	DRAWSTATE.SetTexture(0, m_CompositeImageInstance.GetTextureReference().GetTextureBinding());
	DRAWSTATE.SetRenderState(Renderer::StateCullMode, Renderer::CullNone);
	
// 	SetupFrondForTreeType();
	{
		// update the frond geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry);

#endif
		
		if (!m_CompositeImageInstance.IsEmpty())
			DRAWSTATE.SetTexture(0, m_CompositeImageInstance.GetTextureReference().GetTextureBinding());
		
		if (m_pGeometryCache->m_sFronds.m_usVertexCount > 0)
		{
			// activate the frond vertex buffer
			// set the index buffer
		}
	}
	RenderFronds();
	
	{
		
// 	SetupLeafForTreeType();
		{
			// pass leaf tables to shader
#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
			UploadLeafTables(c_nVertexShader_LeafTables);
#endif
			
			if (!m_CompositeImageInstance.IsEmpty())
				DRAWSTATE.SetTexture(0, m_CompositeImageInstance.GetTextureReference().GetTextureBinding());
		}
		RenderLeaves();
	}
	
	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
	DRAWSTATE.SetRenderState(Renderer::StateColorVertex, FALSE);
	RenderBillboards();
	
	DRAWSTATE.RestoreRenderState(Renderer::StateColorVertex);
	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaTestEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaFunc);
	DRAWSTATE.SetRenderState(Renderer::StateAlphaBlendEnable, dwAlphaBlendEnable);
	DRAWSTATE.SetRenderState(Renderer::StateLighting, dwLighting);
	DRAWSTATE.SetRenderState(Renderer::StateFogEnable, dwFogEnable);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg1);
}

void CSpeedTreeWrapper::OnRender()
{
    Renderer::TreeDrawScope treeScope(true);

	

	CSpeedTreeForestRenderer::Instance().UpdateSystem(ELTimer_GetMSec() / 1000.0f);
	
	// ï¿½Ï³ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½ LOD ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ï¿½ ï¿½ï¿½ï¿½ï¿½
	m_pSpeedTree->SetLodLevel(1.0f);
	//Advance();
	
	CSpeedTreeForestRenderer::Instance().UpdateCompundMatrix(CCameraManager::Instance().GetCurrentCamera()->GetEye(), ms_matView, ms_matProj);
	
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);
	
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressWrap);
	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressWrap);
	
	DRAWSTATE.SaveRenderState(Renderer::StateLighting, FALSE);
	DRAWSTATE.SaveRenderState(Renderer::StateColorVertex, TRUE);
    DRAWSTATE.SaveRenderState(Renderer::StateAlphaTestEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreater);
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullCw);
	DRAWSTATE.SaveRenderState(Renderer::StateFogEnable, FALSE);
	
	// choose fixed function pipeline or custom shader for fronds and branches
	
	SetupBranchForTreeType();
	RenderBranches();
	
	DRAWSTATE.SetTexture(0, m_CompositeImageInstance.GetTextureReference().GetTextureBinding());
	DRAWSTATE.SetRenderState(Renderer::StateCullMode, Renderer::CullNone);
	
	SetupFrondForTreeType();
	RenderFronds();
	
	{
		
		SetupLeafForTreeType();
		RenderLeaves();
	}
	
	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
	DRAWSTATE.SetRenderState(Renderer::StateColorVertex, FALSE);
	RenderBillboards();
	
	DRAWSTATE.RestoreRenderState(Renderer::StateLighting);
	DRAWSTATE.RestoreRenderState(Renderer::StateColorVertex);
    DRAWSTATE.RestoreRenderState(Renderer::StateAlphaTestEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaFunc);
	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
	DRAWSTATE.RestoreRenderState(Renderer::StateFogEnable);
}

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::~CSpeedTreeWrapper

CSpeedTreeWrapper::~CSpeedTreeWrapper()
{
	// if this is not an instance, clean up
	if (!m_bIsInstance)
	{
		if (m_unBranchVertexCount > 0)
		{
		}
		
		if (m_unFrondVertexCount > 0)
		{	
		}
		

		
		
		SAFE_DELETE(m_pTextureInfo);

		SAFE_DELETE(m_pGeometryCache);
	}
	
	// always delete the speedtree
	SAFE_DELETE(m_pSpeedTree);

	Clear();
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::LoadTree
bool CSpeedTreeWrapper::LoadTree(const char * pszSptFile, const BYTE * c_pbBlock, unsigned int uiBlockSize, UINT nSeed, float fSize, float fSizeVariance)
{
    ++Vegetation::statistics.referenceEntries;
    bool bSuccess = false;
	
	// directx, so allow for flipping of the texture coordinate
#ifdef WRAPPER_FLIP_T_TEXCOORD
	m_pSpeedTree->SetTextureFlip(true);
#endif
	
	// load the tree file
	if (!m_pSpeedTree->LoadTree(c_pbBlock, uiBlockSize))
	{
		if (!m_pSpeedTree->LoadTree(pszSptFile))
		{
			TraceError("SpeedTreeRT Error: %s", CSpeedTreeRT::GetCurrentError());
			return false;
		}
	}
		
	// override the lighting method stored in the spt file
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
	m_pSpeedTree->SetBranchLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
	m_pSpeedTree->SetLeafLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
	m_pSpeedTree->SetFrondLightingMethod(CSpeedTreeRT::LIGHT_DYNAMIC);
#else
	m_pSpeedTree->SetBranchLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
	m_pSpeedTree->SetLeafLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
	m_pSpeedTree->SetFrondLightingMethod(CSpeedTreeRT::LIGHT_STATIC);
#endif
	
	// set the wind method
#ifdef WRAPPER_USE_GPU_WIND
	m_pSpeedTree->SetBranchWindMethod(CSpeedTreeRT::WIND_GPU);
	m_pSpeedTree->SetLeafWindMethod(CSpeedTreeRT::WIND_GPU);
	m_pSpeedTree->SetFrondWindMethod(CSpeedTreeRT::WIND_GPU);
#endif
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->SetBranchWindMethod(CSpeedTreeRT::WIND_CPU);
	m_pSpeedTree->SetLeafWindMethod(CSpeedTreeRT::WIND_CPU);
	m_pSpeedTree->SetFrondWindMethod(CSpeedTreeRT::WIND_CPU);
#endif
#ifdef WRAPPER_USE_NO_WIND
	m_pSpeedTree->SetBranchWindMethod(CSpeedTreeRT::WIND_NONE);
	m_pSpeedTree->SetLeafWindMethod(CSpeedTreeRT::WIND_NONE);
	m_pSpeedTree->SetFrondWindMethod(CSpeedTreeRT::WIND_NONE);
#endif
	
	m_pSpeedTree->SetNumLeafRockingGroups(1);
	
	// override the size, if necessary
	if (fSize >= 0.0f && fSizeVariance >= 0.0f)
		m_pSpeedTree->SetTreeSize(fSize, fSizeVariance);
	
	// generate tree geometry
	if (m_pSpeedTree->Compute(NULL, nSeed, false))
	{
		// get the dimensions
		m_pSpeedTree->GetBoundingBox(m_afBoundingBox);
		
		// make the leaves rock in the wind
		m_pSpeedTree->SetLeafRockingState(true);
		
		// billboard setup
#ifdef WRAPPER_NO_BILLBOARD_MODE
		CSpeedTreeRT::SetDropToBillboard(false);
#else
		CSpeedTreeRT::SetDropToBillboard(true);
#endif
		
		// query & set materials
		m_cBranchMaterial.Set(m_pSpeedTree->GetBranchMaterial());
		m_cFrondMaterial.Set(m_pSpeedTree->GetFrondMaterial());
		m_cLeafMaterial.Set(m_pSpeedTree->GetLeafMaterial());
		
		// adjust lod distances
		float fHeight = m_afBoundingBox[5] - m_afBoundingBox[2];
		m_pSpeedTree->SetLodLimits(fHeight * c_fNearLodFactor, fHeight * c_fFarLodFactor);
		
		// query textures
		m_pTextureInfo = new CSpeedTreeRT::STextures;
		m_pSpeedTree->GetTextures(*m_pTextureInfo);
		
		std::filesystem::path path = pszSptFile;
		path = path.parent_path();

		auto branchTexture = path / m_pTextureInfo->m_pBranchTextureFilename;
		branchTexture.replace_extension(".dds");

		// load branch textures
		LoadTexture(branchTexture.generic_string().c_str(), m_BranchImageInstance);
		
#ifdef WRAPPER_RENDER_SELF_SHADOWS
		auto selfShadowTexture = path / m_pTextureInfo->m_pSelfShadowFilename;
		selfShadowTexture.replace_extension(".dds");

		if (m_pTextureInfo->m_pSelfShadowFilename != NULL)
			LoadTexture(selfShadowTexture.generic_string().c_str(), m_ShadowImageInstance);
#endif

		auto compositeTexture = path / m_pTextureInfo->m_pCompositeFilename;
		compositeTexture.replace_extension(".dds");

		if (m_pTextureInfo->m_pCompositeFilename)
			LoadTexture(compositeTexture.generic_string().c_str(), m_CompositeImageInstance);
		
		// setup the index and vertex buffers
		SetupBuffers();

        // ZiiNAN: Keep CPU upload data with the native model; no D3D9 buffer readback.
        TreeRenderBridge::Capture(*this,pszSptFile);
		
		// everything appeared to go well
		bSuccess = true;
	}
	else // tree failed to compute
		fprintf(stderr, "\nFatal Error, cannot compute tree [%s]\n\n", CSpeedTreeRT::GetCurrentError());
	
    return bSuccess;
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetupBuffers

void CSpeedTreeWrapper::SetupBuffers(void)
{
	// read all the geometry for highest LOD into the geometry cache (just a precaution, it's updated later)
	m_pSpeedTree->SetLodLevel(1.0f);
	
	if (m_pGeometryCache == NULL)
		m_pGeometryCache = new CSpeedTreeRT::SGeometry;
	
	m_pSpeedTree->GetGeometry(*m_pGeometryCache);
	
	// setup the buffers for each part
	SetupBranchBuffers();
	SetupFrondBuffers();
	SetupLeafBuffers();
}

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetupBranchBuffers

void CSpeedTreeWrapper::SetupBranchBuffers(void)
{
	// reference to branch structure
	CSpeedTreeRT::SGeometry::SIndexed* pBranches = &(m_pGeometryCache->m_sBranches);
	m_unBranchVertexCount = pBranches->m_usVertexCount; // we asked for a contiguous strip
	
	// check if this tree has branches
	if (m_unBranchVertexCount > 1)
	{
		
        const uint32_t unNumLodLevels = m_pSpeedTree->GetNumBranchLodLevels();
		m_branchStripOffsets.clear();
		m_branchStripLengths.clear();
		if (unNumLodLevels > 0)
			m_branchStripLengths.resize(unNumLodLevels);

		// set LOD0 for strip offsets/index buffer sizing
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry, 0);
		const uint32_t stripCount = pBranches->m_usNumStrips;
		uint32_t totalIndexCount = 0;
		if (stripCount > 0)
		{
			m_branchStripOffsets.resize(stripCount);
			for (uint32_t s = 0; s < stripCount; ++s)
			{
				m_branchStripOffsets[s] = totalIndexCount;
				totalIndexCount += pBranches->m_pStripLengths[s];
			}
		}

		for (uint32_t i = 0; i < unNumLodLevels; ++i)
		{
			m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry, i);
			auto& lengths = m_branchStripLengths[i];
			lengths.assign(stripCount, 0);
			const uint32_t lodStripCount = pBranches->m_usNumStrips;
			for (uint32_t s = 0; s < stripCount && s < lodStripCount; ++s)
			{
				lengths[s] = pBranches->m_pStripLengths[s];
			}
		}
		// set back to highest LOD for buffer fill
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry, 0);


	}
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetupFrondBuffers

void CSpeedTreeWrapper::SetupFrondBuffers(void)
{
	// reference to frond structure
	CSpeedTreeRT::SGeometry::SIndexed* pFronds = &(m_pGeometryCache->m_sFronds);
	m_unFrondVertexCount = pFronds->m_usVertexCount; // we asked for a contiguous strip
	
	// check if tree has fronds
	if (m_unFrondVertexCount > 1)
	{
		
        const uint32_t unNumLodLevels = m_pSpeedTree->GetNumFrondLodLevels();
		m_frondStripOffsets.clear();
		m_frondStripLengths.clear();
		if (unNumLodLevels > 0)
			m_frondStripLengths.resize(unNumLodLevels);

		// set LOD0 for strip offsets/index buffer sizing
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry, -1, 0);
		const uint32_t stripCount = pFronds->m_usNumStrips;
		uint32_t totalIndexCount = 0;
		if (stripCount > 0)
		{
			m_frondStripOffsets.resize(stripCount);
			for (uint32_t s = 0; s < stripCount; ++s)
			{
				m_frondStripOffsets[s] = totalIndexCount;
				totalIndexCount += pFronds->m_pStripLengths[s];
			}
		}

		for (uint32_t j = 0; j < unNumLodLevels; ++j)
		{
			m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry, -1, j);
			auto& lengths = m_frondStripLengths[j];
			lengths.assign(stripCount, 0);
			const uint32_t lodStripCount = pFronds->m_usNumStrips;
			for (uint32_t s = 0; s < stripCount && s < lodStripCount; ++s)
			{
				lengths[s] = pFronds->m_pStripLengths[s];
			}
		}
		// go back to highest LOD for buffer fill
		m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry, -1, 0);
		

	}
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetupLeafBuffers

void CSpeedTreeWrapper::SetupLeafBuffers(void)
{
    m_usNumLeafLods=m_pSpeedTree->GetNumLeafLodLevels();
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::Advance

void CSpeedTreeWrapper::Advance(void)
{
	// compute LOD level (based on distance from camera)
	m_pSpeedTree->ComputeLodLevel();
	m_pSpeedTree->SetLodLevel(1.0f);
	
	// compute wind
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->ComputeWindEffects(true, true, true);
#endif
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::MakeInstance
CSpeedTreeWrapper::SpeedTreeWrapperPtr CSpeedTreeWrapper::MakeInstance()
{
	auto spInstance = std::make_shared<CSpeedTreeWrapper>();
	
	// make an instance of this object's SpeedTree
	spInstance->m_bIsInstance = true;

	SAFE_DELETE(spInstance->m_pSpeedTree);
	spInstance->m_pSpeedTree = m_pSpeedTree->MakeInstance();
	
	if (spInstance->m_pSpeedTree)
    {
		// use the same materials
		spInstance->m_cBranchMaterial = m_cBranchMaterial;
		spInstance->m_cLeafMaterial = m_cLeafMaterial;
		spInstance->m_cFrondMaterial = m_cFrondMaterial;
		spInstance->m_CompositeImageInstance.SetImagePointer(m_CompositeImageInstance.GetGraphicImagePointer());
		spInstance->m_BranchImageInstance.SetImagePointer(m_BranchImageInstance.GetGraphicImagePointer());
		
		if (!m_ShadowImageInstance.IsEmpty())
			spInstance->m_ShadowImageInstance.SetImagePointer(m_ShadowImageInstance.GetGraphicImagePointer());
		
		spInstance->m_pTextureInfo = m_pTextureInfo;
        spInstance->m_treeRenderData = m_treeRenderData; // ZiiNAN: Existing native model sharing.
		
		// use the same geometry cache
		spInstance->m_pGeometryCache = m_pGeometryCache;
		
		// use the same buffers
		spInstance->m_branchStripOffsets = m_branchStripOffsets;
		spInstance->m_branchStripLengths = m_branchStripLengths;
		spInstance->m_unBranchVertexCount = m_unBranchVertexCount;
		
		spInstance->m_frondStripOffsets = m_frondStripOffsets;
		spInstance->m_frondStripLengths = m_frondStripLengths;
		spInstance->m_unFrondVertexCount = m_unFrondVertexCount;
		
		spInstance->m_usNumLeafLods = m_usNumLeafLods;
		
		// new stuff
		memcpy(spInstance->m_afPos, m_afPos, 3 * sizeof(float));
		memcpy(spInstance->m_afBoundingBox, m_afBoundingBox, 6 * sizeof(float));
		spInstance->m_pInstanceOf = shared_from_this();
		m_vInstances.push_back(spInstance);
    }
    else
	{
		fprintf(stderr, "SpeedTreeRT Error: %s\n", m_pSpeedTree->GetCurrentError());
		spInstance.reset();
	}
	
	return spInstance;
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::GetInstances
std::vector <CSpeedTreeWrapper::SpeedTreeWrapperPtr> CSpeedTreeWrapper::GetInstances(UINT& nCount)
{
	std::vector <SpeedTreeWrapperPtr> kResult;

	nCount = m_vInstances.size();
	if (nCount)
	{
		for (auto it : m_vInstances)
		{
			kResult.push_back(it);
		}
	}

	return kResult;
}

void CSpeedTreeWrapper::DeleteInstance(SpeedTreeWrapperPtr pInstance)
{
	auto itor = m_vInstances.begin();
	
	while (itor != m_vInstances.end())
	{
		if (*itor == pInstance)
		{
			itor = m_vInstances.erase(itor);
		}
		else
			++itor;
	}
}

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetupBranchForTreeType

void CSpeedTreeWrapper::SetupBranchForTreeType(void) const
{
#ifdef WRAPPER_USE_DYNAMIC_LIGHTING
	// set lighting material
	DRAWSTATE.SetMaterial(m_cBranchMaterial.Get());
	SetShaderConstants(m_pSpeedTree->GetBranchMaterial());
#endif
	
	// update the branch geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry);
	

#endif
	
	TextureBinding textureBinding;
	
    // set texture map
    if ((textureBinding = m_BranchImageInstance.GetTextureReference().GetTextureBinding()))
        DRAWSTATE.SetTexture(0, textureBinding);
	
	// bind shadow texture
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	if (ms_bSelfShadowOn && (textureBinding = m_ShadowImageInstance.GetTextureReference().GetTextureBinding()))
		DRAWSTATE.SetTexture(1, textureBinding);
	else
		DRAWSTATE.SetTexture(1, NULL);
#endif
	
	if (m_pGeometryCache->m_sBranches.m_usVertexCount > 0)
	{
		// activate the branch vertex buffer
		// set the index buffer
	}
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::RenderBranches

void CSpeedTreeWrapper::RenderBranches(void) const
{
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BranchGeometry);
	
	if (m_pGeometryCache->m_sBranches.m_usVertexCount > 0 && !m_branchStripLengths.empty() && !m_branchStripOffsets.empty())
	{
		const int lod = m_pGeometryCache->m_sBranches.m_nDiscreteLodLevel;
		if (lod < 0 || static_cast<size_t>(lod) >= m_branchStripLengths.size())
			return;

		PositionTree();
		
		// set alpha test value
		DRAWSTATE.SetRenderState(Renderer::StateAlphaRef, DWORD(m_pGeometryCache->m_fBranchAlphaTestValue));
		
		const auto& lengths = m_branchStripLengths[lod];
		const size_t stripCount = lengths.size() < m_branchStripOffsets.size() ? lengths.size() : m_branchStripOffsets.size();
		for (size_t s = 0; s < stripCount; ++s)
		{
			const uint16_t stripLength = lengths[s];
			if (stripLength > 2)
			{
				ms_faceCount += stripLength - 2;
                TreeRenderBridge::Draw(*this,Renderer::TreePart::Branch,lod,m_branchStripOffsets[s],stripLength);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetupFrondForTreeType

void CSpeedTreeWrapper::SetupFrondForTreeType(void) const
{
#ifdef SPEEDTREE_LIGHTING_DYNAMIC
	// set lighting material
	DRAWSTATE.SetMaterial(m_cFrondMaterial.Get());
	SetShaderConstants(m_pSpeedTree->GetFrondMaterial());
#endif
	
	// update the frond geometry for CPU wind
#ifdef WRAPPER_USE_CPU_WIND
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry);

#endif
	
	if (!m_CompositeImageInstance.IsEmpty())
		DRAWSTATE.SetTexture(0, m_CompositeImageInstance.GetTextureReference().GetTextureBinding());
	
	// bind shadow texture
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	TextureBinding textureBinding;
	
	if ((textureBinding = m_ShadowImageInstance.GetTextureReference().GetTextureBinding()))
		DRAWSTATE.SetTexture(1, textureBinding);
#endif
	
	if (m_pGeometryCache->m_sFronds.m_usVertexCount > 0)
	{
		// activate the frond vertex buffer
		// set the index buffer
	}
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::RenderFronds

void CSpeedTreeWrapper::RenderFronds(void) const
{
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_FrondGeometry);
	
	if (m_pGeometryCache->m_sFronds.m_usVertexCount > 0 && !m_frondStripLengths.empty() && !m_frondStripOffsets.empty())
	{
		const int lod = m_pGeometryCache->m_sFronds.m_nDiscreteLodLevel;
		if (lod < 0 || static_cast<size_t>(lod) >= m_frondStripLengths.size())
			return;

		PositionTree();
		
		// set alpha test value
		DRAWSTATE.SetRenderState(Renderer::StateAlphaRef, DWORD(m_pGeometryCache->m_fFrondAlphaTestValue));
		
		const auto& lengths = m_frondStripLengths[lod];
		const size_t stripCount = lengths.size() < m_frondStripOffsets.size() ? lengths.size() : m_frondStripOffsets.size();
		for (size_t s = 0; s < stripCount; ++s)
		{
			const uint16_t stripLength = lengths[s];
			if (stripLength > 2)
			{
				ms_faceCount += stripLength - 2;
                TreeRenderBridge::Draw(*this,Renderer::TreePart::Frond,lod,m_frondStripOffsets[s],stripLength);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetupLeafForTreeType

void CSpeedTreeWrapper::SetupLeafForTreeType(void) const
{
#ifdef SPEEDTREE_LIGHTING_DYNAMIC
	// set lighting material
	DRAWSTATE.SetMaterial(m_cLeafMaterial.Get());
	SetShaderConstants(m_pSpeedTree->GetLeafMaterial());
#endif
	
	// pass leaf tables to shader
#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
	UploadLeafTables(c_nVertexShader_LeafTables);
#endif
	
	if (!m_CompositeImageInstance.IsEmpty())
		DRAWSTATE.SetTexture(0, m_CompositeImageInstance.GetTextureReference().GetTextureBinding());
	
	// bind shadow texture
#ifdef WRAPPER_RENDER_SELF_SHADOWS
	DRAWSTATE.SetTexture(1, NULL);
#endif
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::UploadLeafTables

#ifdef WRAPPER_USE_GPU_LEAF_PLACEMENT
void CSpeedTreeWrapper::UploadLeafTables(UINT uiLocation) const
{
	// query leaf cluster table from RT
	UINT uiEntryCount = 0;
	const float * pTable = m_pSpeedTree->GetLeafBillboardTable(uiEntryCount);
	
	// upload for vertex shader use
	DRAWSTATE.SetVertexConstants(c_nVertexShader_LeafTables, pTable, uiEntryCount / 4);
}
#endif


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::RenderLeaves

void CSpeedTreeWrapper::RenderLeaves(void) const
{
	// update leaf geometry
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_LeafGeometry);

	if (m_usNumLeafLods == 0)
		return;

	const int maxLeafLod = static_cast<int>(m_usNumLeafLods);
	
	PositionTree();
	
	// render LODs, if needed
	for (UINT unLeafLevel = 0; unLeafLevel < 2; ++unLeafLevel)
	{
		const CSpeedTreeRT::SGeometry::SLeaf* pLeaf = (unLeafLevel == 0) ?
			&m_pGeometryCache->m_sLeaves0 : &m_pGeometryCache->m_sLeaves1;
		
		int unLod = pLeaf->m_nDiscreteLodLevel;
		
		if (unLod < 0 || unLod >= maxLeafLod || !pLeaf->m_bIsActive || pLeaf->m_usLeafCount == 0)
			continue;


		DRAWSTATE.SetRenderState(Renderer::StateAlphaRef, DWORD(pLeaf->m_fAlphaTestValue));
		
		ms_faceCount += pLeaf->m_usLeafCount * 2;
        TreeRenderBridge::Draw(*this,Renderer::TreePart::Leaf,unLod,0,pLeaf->m_usLeafCount*6);
	}
}


///////////////////////////////////////////////////////////////////////  




///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::RenderBillboards

void CSpeedTreeWrapper::RenderBillboards(void) const
{
	// render billboards in immediate mode (as close as DirectX comes to immediate mode)
#ifdef WRAPPER_BILLBOARD_MODE
	if (!m_CompositeImageInstance.IsEmpty())
		DRAWSTATE.SetTexture(0, m_CompositeImageInstance.GetTextureReference().GetTextureBinding());
	
	PositionTree();	
	
	struct SBillboardVertex 
	{
		float fX, fY, fZ;
		float fU, fV;
	};
	
	m_pSpeedTree->GetGeometry(*m_pGeometryCache, SpeedTree_BillboardGeometry);
	
	if (m_pGeometryCache->m_sBillboard0.m_bIsActive)
	{
		const float* pCoords = m_pGeometryCache->m_sBillboard0.m_pCoords;
		const float* pTexCoords = m_pGeometryCache->m_sBillboard0.m_pTexCoords;
		SBillboardVertex sVertex[4] = 
		{
			{ pCoords[0], pCoords[1], pCoords[2], pTexCoords[0], pTexCoords[1] },
			{ pCoords[3], pCoords[4], pCoords[5], pTexCoords[2], pTexCoords[3] },
			{ pCoords[6], pCoords[7], pCoords[8], pTexCoords[4], pTexCoords[5] },
			{ pCoords[9], pCoords[10], pCoords[11], pTexCoords[6], pTexCoords[7] },
		};
		
		DRAWSTATE.SetRenderState(Renderer::StateAlphaRef, DWORD(m_pGeometryCache->m_sBillboard0.m_fAlphaTestValue));
		
		ms_faceCount += 2;
        TreeRenderBridge::Billboard(*this,pCoords,pTexCoords);
	}
	
	// if tree supports 360 degree billboards, render the second
	if (m_pGeometryCache->m_sBillboard1.m_bIsActive)
	{
		const float* pCoords = m_pGeometryCache->m_sBillboard1.m_pCoords;
		const float* pTexCoords = m_pGeometryCache->m_sBillboard1.m_pTexCoords;
		SBillboardVertex sVertex[4] = 
		{
			{ pCoords[0], pCoords[1], pCoords[2], pTexCoords[0], pTexCoords[1] },
			{ pCoords[3], pCoords[4], pCoords[5], pTexCoords[2], pTexCoords[3] },
			{ pCoords[6], pCoords[7], pCoords[8], pTexCoords[4], pTexCoords[5] },
			{ pCoords[9], pCoords[10], pCoords[11], pTexCoords[6], pTexCoords[7] },
		};
		DRAWSTATE.SetRenderState(Renderer::StateAlphaRef, DWORD(m_pGeometryCache->m_sBillboard1.m_fAlphaTestValue));
		
		ms_faceCount += 2;
        TreeRenderBridge::Billboard(*this,pCoords,pTexCoords);
	}
	
#ifdef WRAPPER_RENDER_HORIZONTAL_BILLBOARD
	// render horizontal billboard (if enabled)
	if (m_pGeometryCache->m_sHorizontalBillboard.m_bIsActive)
	{	
		const float* pCoords = m_pGeometryCache->m_sHorizontalBillboard.m_pCoords;
		const float* pTexCoords = m_pGeometryCache->m_sHorizontalBillboard.m_pTexCoords;
		SBillboardVertex sVertex[4] = 
		{
			{ pCoords[0], pCoords[1], pCoords[2], pTexCoords[0], pTexCoords[1] },
			{ pCoords[3], pCoords[4], pCoords[5], pTexCoords[2], pTexCoords[3] },
			{ pCoords[6], pCoords[7], pCoords[8], pTexCoords[4], pTexCoords[5] },
			{ pCoords[9], pCoords[10], pCoords[11], pTexCoords[6], pTexCoords[7] },
		};
		DRAWSTATE.SetRenderState(Renderer::StateAlphaRef, DWORD(m_pGeometryCache->m_sHorizontalBillboard.m_fAlphaTestValue));
		
		ms_faceCount += 2;
	}
	
#endif
#endif
}

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::CleanUpMemory

void CSpeedTreeWrapper::CleanUpMemory(void)
{
	if (!m_bIsInstance)
		m_pSpeedTree->DeleteTransientData();
}

///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::PositionTree

void CSpeedTreeWrapper::PositionTree(void) const
{
	Math::Vector3 vecPosition(m_pSpeedTree->GetTreePosition());
	Math::Matrix matTranslation;
	Math::MatrixIdentity(&matTranslation);
	Math::MatrixTranslation(&matTranslation, vecPosition.x, vecPosition.y, vecPosition.z);

	// store translation for client-side transformation
	DRAWSTATE.SetTransform(Renderer::MatrixWorld, &matTranslation);

	// store translation for use in vertex shader
	Math::Vector4 vecConstant(vecPosition[0], vecPosition[1], vecPosition[2], 0.0f);
	DRAWSTATE.SetVertexConstants(c_nVertexShader_TreePos, (float*)&vecConstant, 1);
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::LoadTexture

bool CSpeedTreeWrapper::LoadTexture(const char * pFilename, CGraphicImageInstance & rImage)
{
	CResource * pResource = CResourceManager::Instance().GetResourcePointer(pFilename);
	rImage.SetImagePointer(static_cast<CGraphicImage *>(pResource));

	if (rImage.IsEmpty())
		return false;
	
	//TraceError("SpeedTreeWrapper::LoadTexture: %s", pFilename);
	return true;
}


///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeWrapper::SetShaderConstants

void CSpeedTreeWrapper::SetShaderConstants(const float* pMaterial) const
{
	const float afUsefulConstants[] = 
	{
		m_pSpeedTree->GetLeafLightingAdjustment(), 0.0f, 0.0f, 0.0f,
	};
	
	DRAWSTATE.SetVertexConstants(c_nVertexShader_LeafLightingAdjustment, afUsefulConstants, 1);
	
	const float afMaterial[] = 
	{
		pMaterial[0], pMaterial[1], pMaterial[2], 1.0f,
			pMaterial[3], pMaterial[4], pMaterial[5], 1.0f
	};
	
	DRAWSTATE.SetVertexConstants(c_nVertexShader_Material, afMaterial, 2);
}

void CSpeedTreeWrapper::SetPosition(float x, float y, float z)
{
	m_afPos[0] = x;
	m_afPos[1] = y;
	m_afPos[2] = z;
	m_pSpeedTree->SetTreePosition(x, y, z);
	CGraphicObjectInstance::SetPosition(x, y, z);
}

bool CSpeedTreeWrapper::GetBoundingSphere(Math::Vector3 & v3Center, float & fRadius)
{
	float fX, fY, fZ;
	
	fX = m_afBoundingBox[3] - m_afBoundingBox[0];
	fY = m_afBoundingBox[4] - m_afBoundingBox[1];
	fZ = m_afBoundingBox[5] - m_afBoundingBox[2];
	
	v3Center.x = 0.0f;
	v3Center.y = 0.0f;
	v3Center.z = fZ * 0.5f;
	
	fRadius = sqrtf(fX * fX + fY * fY + fZ * fZ) * 0.5f * 0.9f; // 0.9f for reduce size
	
	Math::Vector3 vec(m_pSpeedTree->GetTreePosition());
	
	v3Center+=vec;
	
	return true;
}

void CSpeedTreeWrapper::CalculateBBox()
{
	float fX, fY, fZ;
	
	fX = m_afBoundingBox[3] - m_afBoundingBox[0];
	fY = m_afBoundingBox[4] - m_afBoundingBox[1];
	fZ = m_afBoundingBox[5] - m_afBoundingBox[2];
	
	m_v3BBoxMin.x = -fX / 2.0f;
	m_v3BBoxMin.y = -fY / 2.0f;
	m_v3BBoxMin.z = 0.0f;
	m_v3BBoxMax.x = fX / 2.0f;
	m_v3BBoxMax.y = fY / 2.0f;
	m_v3BBoxMax.z = fZ;
	
	m_v4TBBox[0] = Math::Vector4(m_v3BBoxMin.x, m_v3BBoxMin.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[1] = Math::Vector4(m_v3BBoxMin.x, m_v3BBoxMax.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[2] = Math::Vector4(m_v3BBoxMax.x, m_v3BBoxMin.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[3] = Math::Vector4(m_v3BBoxMax.x, m_v3BBoxMax.y, m_v3BBoxMin.z, 1.0f);
	m_v4TBBox[4] = Math::Vector4(m_v3BBoxMin.x, m_v3BBoxMin.y, m_v3BBoxMax.z, 1.0f);
	m_v4TBBox[5] = Math::Vector4(m_v3BBoxMin.x, m_v3BBoxMax.y, m_v3BBoxMax.z, 1.0f);
	m_v4TBBox[6] = Math::Vector4(m_v3BBoxMax.x, m_v3BBoxMin.y, m_v3BBoxMax.z, 1.0f);
	m_v4TBBox[7] = Math::Vector4(m_v3BBoxMax.x, m_v3BBoxMax.y, m_v3BBoxMax.z, 1.0f);
	
	const Math::Matrix & c_rmatTransform = GetTransform();
	
	for (DWORD i = 0; i < 8; ++i)
	{
		Math::Vec4Transform(&m_v4TBBox[i], &m_v4TBBox[i], &c_rmatTransform);
		if (0 == i)
		{
			m_v3TBBoxMin.x = m_v4TBBox[i].x;
			m_v3TBBoxMin.y = m_v4TBBox[i].y;
			m_v3TBBoxMin.z = m_v4TBBox[i].z;
			m_v3TBBoxMax.x = m_v4TBBox[i].x;
			m_v3TBBoxMax.y = m_v4TBBox[i].y;
			m_v3TBBoxMax.z = m_v4TBBox[i].z;
		}
		else
		{
			if (m_v3TBBoxMin.x > m_v4TBBox[i].x)
				m_v3TBBoxMin.x = m_v4TBBox[i].x;
			if (m_v3TBBoxMax.x < m_v4TBBox[i].x)
				m_v3TBBoxMax.x = m_v4TBBox[i].x;
			if (m_v3TBBoxMin.y > m_v4TBBox[i].y)
				m_v3TBBoxMin.y = m_v4TBBox[i].y;
			if (m_v3TBBoxMax.y < m_v4TBBox[i].y)
				m_v3TBBoxMax.y = m_v4TBBox[i].y;
			if (m_v3TBBoxMin.z > m_v4TBBox[i].z)
				m_v3TBBoxMin.z = m_v4TBBox[i].z;
			if (m_v3TBBoxMax.z < m_v4TBBox[i].z)
				m_v3TBBoxMax.z = m_v4TBBox[i].z;
		}
	}	
}

// collision detection routines
UINT CSpeedTreeWrapper::GetCollisionObjectCount()
{
	assert(m_pSpeedTree);
	return m_pSpeedTree->GetCollisionObjectCount();
}

void CSpeedTreeWrapper::GetCollisionObject(UINT nIndex, CSpeedTreeRT::ECollisionObjectType& eType, float* pPosition, float* pDimensions)
{
	assert(m_pSpeedTree);
	m_pSpeedTree->GetCollisionObject(nIndex, eType, pPosition, pDimensions);
}


const float * CSpeedTreeWrapper::GetPosition()
{
	return m_afPos;
}

void CSpeedTreeWrapper::GetTreeSize(float & r_fSize, float & r_fVariance)
{
	m_pSpeedTree->GetTreeSize(r_fSize, r_fVariance);
}

// pscdVector may be null
void CSpeedTreeWrapper::OnUpdateCollisionData(const CStaticCollisionDataVector * /*pscdVector*/)
{
	Math::Matrix mat;
	Math::MatrixTranslation(&mat, m_afPos[0], m_afPos[1], m_afPos[2]);
	
	/////
	for (UINT i = 0; i < GetCollisionObjectCount(); ++i)
	{
		CSpeedTreeRT::ECollisionObjectType ObjectType;
		CStaticCollisionData CollisionData;
		
		GetCollisionObject(i, ObjectType, (float * )&CollisionData.v3Position, CollisionData.fDimensions);
		
		if (ObjectType == CSpeedTreeRT::CO_BOX)
			continue;
		
		switch(ObjectType)
		{
		case CSpeedTreeRT::CO_SPHERE:
			CollisionData.dwType = COLLISION_TYPE_SPHERE;
			CollisionData.fDimensions[0] = CollisionData.fDimensions[0] /** fSizeRatio*/;
			//AddCollision(&CollisionData);
			break;
			
		case CSpeedTreeRT::CO_CYLINDER:
			CollisionData.dwType = COLLISION_TYPE_CYLINDER;
			CollisionData.fDimensions[0] = CollisionData.fDimensions[0] /** fSizeRatio*/;
			CollisionData.fDimensions[1] = CollisionData.fDimensions[1] /** fSizeRatio*/;
			//AddCollision(&CollisionData);
			break;
			
			/*case CSpeedTreeRT::CO_BOX:
			break;*/
		}
		AddCollision(&CollisionData, &mat);
	}
}

