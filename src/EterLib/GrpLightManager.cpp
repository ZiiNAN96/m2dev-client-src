#include "StdAfx.h"
#include "EterLib/DrawStateView.h"
#include <algorithm>
#include "EterBase/Timer.h"

#include "GrpLightManager.h"
#include "DrawState.h"

float CLightBase::ms_fCurTime = 0.0f;

CLightManager::CLightManager()
{
	m_v3CenterPosition			= Math::Vector3(0.0f, 0.0f, 0.0f);
	m_dwLimitLightCount			= LIGHT_LIMIT_DEFAULT;
}

CLightManager::~CLightManager()
{
}

void CLightManager::Destroy()
{
	m_LightPool.Destroy();
}

void CLightManager::Initialize()
{
	SetSkipIndex(1);
	
	m_NonUsingLightIDDeque.clear();

	m_LightMap.clear();
	m_LightPool.FreeAll();
}

void CLightManager::RegisterLight(ELightType /*LightType*/, TLightID * poutLightID, Renderer::LightValues & LightData)
{
	CLight * pLight = m_LightPool.Alloc();
	TLightID ID = NewLightID();
	pLight->SetParameter(ID, LightData);
	m_LightMap[ID] = pLight;
	*poutLightID = ID;
}

void CLightManager::DeleteLight(TLightID LightID)
{
	TLightMap::iterator itor = m_LightMap.find(LightID);

	if (m_LightMap.end() == itor)
	{
		assert(!"CLightManager::DeleteLight - Failed to find light ID!");
		return;
	}

	CLight * pLight = itor->second;

	pLight->Clear();
	m_LightPool.Free(pLight);

	m_LightMap.erase(itor);

	ReleaseLightID(LightID);
}

CLight * CLightManager::GetLight(TLightID LightID)
{
	TLightMap::iterator itor = m_LightMap.find(LightID);

	if (m_LightMap.end() == itor)
	{
		assert(!"CLightManager::SetLightData - Failed to find light ID!");
		return NULL;
	}

	return itor->second;
}

void CLightManager::SetCenterPosition(const Math::Vector3 & c_rv3Position)
{
	m_v3CenterPosition = c_rv3Position;
}

void CLightManager::SetLimitLightCount(DWORD dwLightCount)
{
	m_dwLimitLightCount = dwLightCount;
}

void CLightManager::SetSkipIndex(DWORD dwSkipIndex)
{
	m_dwSkipIndex = dwSkipIndex;
}

struct LightComp
{
	bool operator () (const CLight * l, const CLight * r) const
	{
		return l->GetDistance() < r->GetDistance();
	}
};

// NOTE : FlushLight후 렌더링
//        그 후 반드시 RestoreLight를 해줘야만 한다.
void CLightManager::FlushLight()
{
	Update();

	m_LightSortVector.clear();

	// NOTE: Dynamic과 Static을 분리 시키고 CenterPosition이 바뀔때마다 Static만
	//		 다시 Flush 하는 식으로 최적화 할 수 있다. - [levites]

	// light들의 거리를 추출해 정렬한다.
	TLightMap::iterator itor = m_LightMap.begin();

	for (; itor != m_LightMap.end(); ++itor)
	{
		CLight * pLight = itor->second;

		Math::Vector3 v3LightPos(pLight->GetPosition());
		Math::Vector3 v3Distance(v3LightPos - m_v3CenterPosition);
		pLight->SetDistance(Math::Vec3Length(&v3Distance));
		m_LightSortVector.push_back(pLight);
	}

	// quick sort lights
	std::sort(m_LightSortVector.begin(), m_LightSortVector.end(), LightComp());

	// NOTE - 거리로 정렬된 라이트를 Limit 갯수 만큼 제한해서 켜준다.
	DRAWSTATE.SaveRenderState(Renderer::StateLighting, TRUE);

	for (DWORD k = 0; k < std::min((size_t)m_dwLimitLightCount, m_LightSortVector.size()); ++k)
	{
		m_LightSortVector[k]->Update();
		m_LightSortVector[k]->SetDeviceLight(TRUE);

	}
}

void CLightManager::RestoreLight()
{
	DRAWSTATE.RestoreRenderState(Renderer::StateLighting);

	for (DWORD k = 0; k < std::min((size_t)m_dwLimitLightCount, m_LightSortVector.size()); ++k)
		m_LightSortVector[k]->SetDeviceLight(FALSE);
}

TLightID CLightManager::NewLightID()
{
	if (!m_NonUsingLightIDDeque.empty())
	{
		TLightID id = m_NonUsingLightIDDeque.back();
		m_NonUsingLightIDDeque.pop_back();
		return (id);
	}

	return m_dwSkipIndex + m_LightMap.size();
}

void CLightManager::ReleaseLightID(TLightID LightID)
{
	m_NonUsingLightIDDeque.push_back(LightID);
}

void CLightManager::Update()
{
	//static DWORD s_dwStartTime = ELTimer_GetMSec();
	//ms_fCurTime = float(ELTimer_GetMSec() - s_dwStartTime) / 1000.0f;
	ms_fCurTime = CTimer::Instance().GetCurrentSecond();
}

//////////////////////////////////////////////////////////////////////////
CLight::CLight()
{
	Initialize();
}

CLight::~CLight()
{
	Clear();
}

void CLight::Initialize()
{
	m_LightID	= 0;
	m_isEdited	= TRUE;
	m_fDistance	= 0.0f;

	memset(&m_light, 0, sizeof(m_light));

	m_light.Type			= Renderer::LightPoint;
	m_light.Attenuation0	= 0.0f;
	m_light.Attenuation1	= 1.0f;
	m_light.Attenuation2	= 0.0f;
}

void CLight::Clear()
{
	if (m_LightID) 
		SetDeviceLight(FALSE);
	Initialize();
}

void CLight::SetDeviceLight(BOOL bActive)
{
	if (bActive && m_isEdited)
	{
		if (CDrawState::InstancePtr())
			DRAWSTATE.SetLight(m_LightID, &m_light);
	}
	if (CDrawState::InstancePtr())
	{
		DRAWSTATE.LightEnable(m_LightID, bActive);
	}
}

void CLight::SetParameter(TLightID id, const Renderer::LightValues & c_rLight)
{
	m_LightID	= id;
	m_light	= c_rLight;
}

void CLight::SetDiffuseColor(float fr, float fg, float fb, float fa)
{
	if (m_light.Diffuse.r == fr
		&& m_light.Diffuse.g == fg
		&& m_light.Diffuse.b == fb
		&& m_light.Diffuse.a == fa
		)
		return;	
	m_light.Diffuse.r = fr;
	m_light.Diffuse.g = fg;
	m_light.Diffuse.b = fb;
	m_light.Diffuse.a = fa;
	m_isEdited = TRUE;
}

void CLight::SetAmbientColor(float fr, float fg, float fb, float fa)
{
	if (m_light.Ambient.r == fr
		&& m_light.Ambient.g == fg
		&& m_light.Ambient.b == fb
		&& m_light.Ambient.a == fa
		)
		return;
	m_light.Ambient.r = fr;
	m_light.Ambient.g = fg;
	m_light.Ambient.b = fb;
	m_light.Ambient.a = fa;
	m_isEdited = TRUE;
}

void CLight::SetRange(float fRange)
{
	if (m_light.Range == fRange)
		return;
	
	m_light.Range = fRange;
	m_isEdited = TRUE;
}

const Math::Vector3 & CLight::GetPosition() const
{
	return m_light.Position;
}

void CLight::SetPosition(float fx, float fy, float fz)
{
	if (m_light.Position.x == fx && m_light.Position.y == fy && m_light.Position.z == fz)
		return;

	m_light.Position.x = fx;
	m_light.Position.y = fy;
	m_light.Position.z = fz;
	m_isEdited = TRUE;
}

void CLight::SetDistance(float fDistance)
{
	m_fDistance = fDistance;
}

void CLight::BlendDiffuseColor(const Math::Color & c_rColor, float fBlendTime, float fDelayTime)
{
	Math::Color Color(m_light.Diffuse);
	m_DiffuseColorTransitor.SetTransition(Color, c_rColor, ms_fCurTime + fDelayTime, fBlendTime);
}

void CLight::BlendAmbientColor(const Math::Color & c_rColor, float fBlendTime, float fDelayTime)
{
	Math::Color Color(m_light.Ambient);
	m_AmbientColorTransitor.SetTransition(Color, c_rColor, ms_fCurTime + fDelayTime, fBlendTime);
}

void CLight::BlendRange(float fRange, float fBlendTime, float fDelayTime)
{
	m_RangeTransitor.SetTransition(m_light.Range, fRange, ms_fCurTime + fDelayTime, fBlendTime);
}

void CLight::Update()
{
	if (m_AmbientColorTransitor.isActiveTime(ms_fCurTime))
	{
		if (!m_AmbientColorTransitor.isActive())
		{
			m_AmbientColorTransitor.SetActive();
			m_AmbientColorTransitor.SetSourceValue(m_light.Ambient);
		}
		else
		{
			Math::Color Color;

			m_AmbientColorTransitor.GetValue(ms_fCurTime, &Color);
			SetAmbientColor(Color.r, Color.g, Color.b, Color.a);
		}
	}

	if (m_DiffuseColorTransitor.isActiveTime(ms_fCurTime))
	{
		if (!m_DiffuseColorTransitor.isActive())
		{
			m_DiffuseColorTransitor.SetActive();
			m_DiffuseColorTransitor.SetSourceValue(m_light.Diffuse);
		}
		else
		{
			Math::Color Color;
			m_DiffuseColorTransitor.GetValue(ms_fCurTime, &Color);
			SetDiffuseColor(Color.r, Color.g, Color.b, Color.a);
		}
	}

	if (m_RangeTransitor.isActiveTime(ms_fCurTime))
	{
		if (!m_RangeTransitor.isActive())
		{
			m_RangeTransitor.SetActive();
			m_RangeTransitor.SetSourceValue(m_light.Range);
		}
		else
		{
			float fRange;
			m_RangeTransitor.GetValue(ms_fCurTime, &fRange);
			SetRange(fRange);
		}
	}
}
