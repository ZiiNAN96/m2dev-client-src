#include "StdAfx.h"
#include "EterLib/SourceResourceAudit.h"
#include "MapOutdoor.h"
#include "Renderer/GraphicsConfig.h"
#include "TerrainPatch.h"
#include "TerrainQuadtree.h"

#include "EterLib/Camera.h"
#include "EterLib/DrawState.h"

struct SoftwareTransformPatch_SSplatVertex
{
	Math::Vector4 kPosition;
	DWORD		dwDiffuse;
	DWORD		dwSpecular;
	Math::Vector2 kTex1;
	Math::Vector2 kTex2;
};



void CMapOutdoor::__RenderTerrain_RenderSoftwareTransformPatch()
{	
	SoftwareTransformPatch_SRenderState kTPRS;

	DWORD dwFogEnable = DRAWSTATE.GetRenderState(Renderer::StateFogEnable);

	__SoftwareTransformPatch_ApplyRenderState();

	__SoftwareTransformPatch_BuildPipeline(kTPRS);

	std::pair<float, long> fog_far(kTPRS.m_fFogFarDistance+800.0f, 0);
	std::pair<float, long> fog_near(kTPRS.m_fFogNearDistance-3200.0f, 0);

	if (mc_pEnvironmentData && mc_pEnvironmentData->bDensityFog)
		fog_far.first = 1e10f;

	std::vector<std::pair<float ,long> >::iterator far_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_far);
	std::vector<std::pair<float ,long> >::iterator near_it = std::upper_bound(m_PatchVector.begin(),m_PatchVector.end(),fog_near);
    // Keep all visible Modern terrain textured. Classic retains its authored
    // distance fog and the old solid-colour far patch optimization.
    if(Renderer::GetGraphicsRuntimeConfig().style==Graphics::GraphicsStyle::Modern)
        near_it=far_it=m_PatchVector.end();

	WORD wPrimitiveCount;
	Renderer::PrimitiveTopology ePrimitiveType;

	BYTE byCUrrentLODLevel = 0;

	float fLODLevel1Distance = __GetNoFogDistance();
	float fLODLevel2Distance = __GetFogDistance();

	SelectIndexBuffer(0, &wPrimitiveCount, &ePrimitiveType);


	std::vector<std::pair<float, long> >::iterator it = m_PatchVector.begin();
    if(m_stableWorld&&Renderer::GetGraphicsRuntimeConfig().style==Graphics::GraphicsStyle::Modern) {
        for(const auto& entry:m_PatchVector) {
            SelectIndexBuffer(BYTE(m_pTerrainPatchProxyList[entry.second].GetStableLod()),&wPrimitiveCount,&ePrimitiveType);
            __SoftwareTransformPatch_RenderPatchSplat(kTPRS,entry.second,wPrimitiveCount,ePrimitiveType,false);
        }
        it=near_it;
    }

	for( ; it != near_it; ++it)
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
		
		__SoftwareTransformPatch_RenderPatchSplat(kTPRS, it->second, wPrimitiveCount, ePrimitiveType, false);
		if (m_iRenderedSplatNum >= m_iSplatLimit)
			break;
		
	}

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

			__SoftwareTransformPatch_RenderPatchSplat(kTPRS, it->second, wPrimitiveCount, ePrimitiveType, true);

			if (m_iRenderedSplatNum >= m_iSplatLimit)
				break;

		}
	}

	
	DRAWSTATE.SetTexture(0, NULL);
	DRAWSTATE.SetTexture(1, NULL);
	
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTFactor);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);


	if (IsFastTNL())
	{
		if (byCUrrentLODLevel != 2)
		{
			byCUrrentLODLevel = 2;
			SelectIndexBuffer(2, &wPrimitiveCount, &ePrimitiveType);
		}

		if (m_iRenderedSplatNum < m_iSplatLimit)
		{
			for(it = far_it; it != m_PatchVector.end(); ++it)
			{
				__SoftwareTransformPatch_RenderPatchNone(kTPRS, it->second, wPrimitiveCount, ePrimitiveType);

				if (m_iRenderedSplatNum >= m_iSplatLimit)
					break;

			}
		}
	}


	//////////////////////////////////////////////////////////////////////////
	// Render State & TextureStageState
	__SoftwareTransformPatch_RestoreRenderState(dwFogEnable);
}

void CMapOutdoor::__SoftwareTransformPatch_RenderPatchSplat(SoftwareTransformPatch_SRenderState& rkTPRS, long patchnum, WORD wPrimitiveCount, Renderer::PrimitiveTopology ePrimitiveType, bool isFogEnable)
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::__SoftwareTransformPatch_RenderPatchSplat");

	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];
	
	if (!pTerrainPatchProxy->isUsed())
		return;

	bool isDynamicShadow = pTerrainPatchProxy->IsIn(rkTPRS.m_v3Player, 3000.0f);

	if (!m_bDrawChrShadow)
		isDynamicShadow = false;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();

	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();

	if (0xFF == ucTerrainNum)
		return;

	CTerrain * pTerrain=pTerrainPatchProxy->terrainOwner;
	if (!pTerrain&&!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	TTerrainSplatPatch & rTerrainSplatPatch = pTerrain->GetTerrainSplatPatch();
	
	SoftwareTransformPatch_STLVertex akTransVertex[CTerrainPatch::TERRAIN_VERTEX_COUNT];
	if (!__SoftwareTransformPatch_SetTransform(rkTPRS, akTransVertex, *pTerrainPatchProxy, wCoordX, wCoordY, isFogEnable, isDynamicShadow))
		return;
	
	if(Renderer::terrainRenderer)
	{
		std::array<Renderer::TerrainSplatVertex,CTerrainPatch::TERRAIN_VERTEX_COUNT> attributes;
		for(size_t i=0;i<attributes.size();++i)
		{
			const auto& source=akTransVertex[i];
			const Math::Color color(source.dwDiffuse);
			attributes[i].diffuse={color.r,color.g,color.b,color.a};
			attributes[i].fog=float(source.dwFog>>24)/255.0f;
			attributes[i].colorUV={source.kTexTile.x,source.kTexTile.y};
			attributes[i].alphaUV={source.kTexAlpha.x,source.kTexAlpha.y};
		}
		Renderer::terrainRenderer->SetSplatVertices(attributes.data(),attributes.size());
	}
	
	if (isFogEnable)
	{
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgTFactor);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpBlendDiffuseAlpha);
	}
	else
	{
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgDiffuse);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	}

	int iPrevRenderedSplatNum=m_iRenderedSplatNum;
	
	bool isFirst=true;
	for (DWORD j = 1; j < pTerrain->GetNumTextures(); ++j)
	{
		TTerainSplat & rSplat = rTerrainSplatPatch.Splats[j];
		
		if (!rSplat.Active)
			continue;
		
		if (rTerrainSplatPatch.PatchTileCount[sPatchNum][j] == 0)
			continue;
		
		const TTerrainTexture & rTexture = m_TextureSet.GetTexture(j);
		
		if (isFirst)
		{
			DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg2);
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
		if (m_iRenderedSplatNum >= m_iSplatLimit&&!(m_stableWorld&&Renderer::GetGraphicsRuntimeConfig().style==Graphics::GraphicsStyle::Modern))
			break;
	}	

	// 그림자
	if (m_bDrawShadow)
	{	
		__SoftwareTransformPatch_ApplyStaticShadowRenderState();

		if (isDynamicShadow)
			__SoftwareTransformPatch_ApplyDynamicShadowRenderState();
		else
			__SoftwareTransformPatch_ApplyFogShadowRenderState();

		if (isFogEnable)
		{
			DRAWSTATE.SetRenderState(Renderer::StateFogEnable, TRUE);
			DRAWSTATE.SetRenderState(Renderer::StateFogColor, 0xFFFFFFFF);
			DRAWSTATE.SetTexture(0, nullptr);
			DRAWSTATE.SetRenderState(Renderer::StateFogColor, rkTPRS.m_dwFogColor);
			DRAWSTATE.SetRenderState(Renderer::StateFogEnable, FALSE);
		}
		else
		{
			DRAWSTATE.SetTexture(0, nullptr);
		}

		if (isDynamicShadow)
			__SoftwareTransformPatch_RestoreDynamicShadowRenderState();
		else
			__SoftwareTransformPatch_RestoreFogShadowRenderState();
		
		ms_faceCount += wPrimitiveCount;
  		++m_iRenderedSplatNum;

		

		__SoftwareTransformPatch_RestoreStaticShadowRenderState();
	}

	++m_iRenderedPatchNum;

	int iCurRenderedSplatNum=m_iRenderedSplatNum-iPrevRenderedSplatNum;

	m_iRenderedSplatNumSqSum+=iCurRenderedSplatNum*iCurRenderedSplatNum;
}

void CMapOutdoor::__SoftwareTransformPatch_RenderPatchNone(SoftwareTransformPatch_SRenderState& rkTPRS, long patchnum,	WORD wPrimitiveCount, Renderer::PrimitiveTopology ePrimitiveType)
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::__SoftwareTransformPatch_RenderPatchNone");

	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];
	
	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	CTerrain * pTerrain=pTerrainPatchProxy->terrainOwner;
	if (!pTerrain&&!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	SoftwareTransformPatch_SSourceVertex* akSrcVertex=pTerrainPatchProxy->SoftwareTransformPatch_GetTerrainVertexDataPtr();
	if (!akSrcVertex)
		return;
	
	float fScreenHalfWidth=rkTPRS.m_fScreenHalfWidth;
	float fScreenHalfHeight=rkTPRS.m_fScreenHalfHeight;

	Math::Matrix m4Frustum=rkTPRS.m_m4Frustum;

	SoftwareTransformPatch_STVertex akTransVertex[CTerrainPatch::TERRAIN_VERTEX_COUNT];

	Math::Vector4* akPosition=(Math::Vector4*)akTransVertex;
	Math::Vector4* pkPosition;
	for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
	{		
		pkPosition=akPosition+uIndex;
		Math::Vec3Transform(pkPosition, &akSrcVertex[uIndex].kPosition, &m4Frustum);
		pkPosition->w=1.0f/pkPosition->w;
		pkPosition->z*=pkPosition->w;
		pkPosition->y=(pkPosition->y*pkPosition->w-1.0f)*fScreenHalfHeight;	
		pkPosition->x=(pkPosition->x*pkPosition->w+1.0f)*fScreenHalfWidth;
	}
	

    SubmitTerrainGeometry(patchnum); ms_faceCount+=wPrimitiveCount;
}

void CMapOutdoor::__SoftwareTransformPatch_ApplyStaticShadowRenderState()
{
	DRAWSTATE.SetRenderState(Renderer::StateSrcBlend, Renderer::BlendZero);
	DRAWSTATE.SetRenderState(Renderer::StateDestBlend, Renderer::BlendSrcColor);
	
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressClamp);
	DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressClamp);

}

void CMapOutdoor::__SoftwareTransformPatch_ApplyDynamicShadowRenderState()
{
	DRAWSTATE.SetTexture(1, nullptr);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);

	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressClamp);
	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressClamp);
}

void CMapOutdoor::__SoftwareTransformPatch_ApplyFogShadowRenderState()
{
	DRAWSTATE.SetTexture(1, NULL);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
}
void CMapOutdoor::__SoftwareTransformPatch_RestoreStaticShadowRenderState()
{
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressWrap);
	DRAWSTATE.SetSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressWrap);
	
	DRAWSTATE.SetRenderState(Renderer::StateSrcBlend, Renderer::BlendSrcAlpha);
	DRAWSTATE.SetRenderState(Renderer::StateDestBlend, Renderer::BlendInvSrcAlpha);
}



void CMapOutdoor::__SoftwareTransformPatch_RestoreDynamicShadowRenderState()
{
	DRAWSTATE.SetTexture(1, NULL);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);

	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressClamp);
	DRAWSTATE.SetSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressClamp);
}



void CMapOutdoor::__SoftwareTransformPatch_RestoreFogShadowRenderState()
{
	DRAWSTATE.SetRenderState(Renderer::StateFogEnable, FALSE);
	
	DRAWSTATE.SetTexture(1, NULL);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpSelectArg1);
}

void CMapOutdoor::__SoftwareTransformPatch_ApplyRenderState()
{
}

void CMapOutdoor::__SoftwareTransformPatch_RestoreRenderState(DWORD dwFogEnable)
{
}

void CMapOutdoor::__SoftwareTransformPatch_BuildPipeline(SoftwareTransformPatch_SRenderState& rkTPRS)
{
	memset(&rkTPRS, 0, sizeof(rkTPRS));

	if (mc_pEnvironmentData)
	{
		rkTPRS.m_dwFogColor = mc_pEnvironmentData->FogColor;
		rkTPRS.m_fFogNearDistance = mc_pEnvironmentData->GetFogNearDistance();
		rkTPRS.m_fFogFarDistance = mc_pEnvironmentData->GetFogFarDistance();
	}
	else
	{
		rkTPRS.m_dwFogColor = 0xffffffff;
		rkTPRS.m_fFogNearDistance = 5000.0f;
		rkTPRS.m_fFogFarDistance = 10000.0f;
	}

	UINT uScreenWidth;
	UINT uScreenHeight;
	CScreen::GetBackBufferSize(&uScreenWidth, &uScreenHeight);

	rkTPRS.m_fScreenHalfWidth = +float(uScreenWidth) / 2.0f;
	rkTPRS.m_fScreenHalfHeight = -float(uScreenHeight) / 2.0f;

	DRAWSTATE.GetLight(0, &rkTPRS.m_kLight);
	DRAWSTATE.GetMaterial(&rkTPRS.m_kMtrl);

	Math::Matrix m4View;DRAWSTATE.GetTransform(Renderer::MatrixView, &m4View);
	Math::Matrix m4Proj;DRAWSTATE.GetTransform(Renderer::MatrixProjection, &m4Proj);
	
	Math::MatrixMultiply(&rkTPRS.m_m4Frustum, &m4View, &m4Proj);

	rkTPRS.m_v3Player.x = +m_v3Player.x;
	rkTPRS.m_v3Player.y = -m_v3Player.y;
	rkTPRS.m_v3Player.z = +m_v3Player.z;

	rkTPRS.m_m4Proj = m4Proj;
	rkTPRS.m_m4DynamicShadow = m_matLightView * m_matDynamicShadowScale;

	Math::Vector3 kFogNearVector;
	const auto farvv = Math::Vector3(0.0f, 0.0f, -rkTPRS.m_fFogNearDistance);
	Math::Vec3TransformCoord(&kFogNearVector, &farvv, &rkTPRS.m_m4Proj);

	Math::Vector3 kFogFarVector;
	const auto nearvv = Math::Vector3(0.0f, 0.0f, -rkTPRS.m_fFogFarDistance);
	Math::Vec3TransformCoord(&kFogFarVector, &nearvv, &rkTPRS.m_m4Proj);
	
	float fFogNear = kFogNearVector.z;
	float fFogFar = kFogFarVector.z;
	float fFogLenInv = 1.0f / (fFogFar-fFogNear);
	
	rkTPRS.m_fFogNearTransZ = fFogNear;
	rkTPRS.m_fFogFarTransZ = fFogFar;
	rkTPRS.m_fFogLenInv = fFogLenInv;

}
		
bool CMapOutdoor::__SoftwareTransformPatch_SetTransform(SoftwareTransformPatch_SRenderState& rkTPRS, SoftwareTransformPatch_STLVertex* akTransVertex, CTerrainPatchProxy& rkTerrainPatchProxy, UINT uTerrainX, UINT uTerrainY, bool isFogEnable, bool isDynamicShadow)
{
	SoftwareTransformPatch_SSourceVertex* akSrcVertex=rkTerrainPatchProxy.SoftwareTransformPatch_GetTerrainVertexDataPtr();
	if (!akSrcVertex)
		return false;
	
	rkTerrainPatchProxy.SoftwareTransformPatch_UpdateTerrainLighting(
		m_terrainLightVersion,
		rkTPRS.m_kLight, rkTPRS.m_kMtrl);
	
	Math::Vector3* pkSrcPosition;

	float fTilePatternX=+1/640.0f;
	float fTilePatternY=-1/640.0f;

	float fTerrainBaseX=-(float) (uTerrainX * CTerrainImpl::TERRAIN_XSIZE)+m_fTerrainTexCoordBase * 12.30769f;
	float fTerrainBaseY=+(float) (uTerrainY * CTerrainImpl::TERRAIN_YSIZE)+m_fTerrainTexCoordBase * 12.30769f;	

	float fScreenHalfWidth=rkTPRS.m_fScreenHalfWidth;
	float fScreenHalfHeight=rkTPRS.m_fScreenHalfHeight;

	float fAlphaPatternX=m_matSplatAlpha._11;
	float fAlphaPatternY=m_matSplatAlpha._22;
	float fAlphaBiasX=m_matSplatAlpha._41;
	float fAlphaBiasY=m_matSplatAlpha._42;	
	float fShadowPatternX=+m_fTerrainTexCoordBase * ((float) CTerrainImpl::PATCH_XSIZE / static_cast<float>(CTerrainImpl::XSIZE));		
	float fShadowPatternY=-m_fTerrainTexCoordBase * ((float) CTerrainImpl::PATCH_YSIZE / static_cast<float>(CTerrainImpl::YSIZE));

	Math::Matrix m4Frustum=rkTPRS.m_m4Frustum;
	
	if (isFogEnable)
	{
		float fFogCur;
		float fFogFar=rkTPRS.m_fFogFarTransZ;
		float fFogLenInv=rkTPRS.m_fFogLenInv;

		float fLocalX;
		float fLocalY;

		SoftwareTransformPatch_STLVertex kWorkVertex; 
		for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
		{		
			pkSrcPosition=&akSrcVertex[uIndex].kPosition;
			Math::Vec3Transform(&kWorkVertex.kPosition, pkSrcPosition, &m4Frustum);
			fLocalX=pkSrcPosition->x+fTerrainBaseX;
			fLocalY=pkSrcPosition->y+fTerrainBaseY;	
			kWorkVertex.kPosition.w=1.0f/kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.y*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.z*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x=(kWorkVertex.kPosition.x+1.0f)*fScreenHalfWidth;
			kWorkVertex.kPosition.y=(kWorkVertex.kPosition.y-1.0f)*fScreenHalfHeight;			
			kWorkVertex.dwDiffuse=akSrcVertex[uIndex].dwDiffuse;
			kWorkVertex.kTexTile.x=pkSrcPosition->x*fTilePatternX;
			kWorkVertex.kTexTile.y=pkSrcPosition->y*fTilePatternY;	
			kWorkVertex.kTexAlpha.x=fLocalX*fAlphaPatternX+fAlphaBiasX;
			kWorkVertex.kTexAlpha.y=fLocalY*fAlphaPatternY+fAlphaBiasY;
			kWorkVertex.kTexStaticShadow.x=fLocalX*fShadowPatternX;
			kWorkVertex.kTexStaticShadow.y=fLocalY*fShadowPatternY;
			kWorkVertex.kTexDynamicShadow.x=0.0f;
			kWorkVertex.kTexDynamicShadow.y=0.0f;

			fFogCur=(fFogFar-kWorkVertex.kPosition.z)*fFogLenInv;
			if (fFogCur<0.0f)
				kWorkVertex.dwFog=kWorkVertex.dwDiffuse=0x0000000|(kWorkVertex.dwDiffuse&0xffffff);
			else if (fFogCur>1.0f)
				kWorkVertex.dwFog=kWorkVertex.dwDiffuse=0xFF000000|(kWorkVertex.dwDiffuse&0xffffff);
			else
				kWorkVertex.dwFog=kWorkVertex.dwDiffuse=BYTE(255.0f*fFogCur)<<24|(kWorkVertex.dwDiffuse&0xffffff);

			*(akTransVertex+uIndex)=kWorkVertex;
		}
	}
	else
	{
		float fLocalX;
		float fLocalY;
		
		SoftwareTransformPatch_STLVertex kWorkVertex; 
		for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
		{		
			pkSrcPosition=&akSrcVertex[uIndex].kPosition;
			Math::Vec3Transform(&kWorkVertex.kPosition, pkSrcPosition, &m4Frustum);
			fLocalX=pkSrcPosition->x+fTerrainBaseX;
			fLocalY=pkSrcPosition->y+fTerrainBaseY;	
			kWorkVertex.kPosition.w=1.0f/kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.y*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.z*=kWorkVertex.kPosition.w;
			kWorkVertex.kPosition.x=(kWorkVertex.kPosition.x+1.0f)*fScreenHalfWidth;
			kWorkVertex.kPosition.y=(kWorkVertex.kPosition.y-1.0f)*fScreenHalfHeight;
			kWorkVertex.dwDiffuse=akSrcVertex[uIndex].dwDiffuse;
			kWorkVertex.dwFog=0xffffffff;
			kWorkVertex.kTexTile.x=pkSrcPosition->x*fTilePatternX;
			kWorkVertex.kTexTile.y=pkSrcPosition->y*fTilePatternY;	
			kWorkVertex.kTexAlpha.x=fLocalX*fAlphaPatternX+fAlphaBiasX;
			kWorkVertex.kTexAlpha.y=fLocalY*fAlphaPatternY+fAlphaBiasY;
			kWorkVertex.kTexStaticShadow.x=fLocalX*fShadowPatternX;
			kWorkVertex.kTexStaticShadow.y=fLocalY*fShadowPatternY;
			kWorkVertex.kTexDynamicShadow.x=0.0f;
			kWorkVertex.kTexDynamicShadow.y=0.0f;

			*(akTransVertex+uIndex)=kWorkVertex;
		}		
	}

	if (isDynamicShadow)
	{
		Math::Matrix m4DynamicShadow=rkTPRS.m_m4DynamicShadow;

		Math::Vector3 v3Shadow;
		for (UINT uIndex=0; uIndex!=CTerrainPatch::TERRAIN_VERTEX_COUNT; ++uIndex)
		{
			Math::Vec3TransformCoord(&v3Shadow, &akSrcVertex[uIndex].kPosition, &m4DynamicShadow);
			akTransVertex[uIndex].kTexDynamicShadow.x=v3Shadow.x;
			akTransVertex[uIndex].kTexDynamicShadow.y=v3Shadow.y;
		}
	}

	return true;
}
