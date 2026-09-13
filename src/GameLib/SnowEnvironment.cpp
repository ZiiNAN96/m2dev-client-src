#include "StdAfx.h"
#include "SnowEnvironment.h"

#include "EterLib/DrawState.h"
#include "EterLib/Camera.h"
#include "EterLib/ResourceManager.h"
#include "SnowParticle.h"
#include "EffectLib/EffectRenderBridge.h" // ZiiNAN: Diligent effect rendering integration.

void CSnowEnvironment::Enable()
{
	if (!m_bSnowEnable)
	{
		Create();
	}

	m_bSnowEnable = TRUE;
}

void CSnowEnvironment::Disable()
{
	m_bSnowEnable = FALSE;
}

void CSnowEnvironment::Update(const Math::Vector3 & c_rv3Pos)
{
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	m_v3Center=c_rv3Pos;
}

void CSnowEnvironment::Deform()
{
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	const Math::Vector3 & c_rv3Pos=m_v3Center;
	
	static long s_lLastTime = CTimer::Instance().GetCurrentMillisecond();
	long lcurTime = CTimer::Instance().GetCurrentMillisecond();
	float fElapsedTime = float(lcurTime - s_lLastTime) / 1000.0f;
	s_lLastTime = lcurTime;

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	const Math::Vector3 & c_rv3View = pCamera->GetView();

	Math::Vector3 v3ChangedPos = c_rv3View * 3500.0f + c_rv3Pos;
	v3ChangedPos.z = c_rv3Pos.z;

	std::vector<CSnowParticle*>::iterator itor = m_kVct_pkParticleSnow.begin();
	for (; itor != m_kVct_pkParticleSnow.end();)
	{
		CSnowParticle * pSnow = *itor;
		pSnow->Update(fElapsedTime, v3ChangedPos);

		if (!pSnow->IsActivate())
		{
			CSnowParticle::Delete(pSnow);

			itor = m_kVct_pkParticleSnow.erase(itor);
		}
		else
		{
			++itor;
		}
	}

	if (m_bSnowEnable)
	{
		for (int p = 0; p < std::min(10ull, m_dwParticleMaxNum - m_kVct_pkParticleSnow.size()); ++p)
		{
			CSnowParticle * pSnowParticle = CSnowParticle::New();
			pSnowParticle->Init(v3ChangedPos);
			m_kVct_pkParticleSnow.push_back(pSnowParticle);
		}
	}
}





void CSnowEnvironment::Render()
{
    EffectRenderScope effectScope(m_effectResources,"snow",Renderer::EffectPart::Snow);
    std::vector<SParticleVertex> effectVertices;
	if (!m_bSnowEnable)
	{
		if (m_kVct_pkParticleSnow.empty())
			return;
	}

	DWORD dwParticleCount = std::min((size_t)m_dwParticleMaxNum, m_kVct_pkParticleSnow.size());

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	const Math::Vector3 & c_rv3Up = pCamera->GetUp();
	const Math::Vector3 & c_rv3Cross = pCamera->GetCross();

	std::vector<SParticleVertex> cpuVertices;
    cpuVertices.resize(size_t(dwParticleCount)*4);
    SParticleVertex * pv3Verticies=cpuVertices.data();
	{
		int i = 0;
		std::vector<CSnowParticle*>::iterator itor = m_kVct_pkParticleSnow.begin();
		for (; i < dwParticleCount && itor != m_kVct_pkParticleSnow.end(); ++i, ++itor)
		{
			CSnowParticle * pSnow = *itor;
			pSnow->SetCameraVertex(c_rv3Up, c_rv3Cross);
			pSnow->GetVerticies(pv3Verticies[i*4+0],
								pv3Verticies[i*4+1],
								pv3Verticies[i*4+2],
								pv3Verticies[i*4+3]);
		}

        if(Renderer::effectRenderer && Renderer::effectWorldFrame) {
            // Preserve the native index order while the original CPU vertices are still locked.
            constexpr unsigned corners[]={0,2,1,2,3,1};
            effectVertices.reserve(size_t(dwParticleCount)*6);
            for(unsigned particle=0;particle<dwParticleCount;++particle)
                for(auto corner:corners) effectVertices.push_back(pv3Verticies[particle*4+corner]);
        }
	}

	DRAWSTATE.SaveRenderState(Renderer::StateZWriteEnable, FALSE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullNone);
	DRAWSTATE.SetRenderState(Renderer::StateSrcBlend,  Renderer::BlendSrcAlpha);
	DRAWSTATE.SetRenderState(Renderer::StateDestBlend, Renderer::BlendInvSrcAlpha);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTexture(1, NULL);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);

	m_pImageInstance->GetGraphicImagePointer()->GetTextureReference().SetTextureStage(0);
    if(!effectVertices.empty()) {
        EffectRenderBridge::Texture(m_pImageInstance->GetGraphicImagePointer());
        EffectRenderBridge::Submit(Renderer::TopologyTriangleList,dwParticleCount*2,effectVertices.data(),sizeof(SParticleVertex));
    }
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateZWriteEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
}





bool CSnowEnvironment::Create()
{
	Destroy();



	CGraphicImage * pImage = (CGraphicImage *)CResourceManager::Instance().GetResourcePointer("d:/ymir work/special/snow.dds");
	m_pImageInstance = CGraphicImageInstance::New();
	m_pImageInstance->SetImagePointer(pImage);

	return true;
}

void CSnowEnvironment::Destroy()
{
    m_effectResources.textures.clear();

	stl_wipe(m_kVct_pkParticleSnow);
	CSnowParticle::DestroyPool();

	if (m_pImageInstance)
	{
		CGraphicImageInstance::Delete(m_pImageInstance);
		m_pImageInstance = NULL;
	}

	__Initialize();
}

void CSnowEnvironment::__Initialize()
{
	m_bSnowEnable = FALSE;
	m_pImageInstance = NULL;

	m_kVct_pkParticleSnow.reserve(m_dwParticleMaxNum);
}

CSnowEnvironment::CSnowEnvironment()
{
	m_dwParticleMaxNum = 3000;
	m_wBlurTextureSize = 512;

	__Initialize();
}
CSnowEnvironment::~CSnowEnvironment()
{
	Destroy();
}
