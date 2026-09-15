#include "StdAfx.h"
#include "MapOutdoor.h"

#include "EterLib/DrawState.h"

void CMapOutdoor::__RenderTerrain_RenderHardwareTransformPatch()
{
	DWORD dwFogColor;
	float fFogFarDistance;
	float fFogNearDistance;
	if (mc_pEnvironmentData)
	{
		dwFogColor=mc_pEnvironmentData->FogColor;
		fFogNearDistance=mc_pEnvironmentData->GetFogNearDistance();
		fFogFarDistance=mc_pEnvironmentData->GetFogFarDistance();
	}
	else
	{
		dwFogColor=0xffffffff;
		fFogNearDistance=5000.0f;
		fFogFarDistance=10000.0f;
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Render State & TextureStageState	

	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);
	DRAWSTATE.SaveTextureStageState(1, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
	DRAWSTATE.SaveTextureStageState(1, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);

	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaTestEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaRef, 0x00000000);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreater);

	DRAWSTATE.SaveRenderState(Renderer::StateTextureFactor, dwFogColor);

	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressWrap);
	DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressWrap);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressClamp);
	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressClamp);

	DRAWSTATE.SetBestFiltering(0);
	DRAWSTATE.SetBestFiltering(1);

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	DRAWSTATE.SetTransform(Renderer::MatrixWorld, &m_matWorldForCommonUse);

	DRAWSTATE.SaveTransform(Renderer::MatrixTexture0, &m_matWorldForCommonUse);
	DRAWSTATE.SaveTransform(Renderer::MatrixTexture1, &m_matWorldForCommonUse);

	// Render State & TextureStageState
	//////////////////////////////////////////////////////////////////////////


	m_iRenderedSplatNumSqSum = 0;
	m_iRenderedPatchNum = 0;
	m_iRenderedSplatNum = 0;
	m_RenderedTextureNumVector.clear();

	std::pair<float, long> fog_far(fFogFarDistance+1600.0f, 0);
	std::pair<float, long> fog_near(fFogNearDistance-3200.0f, 0);

	if (mc_pEnvironmentData && mc_pEnvironmentData->bDensityFog)
		fog_far.first = 1e10f;

	std::vector<std::pair<float ,long> >::iterator far_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_far);
	std::vector<std::pair<float ,long> >::iterator near_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_near);

	// NOTE: Word Editor 툴에서는 fog far보다 멀리있는 물체를 텍스쳐 없이 그리는 작업을 하지 않음
	WORD wPrimitiveCount;
	Renderer::PrimitiveTopology ePrimitiveType;

	BYTE byCUrrentLODLevel = 0;

	float fLODLevel1Distance = __GetNoFogDistance();
	float fLODLevel2Distance = __GetFogDistance();

	SelectIndexBuffer(0, &wPrimitiveCount, &ePrimitiveType);

	// MR-14: Fog update by Alaric
	// DWORD dwFogEnable = DRAWSTATE.GetRenderState(Renderer::StateFogEnable);
	// MR-14: -- END OF -- Fog update by Alaric
	std::vector<std::pair<float, long> >::iterator it = m_PatchVector.begin();

	// NOTE: 맵툴에서는 view ~ fog near 사이의 지형을 fog disabled 상태로 그리는 작업을 하지 않음.
	// MR-14: Fog update by Alaric
	// DRAWSTATE.SetRenderState(Renderer::StateFogEnable, FALSE);
	// MR-14: -- END OF -- Fog update by Alaric

	for(; it != near_it; ++it)
	{
		if (byCUrrentLODLevel == 0 && fLODLevel1Distance <= it->first)
		{
			byCUrrentLODLevel = 1;
			SelectIndexBuffer(1, &wPrimitiveCount, &ePrimitiveType);
		}
		else if (byCUrrentLODLevel == 1 && fLODLevel2Distance <= it->first)
		{
			byCUrrentLODLevel = 2;
			SelectIndexBuffer(2, &wPrimitiveCount, &ePrimitiveType);
		}
		
		__HardwareTransformPatch_RenderPatchSplat(it->second, wPrimitiveCount, ePrimitiveType);

		if (m_iRenderedSplatNum >= m_iSplatLimit)
			break;
		
 		if (m_bDrawWireFrame)
			DrawWireFrame(it->second, wPrimitiveCount, ePrimitiveType);
	}

	// MR-14: Fog update by Alaric
	// DRAWSTATE.SetRenderState(Renderer::StateFogEnable, dwFogEnable);
	// MR-14: -- END OF -- Fog update by Alaric

	if (m_iRenderedSplatNum < m_iSplatLimit)
	{
		for(it = near_it; it != far_it; ++it)
		{
			if (byCUrrentLODLevel == 0 && fLODLevel1Distance <= it->first)
			{
				byCUrrentLODLevel = 1;
				SelectIndexBuffer(1, &wPrimitiveCount, &ePrimitiveType);
			}
			else if (byCUrrentLODLevel == 1 && fLODLevel2Distance <= it->first)
			{
				byCUrrentLODLevel = 2;
				SelectIndexBuffer(2, &wPrimitiveCount, &ePrimitiveType);
			}

			__HardwareTransformPatch_RenderPatchSplat(it->second, wPrimitiveCount, ePrimitiveType);

			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;

			if (m_bDrawWireFrame)
				DrawWireFrame(it->second, wPrimitiveCount, ePrimitiveType);
		}
	}

	// MR-14: Fog update by Alaric
	// DRAWSTATE.SetRenderState(Renderer::StateFogEnable, FALSE);
	// MR-14: -- END OF -- Fog update by Alaric
	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);

	DRAWSTATE.SetTexture(0, NULL);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageTextureTransformFlags, FALSE);

	DRAWSTATE.SetTexture(1, NULL);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageTextureTransformFlags, FALSE);

	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTFactor);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);

	if (m_iRenderedSplatNum < m_iSplatLimit)
	{
		for(it = far_it; it != m_PatchVector.end(); ++it)
		{
			if (byCUrrentLODLevel == 0 && fLODLevel1Distance <= it->first)
			{
				byCUrrentLODLevel = 1;
				SelectIndexBuffer(1, &wPrimitiveCount, &ePrimitiveType);
			}
			else if (byCUrrentLODLevel == 1 && fLODLevel2Distance <= it->first)
			{
				byCUrrentLODLevel = 2;
				SelectIndexBuffer(2, &wPrimitiveCount, &ePrimitiveType);
			}

			__HardwareTransformPatch_RenderPatchNone(it->second, wPrimitiveCount, ePrimitiveType);

			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;

			if (m_bDrawWireFrame)
 				DrawWireFrame(it->second, wPrimitiveCount, ePrimitiveType);
		}
	}

	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);

	// MR-14: Fog update by Alaric
	// DRAWSTATE.SetRenderState(Renderer::StateFogEnable, dwFogEnable);
	// MR-14: -- END OF -- Fog update by Alaric
	DRAWSTATE.SetRenderState(Renderer::StateLighting, TRUE);

	std::sort(m_RenderedTextureNumVector.begin(),m_RenderedTextureNumVector.end());

	//////////////////////////////////////////////////////////////////////////
	// Render State & TextureStageState

	DRAWSTATE.RestoreRenderState(Renderer::StateTextureFactor);

	DRAWSTATE.RestoreTransform(Renderer::MatrixTexture0);
	DRAWSTATE.RestoreTransform(Renderer::MatrixTexture1);

	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTexCoordIndex);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTextureTransformFlags);
	DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTexCoordIndex);
	DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTextureTransformFlags);

	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaTestEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaRef);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaFunc);

	// Render State & TextureStageState
	//////////////////////////////////////////////////////////////////////////
}

void CMapOutdoor::__HardwareTransformPatch_RenderPatchSplat(long patchnum, WORD wPrimitiveCount, Renderer::PrimitiveTopology ePrimitiveType)
{
	assert(NULL!=m_pTerrainPatchProxyList && "__HardwareTransformPatch_RenderPatchSplat");
	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];
	
	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	CTerrain * pTerrain;
	if (!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	DWORD dwFogColor;
	if (mc_pEnvironmentData)
		dwFogColor=mc_pEnvironmentData->FogColor;
	else
		dwFogColor=0xffffffff;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	TTerrainSplatPatch & rTerrainSplatPatch = pTerrain->GetTerrainSplatPatch();
	
	Math::Matrix matTexTransform, matSplatAlphaTexTransform, matSplatColorTexTransform;
	m_matWorldForCommonUse._41 = -(float) (wCoordX * CTerrainImpl::TERRAIN_XSIZE);
	m_matWorldForCommonUse._42 = (float) (wCoordY * CTerrainImpl::TERRAIN_YSIZE);
	Math::MatrixMultiply(&matTexTransform, &m_matViewInverse, &m_matWorldForCommonUse);
	Math::MatrixMultiply(&matSplatAlphaTexTransform, &matTexTransform, &m_matSplatAlpha);
	DRAWSTATE.SetTransform(Renderer::MatrixTexture1, &matSplatAlphaTexTransform);

	Math::Matrix matTiling;
	Math::MatrixScaling(&matTiling, 1.0f/640.0f, -1.0f/640.0f, 0.0f);
	matTiling._41=0.0f;
	matTiling._42=0.0f;
	
	Math::MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &matTiling);
	DRAWSTATE.SetTransform(Renderer::MatrixTexture0, &matSplatColorTexTransform);
					
	CGraphicVertexBuffer* pkVB=pTerrainPatchProxy->HardwareTransformPatch_GetVertexBufferPtr();
	if (!pkVB)
		return;

	
	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);

	int iPrevRenderedSplatNum=m_iRenderedSplatNum;
	if(Renderer::terrainRenderer) Renderer::terrainRenderer->SetSplatVertices(nullptr,0);
	bool isFirst=true;
	for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
	{
		TTerainSplat & rSplat = rTerrainSplatPatch.Splats[j];
		
		if (!rSplat.Active)
			continue;
		
		if (rTerrainSplatPatch.PatchTileCount[sPatchNum][j] == 0)
			continue;
		
		const TTerrainTexture & rTexture = m_TextureSet.GetTexture(j);
		
		Math::MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &rTexture.m_matTransform);
		DRAWSTATE.SetTransform(Renderer::MatrixTexture0, &matSplatColorTexTransform);
		if (isFirst)
		{
			DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);
			DRAWSTATE.SetTexture(0, nullptr);
			DRAWSTATE.SetTexture(1, nullptr);
			SubmitTerrainSplat(patchnum,pTerrain,j);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
			isFirst=false;
		}
		else
		{
			DRAWSTATE.SetTexture(0, nullptr);
			DRAWSTATE.SetTexture(1, nullptr);
			SubmitTerrainSplat(patchnum,pTerrain,j);
		}

		std::vector<int>::iterator aIterator = std::find(m_RenderedTextureNumVector.begin(), m_RenderedTextureNumVector.end(), (int)j);
		if (aIterator == m_RenderedTextureNumVector.end())
			m_RenderedTextureNumVector.push_back(j);
		++m_iRenderedSplatNum;
		if (m_iRenderedSplatNum >= m_iSplatLimit)
			break;
		
	}

/*
	if (GetAsyncKeyState(VK_CAPITAL) & 0x8000)
	{
		TTerainSplat & rSplat = rTerrainSplatPatch.Splats[200];
		
		if (rSplat.Active)
		{
			const TTerrainTexture & rTexture = m_TextureSet.GetTexture(1);
			
			Math::MatrixMultiply(&matSplatColorTexTransform, &m_matViewInverse, &rTexture.m_matTransform);
			DRAWSTATE.SetTransform(Renderer::MatrixTexture0, &matSplatColorTexTransform);
			
			DRAWSTATE.SetTexture(0, NULL);
			DRAWSTATE.SetTexture(1, nullptr);
		}
	}
*/

	// 그림자
	if (m_bDrawShadow)
	{
		DRAWSTATE.SetRenderState(Renderer::StateLighting, TRUE);
		
		DRAWSTATE.SetRenderState(Renderer::StateFogColor, 0xFFFFFFFF);
		DRAWSTATE.SetRenderState(Renderer::StateSrcBlend, Renderer::BlendZero);
		DRAWSTATE.SetRenderState(Renderer::StateDestBlend, Renderer::BlendSrcColor);
				
		Math::Matrix matShadowTexTransform;
		Math::MatrixMultiply(&matShadowTexTransform, &matTexTransform, &m_matStaticShadow);

		DRAWSTATE.SetTransform(Renderer::MatrixTexture0, &matShadowTexTransform);
		DRAWSTATE.SetTexture(0, nullptr);
		
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpModulate);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2, Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);
		DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressClamp);
		DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressClamp);
		if (m_bDrawChrShadow)
		{
			DRAWSTATE.SetTransform(Renderer::MatrixTexture1, &m_matDynamicShadow);

			DRAWSTATE.SetTexture(1, nullptr);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgCurrent);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpModulate);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);
			DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressClamp);
			DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressClamp);
		}		
		else
		{
			DRAWSTATE.SetTexture(1, NULL);
		}
		
		ms_faceCount += wPrimitiveCount;
  		++m_iRenderedSplatNum;

		if (m_bDrawChrShadow)
		{
			DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
			DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
		}			

		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpModulate);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
		DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressWrap);
		DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressWrap);
		
		
		DRAWSTATE.SetRenderState(Renderer::StateSrcBlend, Renderer::BlendSrcAlpha);
		DRAWSTATE.SetRenderState(Renderer::StateDestBlend, Renderer::BlendInvSrcAlpha);
		DRAWSTATE.SetRenderState(Renderer::StateFogColor, dwFogColor);

		DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
	}
	++m_iRenderedPatchNum;

	int iCurRenderedSplatNum=m_iRenderedSplatNum-iPrevRenderedSplatNum;

	m_iRenderedSplatNumSqSum+=iCurRenderedSplatNum*iCurRenderedSplatNum;

}

void CMapOutdoor::__HardwareTransformPatch_RenderPatchNone(long patchnum, WORD wPrimitiveCount, Renderer::PrimitiveTopology ePrimitiveType)
{
	assert(NULL!=m_pTerrainPatchProxyList && "__HardwareTransformPatch_RenderPatchNone");
	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];
	
	if (!pTerrainPatchProxy->isUsed())
		return;

	CGraphicVertexBuffer* pkVB=pTerrainPatchProxy->HardwareTransformPatch_GetVertexBufferPtr();
	if (!pkVB)
		return;

	SubmitTerrainGeometry(patchnum);
}
