// TerrainDecal.cpp: implementation of the CTerrainDecal class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "EterLib/DrawState.h"
#include "PRTerrainLib/StdAfx.h"

#include "TerrainDecal.h"
#include "MapOutdoor.h"
#include "AreaTerrain.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CTerrainDecal::CTerrainDecal(CMapOutdoor * pMapOutdoor):m_pMapOutdoor(pMapOutdoor)
{

}

CTerrainDecal::~CTerrainDecal()
{
	CDecal::Clear();
}

void CTerrainDecal::Make(Math::Vector3 v3Center, Math::Vector3 v3Normal, Math::Vector3 v3Tangent, float fWidth, float fHeight, float fDepth)
{
	Clear();
	m_v3Center = v3Center;
	m_v3Normal = v3Normal;
	
	Math::Vector3 v3Binormal;
	Math::Vec3Normalize(&v3Normal, &v3Normal);
	Math::Vec3Normalize(&v3Tangent, &v3Tangent);
	Math::Vec3Cross(&v3Binormal, &m_v3Normal, &v3Tangent);
	Math::Vec3Normalize(&v3Binormal, &v3Binormal);
	
	// Calculate boundary planes
	float fd = Math::Vec3Dot(&m_v3Center, &v3Tangent);
	m_v4LeftPlane = Math::Plane(v3Tangent.x, v3Tangent.y, v3Tangent.z, fWidth * 0.5f - fd);
	m_v4RightPlane = Math::Plane(-v3Tangent.x, -v3Tangent.y, -v3Tangent.z, fWidth * 0.5f + fd);
	
	fd = Math::Vec3Dot(&m_v3Center, &v3Binormal);
	m_v4BottomPlane = Math::Plane(v3Binormal.x, v3Binormal.y, v3Binormal.z, fHeight * 0.5f - fd);
	m_v4TopPlane = Math::Plane(-v3Binormal.x, -v3Binormal.y, -v3Binormal.z, fHeight * 0.5f + fd);
	
	fd = Math::Vec3Dot(&m_v3Center, &m_v3Normal);
	m_v4FrontPlane = Math::Plane(-m_v3Normal.x, -m_v3Normal.y, -m_v3Normal.z, fDepth + fd);
	m_v4BackPlane = Math::Plane(m_v3Normal.x, m_v3Normal.y, m_v3Normal.z, fDepth - fd);
	
	// Begin with empty mesh
	m_dwVertexCount = 0;
	m_dwPrimitiveCount = 0;
	
	// Add this point, determine which surfaces may be affected by this decal and call ClipMesh().

	float fSearchRadius = fMAX(fWidth, fHeight);// 0.75f >= sqrtf(2)/2;
	float fMinX = v3Center.x - fSearchRadius;
	float fMaxX = v3Center.x + fSearchRadius;
	float fMinY = fabs(v3Center.y) - fSearchRadius;
	float fMaxY = fabs(v3Center.y) + fSearchRadius;

	DWORD dwAffectedPrimitiveCount = 0;
	Math::Vector3 v3AffectedVertex[MAX_SEARCH_VERTICES];
	Math::Vector3 v3AffectedNormal[MAX_SEARCH_VERTICES];
	memset(v3AffectedVertex, 0, sizeof(v3AffectedVertex));
	memset(v3AffectedNormal, 0, sizeof(v3AffectedNormal));

	SearchAffectedTerrainMesh(fMinX, fMaxX, fMinY, fMaxY, &dwAffectedPrimitiveCount, v3AffectedVertex, v3AffectedNormal);
	
 	ClipMesh(dwAffectedPrimitiveCount, v3AffectedVertex, v3AffectedNormal);
	
	// Assign texture mapping coordinates
	float fOne_over_w = 1.0f / fWidth;
	float fOne_over_h = 1.0f / fHeight;
	for (DWORD dwi = 0; dwi < m_dwVertexCount; ++dwi)
	{
		Math::Vector3 v3 = m_Vertices[dwi].position - m_v3Center;
		float fu = -Math::Vec3Dot(&v3, &v3Binormal) * fOne_over_w + 0.5f;
		float fv = -Math::Vec3Dot(&v3, &v3Tangent) * fOne_over_h + 0.5f;
		m_Vertices[dwi].texCoord = Math::Vector2(fu, fv);
	}
}

/*
void CTerrainDecal::Update()
{
}
*/

void CTerrainDecal::Render()
{
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTexCoordIndex, 0);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTextureTransformFlags, Renderer::TexTransformDisable);
	DRAWSTATE.SaveSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressClamp);
	DRAWSTATE.SaveSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressClamp);
	
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpSelectArg1);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);

	CDecal::Render();
	
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTexCoordIndex);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTextureTransformFlags);
	DRAWSTATE.RestoreSamplerState(0, Renderer::SamplerAddressU);
	DRAWSTATE.RestoreSamplerState(0, Renderer::SamplerAddressV);

	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
}

void CTerrainDecal::SearchAffectedTerrainMesh(float fMinX,
											  float fMaxX,
											  float fMinY,
											  float fMaxY,
											  DWORD * pdwAffectedPrimitiveCount,
											  Math::Vector3 * pv3AffectedVertex,
											  Math::Vector3 * pv3AffectedNormal)
{
	if (!m_pMapOutdoor)
		return;
	int iMinX, iMaxX, iMinY, iMaxY;
	PR_FLOAT_TO_INT(fMinX, iMinX);
	PR_FLOAT_TO_INT(fMaxX, iMaxX);
	PR_FLOAT_TO_INT(fMinY, iMinY);
	PR_FLOAT_TO_INT(fMaxY, iMaxY);

	iMinX -= iMinX % CTerrainImpl::CELLSCALE; 
	iMaxX -= iMaxX % CTerrainImpl::CELLSCALE; 
	iMinY -= iMinY % CTerrainImpl::CELLSCALE; 
	iMaxY -= iMaxY % CTerrainImpl::CELLSCALE; 

	for(int iy = iMinY; iy <= iMaxY; iy += CTerrainImpl::CELLSCALE)
	{
		if (iy < 0)
			continue;
		WORD wTerrainNumY = iy / CTerrainImpl::TERRAIN_YSIZE;
		for(int ix = iMinX; ix <= iMaxX; ix += CTerrainImpl::CELLSCALE)
		{
			if (ix < 0)
				continue;
			WORD wTerrainNumX = ix / CTerrainImpl::TERRAIN_YSIZE;
			
			BYTE byTerrainNum;
			if (!m_pMapOutdoor->GetTerrainNumFromCoord(wTerrainNumX, wTerrainNumY, &byTerrainNum))
				continue;
			CTerrain * pTerrain;
			if (!m_pMapOutdoor->GetTerrainPointer(byTerrainNum, &pTerrain))
				continue;

			float fHeightLT = pTerrain->GetHeight(ix, iy) + m_cfDecalEpsilon;
			float fHeightRT = pTerrain->GetHeight(ix + CTerrainImpl::CELLSCALE, iy) + m_cfDecalEpsilon;
			float fHeightLB = pTerrain->GetHeight(ix, iy + CTerrainImpl::CELLSCALE) + m_cfDecalEpsilon;
			float fHeightRB = pTerrain->GetHeight(ix + CTerrainImpl::CELLSCALE, iy + CTerrainImpl::CELLSCALE) + m_cfDecalEpsilon;

			*pdwAffectedPrimitiveCount += 2;

			*pv3AffectedVertex++ = Math::Vector3((float)ix, (float)(-iy), fHeightLT);
			*pv3AffectedVertex++ = Math::Vector3((float)ix, (float)(-iy - CTerrainImpl::CELLSCALE), fHeightLB);
			*pv3AffectedVertex++ = Math::Vector3((float)(ix + CTerrainImpl::CELLSCALE), (float)(-iy), fHeightRT);
			*pv3AffectedVertex++ = Math::Vector3((float)(ix + CTerrainImpl::CELLSCALE), (float)(-iy), fHeightRT);
			*pv3AffectedVertex++ = Math::Vector3((float)ix, (float)(-iy - CTerrainImpl::CELLSCALE), fHeightLB);
			*pv3AffectedVertex++ = Math::Vector3((float)(ix + CTerrainImpl::CELLSCALE), (float)(-iy - CTerrainImpl::CELLSCALE), fHeightRB);

			*pv3AffectedNormal++ = Math::Vector3(0.0f, 0.0f, 1.0f);
			*pv3AffectedNormal++ = Math::Vector3(0.0f, 0.0f, 1.0f);
			*pv3AffectedNormal++ = Math::Vector3(0.0f, 0.0f, 1.0f);
			*pv3AffectedNormal++ = Math::Vector3(0.0f, 0.0f, 1.0f);
			*pv3AffectedNormal++ = Math::Vector3(0.0f, 0.0f, 1.0f);
			*pv3AffectedNormal++ = Math::Vector3(0.0f, 0.0f, 1.0f);
		}
	}
}
