#include "StdAfx.h"
#include "SpeedTreeLib/TreeRenderBridge.h" // ZiiNAN: Existing tree camera-blocker texture.
#include "StaticObjectBridge.h"
#include "MapOutdoor.h"
#include "TerrainPatch.h"
#include "AreaTerrain.h"
#include "TerrainQuadtree.h"

#include "EterLib/Camera.h"
#include "EterLib/DrawState.h"
#include "EterLib/MaterialStateSnapshot.h"
#include "EterLib/StaticObjectTextureLoader.h"


#define MAX_RENDER_SPALT 150

CArea::TCRCWithNumberVector m_dwRenderedCRCWithNumberVector;

CMapOutdoor::TTerrainNumVector CMapOutdoor::FSortPatchDrawStructWithTerrainNum::m_TerrainNumVector;

void CMapOutdoor::RenderTerrain()
{
	if (!IsVisiblePart(PART_TERRAIN))
		return;

	if (!m_bSettingTerrainVisible)
		return;

	// Inserted by levites
	if (!m_pTerrainPatchProxyList)
		return;

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	auto vv = ms_matView * ms_matProj;
	BuildViewFrustum(vv);

	Math::Vector3 v3Eye = pCamera->GetEye();
	m_fXforDistanceCaculation = -v3Eye.x;
	m_fYforDistanceCaculation = -v3Eye.y;
	
	//////////////////////////////////////////////////////////////////////////
	// Push
	m_PatchVector.clear();
	m_PatchVector.reserve(256);  // Pre-allocate to avoid reallocations

	__RenderTerrain_RecurseRenderQuadTree(m_pRootNode);
	
	// 거리순 정렬
	std::sort(m_PatchVector.begin(),m_PatchVector.end());

	if (Renderer::terrainRenderer)
	{
		Math::Matrix world, view, projection;
		if (CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE)
			Math::MatrixIdentity(&world); // STP applies View*Projection to world-space positions.
		else
		{
			world = m_matWorldForCommonUse;
			world._41 = world._42 = 0.0f; // Same world set by HTP before its patch loop.
		}
		DRAWSTATE.GetTransform(Renderer::MatrixView, &view);
		DRAWSTATE.GetTransform(Renderer::MatrixProjection, &projection);
		Renderer::TerrainMatrices matrices;
		memcpy(matrices.world.data(), &world, sizeof(world));
		memcpy(matrices.view.data(), &view, sizeof(view));
		memcpy(matrices.projection.data(), &projection, sizeof(projection));
		const bool statesMatch = DRAWSTATE.GetRenderState(Renderer::StateCullMode) == Renderer::CullCw &&
			DRAWSTATE.GetRenderState(Renderer::StateZEnable) == TRUE &&
			DRAWSTATE.GetRenderState(Renderer::StateZWriteEnable) == TRUE &&
			DRAWSTATE.GetRenderState(Renderer::StateZFunc) == Renderer::CompareLessEqual;
		Renderer::terrainRenderer->BeginTerrain(matrices, statesMatch);
	}

	// 그리기 위한 벡터 세팅
	if (CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE)
		__RenderTerrain_RenderSoftwareTransformPatch();
	else
		__RenderTerrain_RenderHardwareTransformPatch();
}

void CMapOutdoor::__RenderTerrain_RecurseRenderQuadTree(CTerrainQuadtreeNode *Node, bool bCullCheckNeed)
{
	if (bCullCheckNeed)
	{
		switch (__RenderTerrain_RecurseRenderQuadTree_CheckBoundingCircle(Node->center, Node->radius))
		{
			case VIEW_ALL:
				// all child nodes need not cull check
				bCullCheckNeed = false;
				break;
			case VIEW_PART:
				break;
			case VIEW_NONE:
				// no need to render
				return;
		}
		// if no need cull check more
		// -> bCullCheckNeed = false;
	}
	
	if (Node->Size == 1)
	{
		Math::Vector3 v3Center = Node->center;
		float fDistance = fMAX(fabs(v3Center.x + m_fXforDistanceCaculation), fabs(-v3Center.y + m_fYforDistanceCaculation));
		__RenderTerrain_AppendPatch(v3Center, fDistance, Node->PatchNum);
	}
	else
	{
		if (Node->NW_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->NW_Node, bCullCheckNeed);
		if (Node->NE_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->NE_Node, bCullCheckNeed);
		if (Node->SW_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->SW_Node, bCullCheckNeed);
		if (Node->SE_Node != NULL)
			__RenderTerrain_RecurseRenderQuadTree(Node->SE_Node, bCullCheckNeed);
	}
}

int	CMapOutdoor::__RenderTerrain_RecurseRenderQuadTree_CheckBoundingCircle(const Math::Vector3 & c_v3Center, const float & c_fRadius)
{
	const int count = 6;

	Math::Vector3 center = c_v3Center;
	center.y = -center.y;

	int i;

	float distance[count];
	for(i = 0; i < count; ++i)
	{
		distance[i] = Math::PlaneDotCoord(&m_plane[i], &center);
		if (distance[i] <= -c_fRadius) 
			return VIEW_NONE;
	}

	for(i = 0; i < count;++i)
	{
		if (distance[i] <= c_fRadius) 
			return VIEW_PART;
	}
	
	return VIEW_ALL;
}

void CMapOutdoor::__RenderTerrain_AppendPatch(const Math::Vector3& c_rv3Center, float fDistance, long lPatchNum)
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::__RenderTerrain_AppendPatch");
	if (!m_pTerrainPatchProxyList[lPatchNum].isUsed())
		return;

	m_pTerrainPatchProxyList[lPatchNum].SetCenterPosition(c_rv3Center);
	m_PatchVector.push_back(std::make_pair(fDistance, lPatchNum));
}

void CMapOutdoor::ApplyLight(std::uintptr_t version, const Renderer::LightValues& c_rkLight)
{
	m_terrainLightVersion=version;
	DRAWSTATE.SetLight(0, &c_rkLight);
}

// 2004. 2. 17. myevan. 모든 부분을 보이게 초기화 한다
void CMapOutdoor::InitializeVisibleParts()
{
	m_dwVisiblePartFlags=0xffffffff;
}

// 2004. 2. 17. myevan. 특정 부분을 보이게 하거나 감추는 함수
void CMapOutdoor::SetVisiblePart(int ePart, bool isVisible)
{
	DWORD dwMask=(1<<ePart);
	if (isVisible)
	{
		m_dwVisiblePartFlags|=dwMask;
	}	
	else
	{
		DWORD dwReverseMask=~dwMask;
		m_dwVisiblePartFlags&=dwReverseMask;
	}
}

// 2004. 2. 17. myevan. 특정 부분이 보이는지 알아내는 함수
bool CMapOutdoor::IsVisiblePart(int ePart)
{
	DWORD dwMask=(1<<ePart);
	if (dwMask & m_dwVisiblePartFlags)
		return true;

	return false;
}

// Splat 개수 제한
void CMapOutdoor::SetSplatLimit(int iSplatNum)
{
	m_iSplatLimit = iSplatNum;
}

std::vector<int> & CMapOutdoor::GetRenderedSplatNum(int * piPatch, int * piSplat, float * pfSplatRatio)
{	
	*piPatch = m_iRenderedPatchNum;
	*piSplat = m_iRenderedSplatNum;
	*pfSplatRatio = m_iRenderedSplatNumSqSum/float(m_iRenderedPatchNum);

	return m_RenderedTextureNumVector;
}

CArea::TCRCWithNumberVector & CMapOutdoor::GetRenderedGraphicThingInstanceNum(DWORD * pdwGraphicThingInstanceNum, DWORD * pdwCRCNum)
{
	*pdwGraphicThingInstanceNum = m_dwRenderedGraphicThingInstanceNum;
	*pdwCRCNum = m_dwRenderedCRCNum;

	return m_dwRenderedCRCWithNumberVector;
}

void CMapOutdoor::RenderBeforeLensFlare()
{
	m_LensFlare.DrawBeforeFlare();	

	if (!mc_pEnvironmentData)
	{
		TraceError("CMapOutdoor::RenderBeforeLensFlare mc_pEnvironmentData is NULL");
		return;
	}
	
	m_LensFlare.Compute(mc_pEnvironmentData->DirLights[ENV_DIRLIGHT_BACKGROUND].Direction);
}

void CMapOutdoor::RenderAfterLensFlare()
{
	m_LensFlare.AdjustBrightness();
	m_LensFlare.DrawFlare();
}

void CMapOutdoor::RenderCollision()
{
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (GetAreaPointer(i, &pArea))
			pArea->RenderCollision();
	}
}

void CMapOutdoor::RenderScreenFiltering()
{
	m_ScreenFilter.Render();
}

void CMapOutdoor::RenderSky()
{
	if (IsVisiblePart(PART_SKY))
		m_SkyBox.Render();
}

void CMapOutdoor::RenderCloud()
{
	if (IsVisiblePart(PART_CLOUD))
		m_SkyBox.RenderCloud();
}

void CMapOutdoor::RenderTree()
{
	if (IsVisiblePart(PART_TREE))
		CSpeedTreeForestRenderer::Instance().Render();
}

void CMapOutdoor::SetInverseViewAndDynamicShaodwMatrices()
{
	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();

	if (!pCamera)
		return;

	m_matViewInverse = pCamera->GetInverseViewMatrix();
	
	Math::Vector3 v3Target = pCamera->GetTarget();

	Math::Vector3 v3LightEye(v3Target.x - 1.732f * 1250.0f,
						   v3Target.y - 1250.0f,
						   v3Target.z + 2.0f * 1.732f * 1250.0f);

	const auto vv = Math::Vector3(0.0f, 0.0f, 1.0f);
	Math::MatrixLookAtRH(&m_matLightView, &v3LightEye, &v3Target, &vv);
	m_matDynamicShadow = m_matViewInverse * m_matLightView * m_matDynamicShadowScale;
}

void CMapOutdoor::OnRender()
{
#ifdef __PERFORMANCE_CHECKER__
	DWORD t1=ELTimer_GetMSec();
	SetInverseViewAndDynamicShaodwMatrices();

	SetBlendOperation();
	DWORD t2=ELTimer_GetMSec();
	RenderArea();
	DWORD t3=ELTimer_GetMSec();
	if (!m_bEnableTerrainOnlyForHeight)
		RenderTerrain();
	DWORD t4=ELTimer_GetMSec();
	RenderTree();
	DWORD t5=ELTimer_GetMSec();
	DWORD tEnd=ELTimer_GetMSec();

	if (tEnd-t1<7)
		return;

	static FILE* fp=fopen("perf_map_render.txt", "w");
 	fprintf(fp, "MAP.Total %d (Time %d)\n", tEnd-t1, ELTimer_GetMSec());
	fprintf(fp, "MAP.ENV %d\n", t2-t1);
	fprintf(fp, "MAP.OBJ %d\n", t3-t2);
	fprintf(fp, "MAP.TRN %d\n", t4-t3);
	fprintf(fp, "MAP.TRE %d\n", t5-t4);

#else
	SetInverseViewAndDynamicShaodwMatrices();

	SetBlendOperation();
	RenderArea();
	RenderTree();
	if (!m_bEnableTerrainOnlyForHeight)
		RenderTerrain();
	RenderBlendArea();
#endif
}

struct FAreaRenderShadow
{
	void operator () (CGraphicObjectInstance * pInstance)
	{
		pInstance->RenderShadow();
		if (auto* thing = dynamic_cast<CGraphicThingInstance*>(pInstance))
			SubmitStaticMapObject(*thing, StaticMapObjectPass::ShadowReceiver);
		pInstance->Hide();
	}
};

struct FPCBlockerHide
{
	void operator () (CGraphicObjectInstance * pInstance)
	{
		pInstance->Hide();
	}
};

struct FRenderPCBlocker
{
	CGraphicImage* cameraAlpha;
	void operator () (CGraphicObjectInstance * pInstance)
	{
        TreeCameraMaskScope treeMask(cameraAlpha);
		pInstance->Show();
		CGraphicThingInstance* pThingInstance = dynamic_cast <CGraphicThingInstance*> (pInstance);
		if (pThingInstance != NULL)
		{
			if (pThingInstance->HaveBlendThing())
			{
				DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);
                if(!DrawSpecialMapObject(*pThingInstance,true)) pThingInstance->BlendRender();
				return;
			}
		}
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg1);

        if(pThingInstance && DrawSpecialMapObject(*pThingInstance,false,cameraAlpha)) return;
		pInstance->RenderPCBlocker();
		if (pThingInstance)
			SubmitStaticMapObject(*pThingInstance, StaticMapObjectPass::CameraBlocker, cameraAlpha);
	}
};

void CMapOutdoor::RenderEffect()
{
	if (!IsVisiblePart(PART_OBJECT))
		return;
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (GetAreaPointer(i, &pArea))
		{
			pArea->RenderEffect();
		}
	}
}

struct CMapOutdoor_LessThingInstancePtrRenderOrder
{
	bool operator() (CGraphicThingInstance* pkLeft, CGraphicThingInstance* pkRight)
	{
		//TODO : Camera위치기반으로 소팅
		CCamera * pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
		const Math::Vector3 & c_rv3CameraPos = pCurrentCamera->GetEye();
		const Math::Vector3 & c_v3LeftPos  = pkLeft->GetPosition();
		const Math::Vector3 & c_v3RightPos = pkRight->GetPosition();
		const auto vv = Math::Vector3(c_rv3CameraPos - c_v3RightPos);
		const auto vv2 = Math::Vector3(c_rv3CameraPos - c_v3LeftPos);
		
		return Math::Vec3LengthSq(&vv2) < Math::Vec3LengthSq(&vv);
	}
};

struct CMapOutdoor_FOpaqueThingInstanceRender
{
	inline void operator () (CGraphicThingInstance * pkThingInst)
	{
        if(DrawSpecialMapObject(*pkThingInst,false)) return;
		pkThingInst->Render();
        SubmitStaticMapObject(*pkThingInst);
	}
};
struct CMapOutdoor_FBlendThingInstanceRender
{
	inline void operator () (CGraphicThingInstance * pkThingInst)
	{
        if(!DrawSpecialMapObject(*pkThingInst,true)) pkThingInst->BlendRender();
	}
};

void CMapOutdoor::RenderArea(bool bRenderAmbience)
{
	if (!IsVisiblePart(PART_OBJECT))
		return;

	m_dwRenderedCRCNum = 0;
	m_dwRenderedGraphicThingInstanceNum = 0;
	m_dwRenderedCRCWithNumberVector.clear();

	// NOTE - 20041201.levites.던젼 그림자 추가
	for (int j = 0; j < AROUND_AREA_NUM; ++j)
	{
		CArea * pArea;
		if (GetAreaPointer(j, &pArea))
		{
			pArea->RenderDungeon();
		}
	}

	BeginStaticMapObjects(m_bDrawShadow && m_bDrawChrShadow);
	// PCBlocker
	std::for_each(m_PCBlockerVector.begin(), m_PCBlockerVector.end(), FPCBlockerHide());

	// Shadow Receiver
	if (m_bDrawShadow && m_bDrawChrShadow)
	{
		if (mc_pEnvironmentData != NULL)
			DRAWSTATE.SetRenderState(Renderer::StateFogColor, 0xFFFFFFFF);

		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgDiffuse);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpModulate);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
		DRAWSTATE.SaveTextureStageState(1, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
		DRAWSTATE.SaveTextureStageState(1, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);

		// Transform
		DRAWSTATE.SaveTransform(Renderer::MatrixTexture1, &m_matDynamicShadow);
		DRAWSTATE.SetTexture(1, nullptr);

		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpModulate);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);
		DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressBorder);
		DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressBorder);
		DRAWSTATE.SaveSamplerState(1, Renderer::SamplerBorderColor, 0xFFFFFFFF);

		std::for_each(m_ShadowReceiverVector.begin(), m_ShadowReceiverVector.end(), FAreaRenderShadow());

		DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTexCoordIndex);
		DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTextureTransformFlags);
		DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressU);
		DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressV);
		DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerBorderColor);

		DRAWSTATE.RestoreTransform(Renderer::MatrixTexture1);

		if (mc_pEnvironmentData != NULL)
			DRAWSTATE.SetRenderState(Renderer::StateFogColor, mc_pEnvironmentData->FogColor);
	}

	DRAWSTATE.SaveRenderState(Renderer::StateZWriteEnable, TRUE);

	bool m_isDisableSortRendering=false;

	if (m_isDisableSortRendering)
	{
		for (int i = 0; i < AROUND_AREA_NUM; ++i)
		{
			CArea * pArea;
			if (GetAreaPointer(i, &pArea))
			{
				pArea->Render();

				m_dwRenderedCRCNum += pArea->DEBUG_GetRenderedCRCNum();
				m_dwRenderedGraphicThingInstanceNum += pArea->DEBUG_GetRenderedGrapphicThingInstanceNum();

				CArea::TCRCWithNumberVector & rCRCWithNumberVector = pArea->DEBUG_GetRenderedCRCWithNumVector();

				CArea::TCRCWithNumberVector::iterator aIterator = rCRCWithNumberVector.begin();
				while (aIterator != rCRCWithNumberVector.end())
				{
					DWORD dwCRC = (*aIterator++).dwCRC;

					CArea::TCRCWithNumberVector::iterator aCRCWithNumberVectorIterator = 
						std::find_if(m_dwRenderedCRCWithNumberVector.begin(), m_dwRenderedCRCWithNumberVector.end(), CArea::FFindIfCRC(dwCRC));

					if ( m_dwRenderedCRCWithNumberVector.end() == aCRCWithNumberVectorIterator)
					{
						CArea::TCRCWithNumber aCRCWithNumber;
						aCRCWithNumber.dwCRC = dwCRC;
						aCRCWithNumber.dwNumber = 1;
						m_dwRenderedCRCWithNumberVector.push_back(aCRCWithNumber);
					}
					else
					{
						CArea::TCRCWithNumber & rCRCWithNumber = *aCRCWithNumberVectorIterator;
						rCRCWithNumber.dwNumber += 1;
					}
				}
			}
		}
	
		std::sort(m_dwRenderedCRCWithNumberVector.begin(), m_dwRenderedCRCWithNumberVector.end(), CArea::CRCNumComp());
	}
	else
	{
		static std::vector<CGraphicThingInstance*> s_kVct_pkOpaqueThingInstSort;
		s_kVct_pkOpaqueThingInstSort.clear();
		s_kVct_pkOpaqueThingInstSort.reserve(512);  // Pre-allocate to avoid reallocations

		for (int i = 0; i < AROUND_AREA_NUM; ++i)
		{
			CArea * pArea;
			if (GetAreaPointer(i, &pArea))
			{
				pArea->CollectRenderingObject(s_kVct_pkOpaqueThingInstSort);
			}

		}

		std::sort(s_kVct_pkOpaqueThingInstSort.begin(), s_kVct_pkOpaqueThingInstSort.end(), CMapOutdoor_LessThingInstancePtrRenderOrder());
		std::for_each(s_kVct_pkOpaqueThingInstSort.begin(), s_kVct_pkOpaqueThingInstSort.end(), CMapOutdoor_FOpaqueThingInstanceRender());
	}

	DRAWSTATE.RestoreRenderState(Renderer::StateZWriteEnable);

	// Shadow Receiver
	if (m_bDrawShadow && m_bDrawChrShadow)
	{
		std::for_each(m_ShadowReceiverVector.begin(), m_ShadowReceiverVector.end(), std::mem_fn(&CGraphicObjectInstance::Show));
	}
}

void CMapOutdoor::RenderBlendArea()
{
	if (!IsVisiblePart(PART_OBJECT))
		return;

	static std::vector<CGraphicThingInstance*> s_kVct_pkBlendThingInstSort;
	s_kVct_pkBlendThingInstSort.clear();
	s_kVct_pkBlendThingInstSort.reserve(256);  // Pre-allocate to avoid reallocations

	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (GetAreaPointer(i, &pArea))
		{
			pArea->CollectBlendRenderingObject(s_kVct_pkBlendThingInstSort);
		}
	}

	if (s_kVct_pkBlendThingInstSort.size() != 0)
	{

		
		//DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
		//DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgDiffuse);
		//DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpModulate);
		//DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
		//DRAWSTATE.SaveTextureStageState(1, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
		//DRAWSTATE.SaveTextureStageState(1, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);

		//// Transform
		//DRAWSTATE.SaveTransform(Renderer::MatrixTexture1, &m_matDynamicShadow);
		//DRAWSTATE.SetTexture(1, nullptr);

		//DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
		//DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgCurrent);
		//DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpModulate);
		//DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);

		////std::for_each(m_ShadowReceiverVector.begin(), m_ShadowReceiverVector.end(), FAreaRenderShadow());

		//DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTexCoordIndex);
		//DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTextureTransformFlags);

		//DRAWSTATE.RestoreTransform(Renderer::MatrixTexture1);


		std::sort(s_kVct_pkBlendThingInstSort.begin(), s_kVct_pkBlendThingInstSort.end(), CMapOutdoor_LessThingInstancePtrRenderOrder());

		DRAWSTATE.SaveRenderState(Renderer::StateZWriteEnable, TRUE);
		DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
		DRAWSTATE.SaveRenderState(Renderer::StateSrcBlend, Renderer::BlendSrcAlpha);
		DRAWSTATE.SaveRenderState(Renderer::StateDestBlend, Renderer::BlendInvSrcAlpha);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg1);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgDiffuse);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2, Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);

		std::for_each(s_kVct_pkBlendThingInstSort.begin(), s_kVct_pkBlendThingInstSort.end(), CMapOutdoor_FBlendThingInstanceRender());

		DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
		DRAWSTATE.RestoreRenderState(Renderer::StateSrcBlend);
		DRAWSTATE.RestoreRenderState(Renderer::StateDestBlend);
		DRAWSTATE.RestoreRenderState(Renderer::StateZWriteEnable);
	}
}
void CMapOutdoor::RenderDungeon()
{
	for (int i = 0; i < AROUND_AREA_NUM; ++i)
	{
		CArea * pArea;
		if (!GetAreaPointer(i, &pArea))
			continue;
		pArea->RenderDungeon();
	}
}

void CMapOutdoor::RenderPCBlocker()
{
	// PCBlocker
	if (m_PCBlockerVector.size() != 0)
	{
		DRAWSTATE.SetTexture(0, NULL);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpSelectArg1);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,	Renderer::TextureOpSelectArg1);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,	Renderer::TextureOpDisable);

		DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
		DRAWSTATE.SaveTextureStageState(1, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
		DRAWSTATE.SaveTextureStageState(1, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpSelectArg1);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg1);
		DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressU,	Renderer::AddressClamp);
		DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressV,	Renderer::AddressClamp);

		DRAWSTATE.SaveTransform(Renderer::MatrixTexture1, &m_matBuildingTransparent);
		DRAWSTATE.SetTexture(1, m_BuildingTransparentImageInstance.GetTexturePointer()->GetTextureBinding());

		std::for_each(m_PCBlockerVector.begin(), m_PCBlockerVector.end(), FRenderPCBlocker{m_BuildingTransparentImageInstance.GetGraphicImagePointer()});

		DRAWSTATE.SetTexture(1, NULL);
		DRAWSTATE.RestoreTransform(Renderer::MatrixTexture1);

		DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTexCoordIndex);
		DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTextureTransformFlags);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
		DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
		DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressU);
		DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressV);
		DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	}
}

void CMapOutdoor::SelectIndexBuffer(BYTE byLODLevel, WORD * pwPrimitiveCount, Renderer::PrimitiveTopology * pePrimitiveType)
{
	m_terrainGeometryLOD = byLODLevel;
	if (0 == byLODLevel)
	{
		*pwPrimitiveCount = m_wNumIndices[byLODLevel] - 2;
		*pePrimitiveType = Renderer::TopologyTriangleStrip;
	}
	else
	{
		*pwPrimitiveCount =  m_wNumIndices[byLODLevel]/3;
		*pePrimitiveType = Renderer::TopologyTriangleList;
	}
}

void CMapOutdoor::SubmitTerrainGeometry(long patchnum)
{
	if (Renderer::terrainRenderer)
	{
		const Math::Color color(DRAWSTATE.GetRenderState(Renderer::StateTextureFactor));
		Renderer::terrainRenderer->DrawTerrainSolid(m_pTerrainPatchProxyList[patchnum].GetTerrainGeometry(),
			m_terrainIndices[m_terrainGeometryLOD], m_wNumIndices[m_terrainGeometryLOD], m_terrainGeometryLOD == 0,
			{color.r,color.g,color.b,color.a});
	}
}

void CMapOutdoor::SetPatchDrawVector()
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::__SetPatchDrawVector");

	m_PatchDrawStructVector.clear();

	std::vector<std::pair<float, long> >::iterator aDistancePatchVectorIterator;

	TPatchDrawStruct aPatchDrawStruct;

	aDistancePatchVectorIterator = m_PatchVector.begin();
	while(aDistancePatchVectorIterator != m_PatchVector.end())
	{
		std::pair<float, long> adistancePatchPair = *aDistancePatchVectorIterator;

		CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[adistancePatchPair.second];

		if (!pTerrainPatchProxy->isUsed())
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		long lPatchNum = pTerrainPatchProxy->GetPatchNum();
		if (lPatchNum < 0)
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		BYTE byTerrainNum = pTerrainPatchProxy->GetTerrainNum();
		if (0xFF == byTerrainNum)
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		CTerrain * pTerrain;
		if (!GetTerrainPointer(byTerrainNum, &pTerrain))
		{
			++aDistancePatchVectorIterator;
			continue;
		}

		aPatchDrawStruct.fDistance				= adistancePatchPair.first;
		aPatchDrawStruct.byTerrainNum			= byTerrainNum;
		aPatchDrawStruct.lPatchNum				= lPatchNum;
		aPatchDrawStruct.pTerrainPatchProxy		= pTerrainPatchProxy;

		m_PatchDrawStructVector.push_back(aPatchDrawStruct);

		++aDistancePatchVectorIterator;
	}

	std::stable_sort(m_PatchDrawStructVector.begin(), m_PatchDrawStructVector.end(), FSortPatchDrawStructWithTerrainNum());
}

float CMapOutdoor::__GetNoFogDistance()
{
	return (float)(CTerrainImpl::CELLSCALE * m_lViewRadius) * 0.5f;
}

float CMapOutdoor::__GetFogDistance()
{
	return (float)(CTerrainImpl::CELLSCALE * m_lViewRadius) * 0.75f;
}

struct FPatchNumMatch
{
	long m_lPatchNumToCheck;
	FPatchNumMatch(long lPatchNum)
	{
		m_lPatchNumToCheck = lPatchNum;
	}
	bool operator() (std::pair<long, BYTE> aPair)
	{
		return m_lPatchNumToCheck == aPair.first;
	}
};

void CMapOutdoor::NEW_DrawWireFrame(CTerrainPatchProxy * pTerrainPatchProxy, WORD wPrimitiveCount, Renderer::PrimitiveTopology ePrimitiveType)
{
	DWORD dwFillMode = DRAWSTATE.GetRenderState(Renderer::StateFillMode);
	DRAWSTATE.SetRenderState(Renderer::StateFillMode, Renderer::FillWireframe);
	
	DWORD dwFogEnable = DRAWSTATE.GetRenderState(Renderer::StateFogEnable);
	DRAWSTATE.SetRenderState(Renderer::StateFogEnable, FALSE);
	
	DRAWSTATE.SetTexture(0, NULL);
	DRAWSTATE.SetTexture(1, NULL);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpDisable);
	
	
	DRAWSTATE.SetRenderState(Renderer::StateFillMode, dwFillMode);
	DRAWSTATE.SetRenderState(Renderer::StateFogEnable, dwFogEnable);
	
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpModulate);
}

void CMapOutdoor::DrawWireFrame(long patchnum, WORD wPrimitiveCount, Renderer::PrimitiveTopology ePrimitiveType)
{
	assert(NULL!=m_pTerrainPatchProxyList && "CMapOutdoor::DrawWireFrame");

	CTerrainPatchProxy * pTerrainPatchProxy= &m_pTerrainPatchProxyList[patchnum];

	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;
	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	DWORD dwFillMode = DRAWSTATE.GetRenderState(Renderer::StateFillMode);
	DRAWSTATE.SetRenderState(Renderer::StateFillMode, Renderer::FillWireframe);

	DWORD dwFogEnable = DRAWSTATE.GetRenderState(Renderer::StateFogEnable);
	DRAWSTATE.SetRenderState(Renderer::StateFogEnable, FALSE);
	
	DRAWSTATE.SetTexture(0, NULL);
	DRAWSTATE.SetTexture(1, NULL);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpDisable);


	DRAWSTATE.SetRenderState(Renderer::StateFillMode, dwFillMode);
	DRAWSTATE.SetRenderState(Renderer::StateFogEnable, dwFogEnable);

	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpModulate);
}

// Attr
void CMapOutdoor::RenderMarkedArea()
{
	if (!m_pTerrainPatchProxyList)
		return;

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	DRAWSTATE.SetTransform(Renderer::MatrixWorld, &m_matWorldForCommonUse);

	WORD wPrimitiveCount;
	Renderer::PrimitiveTopology eType;
	SelectIndexBuffer(0, &wPrimitiveCount, &eType);

	Math::Matrix matTexTransform, matTexTransformTemp;

	Math::MatrixScaling(&matTexTransform, m_fTerrainTexCoordBase * 32.0f, -m_fTerrainTexCoordBase * 32.0f, 0.0f);
	Math::MatrixMultiply(&matTexTransform, &m_matViewInverse, &matTexTransform);
	DRAWSTATE.SaveTransform(Renderer::MatrixTexture0, &matTexTransform);
	DRAWSTATE.SaveTransform(Renderer::MatrixTexture1, &matTexTransform);

	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateSrcBlend, Renderer::BlendSrcAlpha);
	DRAWSTATE.SaveRenderState(Renderer::StateDestBlend, Renderer::BlendInvSrcAlpha);

	static long lStartTime = timeGetTime();
	float fTime = float((timeGetTime() - lStartTime)%3000) / 3000.0f;
	float fAlpha = fabs(fTime - 0.5f) / 2.0f + 0.1f;
	DRAWSTATE.SetRenderState(Renderer::StateTextureFactor, Math::Color(1.0f, 1.0f, 1.0f, fAlpha));
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgTFactor);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpSelectArg2);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2, Renderer::ArgTFactor);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg2);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg2, Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpModulate);
	DRAWSTATE.SaveTextureStageState(1, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
	DRAWSTATE.SaveTextureStageState(1, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);
	DRAWSTATE.SaveSamplerState(1, Renderer::SamplerMinFilter, Renderer::FilterPoint);
	DRAWSTATE.SaveSamplerState(1, Renderer::SamplerMagFilter, Renderer::FilterPoint);
	DRAWSTATE.SaveSamplerState(1, Renderer::SamplerMipFilter, Renderer::FilterPoint);
	DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressClamp);
	DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressClamp);

	DRAWSTATE.SetTexture(0, m_attrImageInstance.GetTexturePointer()->GetTextureBinding());

	RecurseRenderAttr(m_pRootNode);

	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTexCoordIndex);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTextureTransformFlags);
	DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTexCoordIndex);
	DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTextureTransformFlags);
	DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerMinFilter);
	DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerMagFilter);
	DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerMipFilter);
	DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressU);
	DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressV);

	DRAWSTATE.RestoreTransform(Renderer::MatrixTexture0);
	DRAWSTATE.RestoreTransform(Renderer::MatrixTexture1);

	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateSrcBlend);
	DRAWSTATE.RestoreRenderState(Renderer::StateDestBlend);
}

void CMapOutdoor::RecurseRenderAttr(CTerrainQuadtreeNode *Node, bool bCullEnable)
{
	if (bCullEnable)
	{
		if (__RenderTerrain_RecurseRenderQuadTree_CheckBoundingCircle(Node->center, Node->radius)==VIEW_NONE)
			return;
	}

	{
		if (Node->Size == 1)
		{
			DrawPatchAttr(Node->PatchNum);
		}
		else
		{
			if (Node->NW_Node != NULL)
				RecurseRenderAttr(Node->NW_Node, bCullEnable);
			if (Node->NE_Node != NULL)
				RecurseRenderAttr(Node->NE_Node, bCullEnable);
			if (Node->SW_Node != NULL)
				RecurseRenderAttr(Node->SW_Node, bCullEnable);
			if (Node->SE_Node != NULL)
				RecurseRenderAttr(Node->SE_Node, bCullEnable);
		}
 	}
}

void CMapOutdoor::DrawPatchAttr(long patchnum)
{
	CTerrainPatchProxy * pTerrainPatchProxy = &m_pTerrainPatchProxyList[patchnum];
	if (!pTerrainPatchProxy->isUsed())
		return;

	long sPatchNum = pTerrainPatchProxy->GetPatchNum();
	if (sPatchNum < 0)
		return;

	BYTE ucTerrainNum = pTerrainPatchProxy->GetTerrainNum();
	if (0xFF == ucTerrainNum)
		return;

	// Deal with this material buffer
	CTerrain * pTerrain;
	if (!GetTerrainPointer(ucTerrainNum, &pTerrain))
		return;

	if (!pTerrain->IsMarked())
		return;

	WORD wCoordX, wCoordY;
	pTerrain->GetCoordinate(&wCoordX, &wCoordY);

	m_matWorldForCommonUse._41 = -(float) (wCoordX * CTerrainImpl::XSIZE * CTerrainImpl::CELLSCALE);
	m_matWorldForCommonUse._42 = (float) (wCoordY * CTerrainImpl::YSIZE * CTerrainImpl::CELLSCALE);

	Math::Matrix matTexTransform, matTexTransformTemp;
	Math::MatrixMultiply(&matTexTransform, &m_matViewInverse, &m_matWorldForCommonUse);
	Math::MatrixMultiply(&matTexTransform, &matTexTransform, &m_matStaticShadow);
	DRAWSTATE.SetTransform(Renderer::MatrixTexture1, &matTexTransform);

	TTerrainSplatPatch & rAttrSplatPatch = pTerrain->GetMarkedSplatPatch();
	DRAWSTATE.SetTexture(1, pTerrain->GetMarkedBinding());


	// ZiiNAN: Existing guild-area geometry and generated alpha projection, in native order.
	if(Renderer::worldRenderer && Renderer::worldSurfaceFrame) {
		auto* renderer=Renderer::worldRenderer; const auto* source=pTerrainPatchProxy->GetProjectionVertices();
		Renderer::EffectDraw draw; std::string error;
		if(!source || !CaptureMaterialState(draw,error,true)) { renderer->ReportFailure(); return; }
		draw.secondaryTexture=pTerrain->GetMarkedTexture();
		if(!m_projectionTexture) m_projectionTexture=LoadStaticObjectTextureFile(m_attrImageInstance.GetGraphicImagePointer()->GetFileName(),*renderer);
		if(!draw.secondaryTexture || !m_projectionTexture) { renderer->ReportFailure(); return; }
		std::vector<Renderer::EffectVertex> vertices; vertices.reserve(m_projectionIndices.size());
		for(auto index:m_projectionIndices) {
			if(index>=source->size()) { renderer->ReportFailure(); return; }
			vertices.push_back((*source)[index]);
		}
		renderer->Draw(vertices.data(),uint32_t(vertices.size()),m_projectionTexture,draw,Renderer::WorldPart::Guild);
	}
}
