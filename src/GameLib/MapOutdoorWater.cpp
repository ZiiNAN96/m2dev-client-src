#include "StdAfx.h"
#include "EterBase/MapLoadTrace.h"
#include "Renderer/GraphicsConfig.h"
#include "EterLib/DrawState.h"
#include "EterLib/ResourceManager.h"
#include "EterLib/WorldRenderBridge.h"

#include "MapOutdoor.h"
#include "TerrainPatch.h"

void CMapOutdoor::LoadWaterTexture()
{
    MapLoadTrace::Scope p0lScope("Water","water texture setup","cpu");

	UnloadWaterTexture();
	char buf[256];
	for (int i = 0; i < 30; ++i)
	{
		sprintf(buf, "d:/ymir Work/special/water/%02d.dds", i+1);
		m_WaterInstances[i].SetImagePointer((CGraphicImage *) CResourceManager::Instance().GetResourcePointer(buf));
	}
}

void CMapOutdoor::UnloadWaterTexture()
{
	WorldRenderBridge::Release(m_waterResources);
	Renderer::waterTexturesResident=0;
	for (int i = 0; i < 30; ++i)
		m_WaterInstances[i].Destroy();
}

void CMapOutdoor::RenderWater()
{
	// ZiiNAN: Diligent water rendering integration; same native frame, height and material.
	WorldRenderScope worldScope(m_waterResources,Renderer::WorldPart::Water);
	if (m_PatchVector.empty())
		return;

	if (!IsVisiblePart(PART_WATER))
		return;

	//////////////////////////////////////////////////////////////////////////
	// RenderState
	Math::Matrix matTexTransformWater;
	
	DRAWSTATE.SaveRenderState(Renderer::StateZWriteEnable, FALSE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullNone);
	DRAWSTATE.SaveRenderState(Renderer::StateDiffuseMaterialSource, Renderer::MaterialColor1);
	DRAWSTATE.SaveRenderState(Renderer::StateColorVertex, TRUE);

	const auto waterFrame=(ELTimer_GetMSec()/Renderer::GetGraphicsRuntimeConfig().waterFrameMilliseconds)%30;
	DRAWSTATE.SetTexture(0, m_WaterInstances[waterFrame].GetTexturePointer()->GetTextureBinding());
	WorldRenderBridge::Texture(m_WaterInstances[waterFrame].GetGraphicImagePointer());

	Math::MatrixScaling(&matTexTransformWater, m_fWaterTexCoordBase, -m_fWaterTexCoordBase, 0.0f);
	Math::MatrixMultiply(&matTexTransformWater, &m_matViewInverse, &matTexTransformWater);
	
	DRAWSTATE.SaveTransform(Renderer::MatrixTexture0, &matTexTransformWater);

	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpacePosition);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);

	DRAWSTATE.SaveSamplerState(0, Renderer::SamplerMinFilter, Renderer::FilterAnisotropic);
	DRAWSTATE.SaveSamplerState(0, Renderer::SamplerMagFilter, Renderer::FilterAnisotropic);
	DRAWSTATE.SaveSamplerState(0, Renderer::SamplerMipFilter, Renderer::FilterLinear);
	DRAWSTATE.SaveSamplerState(0, Renderer::SamplerAddressU, Renderer::AddressWrap);
	DRAWSTATE.SaveSamplerState(0, Renderer::SamplerAddressV, Renderer::AddressWrap);
	
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg1);
	

	DRAWSTATE.SetTexture(1,NULL);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);

	// RenderState
	//////////////////////////////////////////////////////////////////////////

	// 물 위 아래 애니시키기...
	static float s_fWaterHeightCurrent = 0;
	static float s_fWaterHeightBegin = 0;
	static float s_fWaterHeightEnd = 0;
	static DWORD s_dwLastHeightChangeTime = CTimer::Instance().GetCurrentMillisecond();
	static DWORD s_dwBlendtime = 300;

	// 1.5초 마다 변경
	if ((CTimer::Instance().GetCurrentMillisecond() - s_dwLastHeightChangeTime) > s_dwBlendtime)
	{
		s_dwBlendtime = random_range(1000, 3000);

		if (s_fWaterHeightEnd == 0)
			s_fWaterHeightEnd = -random_range(0, 15);
		else
			s_fWaterHeightEnd = 0;

		s_fWaterHeightBegin = s_fWaterHeightCurrent;
		s_dwLastHeightChangeTime = CTimer::Instance().GetCurrentMillisecond();
	}

	s_fWaterHeightCurrent = s_fWaterHeightBegin + (s_fWaterHeightEnd - s_fWaterHeightBegin) * (float) ((CTimer::Instance().GetCurrentMillisecond() - s_dwLastHeightChangeTime) / (float) s_dwBlendtime);
	m_matWorldForCommonUse._43 = s_fWaterHeightCurrent;

	m_matWorldForCommonUse._41 = 0.0f;
	m_matWorldForCommonUse._42 = 0.0f;
	DRAWSTATE.SetTransform(Renderer::MatrixWorld, &m_matWorldForCommonUse);
	
	float fFogDistance = __GetFogDistance();

	std::vector<std::pair<float, long> >::iterator i;

	for(i = m_PatchVector.begin();i != m_PatchVector.end(); ++i)
	{
		if (i->first<fFogDistance)	
			DrawWater(i->second);
	}

	DRAWSTATE.SetTexture(0, NULL);
	DRAWSTATE.SetRenderState(Renderer::StateAlphaBlendEnable, FALSE);

	for(i = m_PatchVector.begin();i != m_PatchVector.end(); ++i)
	{
		if (i->first>=fFogDistance)	
			DrawWater(i->second);
	}

	// 렌더링 한 후에는 물 z 위치를 복구
	m_matWorldForCommonUse._43 = 0.0f;

	//////////////////////////////////////////////////////////////////////////
	// RenderState
	DRAWSTATE.RestoreTransform(Renderer::MatrixTexture0);
	DRAWSTATE.RestoreSamplerState(0, Renderer::SamplerMinFilter);
	DRAWSTATE.RestoreSamplerState(0, Renderer::SamplerMagFilter);
	DRAWSTATE.RestoreSamplerState(0, Renderer::SamplerMipFilter);
	DRAWSTATE.RestoreSamplerState(0, Renderer::SamplerAddressU);
	DRAWSTATE.RestoreSamplerState(0, Renderer::SamplerAddressV);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTexCoordIndex);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageTextureTransformFlags);
	
	DRAWSTATE.RestoreRenderState(Renderer::StateDiffuseMaterialSource);
	DRAWSTATE.RestoreRenderState(Renderer::StateColorVertex);
	DRAWSTATE.RestoreRenderState(Renderer::StateZWriteEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
}

void CMapOutdoor::DrawWater(long patchnum)
{
	assert(NULL!=m_pTerrainPatchProxyList);
	if (!m_pTerrainPatchProxyList)
		return;

	CTerrainPatchProxy& rkTerrainPatchProxy = m_pTerrainPatchProxyList[patchnum];

	if (!rkTerrainPatchProxy.isUsed())
		return;

	if (!rkTerrainPatchProxy.isWaterExists())
		return;

	CGraphicVertexBuffer* pkVB=rkTerrainPatchProxy.GetWaterVertexBufferPointer();
	if (!pkVB)
		return;
	
	if (pkVB->IsEmpty())
		return;

	UINT uPriCount=rkTerrainPatchProxy.GetWaterFaceCount();
	if (!uPriCount)
		return;
	
	if(Renderer::worldRenderer && Renderer::worldSurfaceFrame) {
		auto geometry=rkTerrainPatchProxy.GetWaterGeometry();
		if(geometry && geometry->vertices.size()==size_t(uPriCount)*3)
			WorldRenderBridge::Submit(geometry->vertices.data(),UINT(geometry->vertices.size()),false);
		else Renderer::worldRenderer->ReportFailure();
	}

	ms_faceCount += uPriCount;
}
