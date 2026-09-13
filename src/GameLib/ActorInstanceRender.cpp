#include "StdAfx.h"
#include "EterLib/DrawState.h"

#include "ActorInstance.h"
#include "ActorRenderBridge.h" // ZiiNAN: No changes to native actor draws.

bool CActorInstance::ms_isDirLine=false;

bool CActorInstance::IsDirLine()
{
	return ms_isDirLine;
}

void CActorInstance::ShowDirectionLine(bool isVisible)
{
	ms_isDirLine=isVisible;
}

void CActorInstance::SetMaterialColor(DWORD dwColor)
{
	if (m_pkHorse)
		m_pkHorse->SetMaterialColor(dwColor);

	m_dwMtrlColor&=0xff000000;
	m_dwMtrlColor|=(dwColor&0x00ffffff);
}

void CActorInstance::SetMaterialAlpha(DWORD dwAlpha)
{
	m_dwMtrlAlpha=dwAlpha;	
}


void CActorInstance::OnRender()
{
	// Early out if race data is not loaded yet (async loading)
	if (!m_pkCurRaceData)
		return;

    // ZiiNAN: Diligent mount actor rendering
    Renderer::ActorDrawScope actorScope(!m_pkHorse || Renderer::actorMountPair.rider==this ?
        MakeAnimatedActorTarget(*this) : Renderer::ActorDrawTarget{});

	Renderer::MaterialValues kMtrl;
	DRAWSTATE.GetMaterial(&kMtrl);

	kMtrl.Diffuse=Math::Color(m_dwMtrlColor);
	DRAWSTATE.SetMaterial(&kMtrl);

	// 현재는 이렇게.. 최종적인 형태는 Diffuse와 Blend의 분리로..
	// 아니면 이런 형태로 가되 Texture & State Sorting 지원으로.. - [levites]
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullNone);

	switch(m_iRenderMode)
	{
		case RENDER_MODE_NORMAL:
			BeginDiffuseRender();
				RenderWithOneTexture();
			EndDiffuseRender();
			BeginOpacityRender();
				BlendRenderWithOneTexture();
			EndOpacityRender();
			break;
		case RENDER_MODE_BLEND:
			if (m_fAlphaValue == 1.0f)
			{
				BeginDiffuseRender();
					RenderWithOneTexture();
				EndDiffuseRender();
				BeginOpacityRender();
					BlendRenderWithOneTexture();
				EndOpacityRender();
			}
			else if (m_fAlphaValue > 0.0f)
			{
				BeginBlendRender();
					RenderWithOneTexture();
					BlendRenderWithOneTexture();
				EndBlendRender();
			}
			break;
		case RENDER_MODE_ADD:
			BeginAddRender();
				RenderWithOneTexture();
				BlendRenderWithOneTexture();
			EndAddRender();
			break;
		case RENDER_MODE_MODULATE:
			BeginModulateRender();
				RenderWithOneTexture();
				BlendRenderWithOneTexture();
			EndModulateRender();
			break;
	}

	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);

	kMtrl.Diffuse=Math::Color(0xffffffff);
	DRAWSTATE.SetMaterial(&kMtrl);

	if (ms_isDirLine)
	{
		Math::Vector3 kD3DVt3Cur(m_x, m_y, m_z);

		Math::Vector3 kD3DVt3LookDir(0.0f, -1.0f, 0.0f);
		Math::Matrix kD3DMatLook;
		Math::MatrixRotationZ(&kD3DMatLook, Math::ToRadian(GetRotation()));
		Math::Vec3TransformCoord(&kD3DVt3LookDir, &kD3DVt3LookDir, &kD3DMatLook);
		Math::Vec3Scale(&kD3DVt3LookDir, &kD3DVt3LookDir, 200.0f);
		Math::Vec3Add(&kD3DVt3LookDir, &kD3DVt3LookDir, &kD3DVt3Cur);

		Math::Vector3 kD3DVt3AdvDir(0.0f, -1.0f, 0.0f);
		Math::Matrix kD3DMatAdv;
		Math::MatrixRotationZ(&kD3DMatAdv, Math::ToRadian(GetAdvancingRotation()));
		Math::Vec3TransformCoord(&kD3DVt3AdvDir, &kD3DVt3AdvDir, &kD3DMatAdv);
		Math::Vec3Scale(&kD3DVt3AdvDir, &kD3DVt3AdvDir, 200.0f);
		Math::Vec3Add(&kD3DVt3AdvDir, &kD3DVt3AdvDir, &kD3DVt3Cur);

		static CScreen s_kScreen;

		DRAWSTATE.SaveTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgDiffuse);
		DRAWSTATE.SaveTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpSelectArg1);
		DRAWSTATE.SaveTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpDisable);
		DRAWSTATE.SaveRenderState(Renderer::StateZEnable, FALSE);
		DRAWSTATE.SaveRenderState(Renderer::StateLighting, FALSE);

		s_kScreen.SetDiffuseColor(1.0f, 1.0f, 0.0f);
		s_kScreen.RenderLine3d(kD3DVt3Cur.x, kD3DVt3Cur.y, kD3DVt3Cur.z, kD3DVt3AdvDir.x, kD3DVt3AdvDir.y, kD3DVt3AdvDir.z);

		s_kScreen.SetDiffuseColor(0.0f, 1.0f, 1.0f);
		s_kScreen.RenderLine3d(kD3DVt3Cur.x, kD3DVt3Cur.y, kD3DVt3Cur.z, kD3DVt3LookDir.x, kD3DVt3LookDir.y, kD3DVt3LookDir.z);

		DRAWSTATE.RestoreRenderState(Renderer::StateLighting);
		DRAWSTATE.RestoreRenderState(Renderer::StateZEnable);

		DRAWSTATE.RestoreTextureStageState(0, Renderer::StageColorArg1);
		DRAWSTATE.RestoreTextureStageState(0, Renderer::StageColorOp);
		DRAWSTATE.RestoreTextureStageState(0, Renderer::StageAlphaOp);
	}
}

void CActorInstance::BeginDiffuseRender()
{
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);

	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, FALSE);
}

void CActorInstance::EndDiffuseRender()
{
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
}

void CActorInstance::BeginOpacityRender()
{
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaTestEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaRef, 0);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreater);

	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);
}

void CActorInstance::EndOpacityRender()
{
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaTestEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaRef);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaFunc);
}

void CActorInstance::BeginBlendRender()
{
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateSrcBlend, Renderer::BlendSrcAlpha);
	DRAWSTATE.SaveRenderState(Renderer::StateDestBlend, Renderer::BlendInvSrcAlpha);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);

	DRAWSTATE.SetRenderState(Renderer::StateTextureFactor, Math::Color(1.0f, 1.0f, 1.0f, m_fAlphaValue));
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2, Renderer::ArgTFactor);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpSelectArg2);
}

void CActorInstance::EndBlendRender()
{
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateSrcBlend);
	DRAWSTATE.RestoreRenderState(Renderer::StateDestBlend);
}

void CActorInstance::BeginAddRender()
{
	DRAWSTATE.SetRenderState(Renderer::StateTextureFactor, m_AddColor);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1,	Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2,	Renderer::ArgTFactor);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,	Renderer::TextureOpAdd);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,	Renderer::TextureOpDisable);

	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, FALSE);
}

void CActorInstance::EndAddRender()
{
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
}

void CActorInstance::RestoreRenderMode()
{
	// NOTE : This is temporary code. I wanna convert this code to that restore the mode to
	//        model's default setting which had has as like specular or normal. - [levites]
	m_iRenderMode = RENDER_MODE_NORMAL;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = m_iRenderMode;
	}
}


void CActorInstance::SetAddRenderMode()
{
	m_iRenderMode = RENDER_MODE_ADD;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = m_iRenderMode;
	}
}

void CActorInstance::SetRenderMode(int iRenderMode)
{
	m_iRenderMode = iRenderMode;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = iRenderMode;
	}
}

void CActorInstance::SetAddColor(const Math::Color & c_rColor)
{
	m_AddColor = c_rColor;
	m_AddColor.a = 1.0f;
}

void CActorInstance::BeginModulateRender()
{
	DRAWSTATE.SetRenderState(Renderer::StateTextureFactor, m_AddColor);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1,	Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2,	Renderer::ArgTFactor);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,	Renderer::TextureOpDisable);

	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, FALSE);
}

void CActorInstance::EndModulateRender()
{
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
}

void CActorInstance::SetModulateRenderMode()
{
	m_iRenderMode = RENDER_MODE_MODULATE;
	if (m_kBlendAlpha.m_isBlending)
	{
		m_kBlendAlpha.m_iOldRenderMode = m_iRenderMode;
	}
}

void CActorInstance::RenderCollisionData()
{
	static CScreen s_Screen;

	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullNone);
	if (m_pAttributeInstance)
	{
		for (DWORD col=0; col < GetCollisionInstanceCount(); ++col)
		{
			CBaseCollisionInstance * pInstance = GetCollisionInstanceData(col);
			pInstance->Render();
		}
	}

	DRAWSTATE.SetRenderState(Renderer::StateZEnable, FALSE);
	s_Screen.SetColorOperation();
	s_Screen.SetDiffuseColor(1.0f, 0.0f, 0.0f);
	TCollisionPointInstanceList::iterator itor;
	/*itor = m_AttackingPointInstanceList.begin();
	for (; itor != m_AttackingPointInstanceList.end(); ++itor)
	{
		const TCollisionPointInstance & c_rInstance = *itor;
		for (DWORD i = 0; i < c_rInstance.SphereInstanceVector.size(); ++i)
		{
			const CDynamicSphereInstance & c_rSphereInstance = c_rInstance.SphereInstanceVector[i];
			s_Screen.RenderCircle3d(c_rSphereInstance.v3Position.x,
									c_rSphereInstance.v3Position.y,
									c_rSphereInstance.v3Position.z,
									c_rSphereInstance.fRadius);
		}
	}*/
	s_Screen.SetDiffuseColor(1.0f, (isShow())?1.0f:0.0f, 0.0f);
	Math::Vector3 center;
	float r;
	GetBoundingSphere(center,r);
	s_Screen.RenderCircle3d(center.x,center.y,center.z,r);

	s_Screen.SetDiffuseColor(0.0f, 0.0f, 1.0f);
	itor = m_DefendingPointInstanceList.begin();
	for (; itor != m_DefendingPointInstanceList.end(); ++itor)
	{
		const TCollisionPointInstance & c_rInstance = *itor;
		for (DWORD i = 0; i < c_rInstance.SphereInstanceVector.size(); ++i)
		{
			const CDynamicSphereInstance & c_rSphereInstance = c_rInstance.SphereInstanceVector[i];
			s_Screen.RenderCircle3d(c_rSphereInstance.v3Position.x,
									c_rSphereInstance.v3Position.y,
									c_rSphereInstance.v3Position.z,
									c_rSphereInstance.fRadius);
		}
	}

	s_Screen.SetDiffuseColor(0.0f, 1.0f, 0.0f);
	itor = m_BodyPointInstanceList.begin();
	for (; itor != m_BodyPointInstanceList.end(); ++itor)
	{
		const TCollisionPointInstance & c_rInstance = *itor;
		for (DWORD i = 0; i < c_rInstance.SphereInstanceVector.size(); ++i)
		{
			const CDynamicSphereInstance & c_rSphereInstance = c_rInstance.SphereInstanceVector[i];
			s_Screen.RenderCircle3d(c_rSphereInstance.v3Position.x,
									c_rSphereInstance.v3Position.y,
									c_rSphereInstance.v3Position.z,
									c_rSphereInstance.fRadius);
		}
	}

	s_Screen.SetDiffuseColor(1.0f, 0.0f, 0.0f);
//	if (m_SplashArea.fDisappearingTime > GetLocalTime())
	{
		CDynamicSphereInstanceVector::iterator itor = m_kSplashArea.SphereInstanceVector.begin();
		for (; itor != m_kSplashArea.SphereInstanceVector.end(); ++itor)
		{
			const CDynamicSphereInstance & c_rInstance = *itor;
			s_Screen.RenderCircle3d(c_rInstance.v3Position.x,
									c_rInstance.v3Position.y,
									c_rInstance.v3Position.z,
									c_rInstance.fRadius);
		}
	}

	DRAWSTATE.SetRenderState(Renderer::StateZEnable, TRUE);
	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
	DRAWSTATE.SetRenderState(Renderer::StateLighting, TRUE);
}


void CActorInstance::RenderToShadowMap()
{
	if (RENDER_MODE_BLEND == m_iRenderMode)
	if (GetAlphaValue() < 0.5f)
		return;

	CGraphicThingInstance::RenderToShadowMap();

	if (m_pkHorse)
		m_pkHorse->RenderToShadowMap();
}
