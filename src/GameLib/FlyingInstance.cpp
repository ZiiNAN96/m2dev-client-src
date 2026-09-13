#include "Stdafx.h"
#include "EterLib/GrpMath.h"
#include "EffectLib/EffectManager.h"

#include "MapManager.h"

#include "FlyingData.h"
#include "FlyTrace.h"
#include "FlyingInstance.h"
#include "FlyingObjectManager.h"
#include "FlyTarget.h"
#include "FlyHandler.h"

CDynamicPool<CFlyingInstance> CFlyingInstance::ms_kPool;

CFlyingInstance::CFlyingInstance()
{
	__Initialize();
}

CFlyingInstance::~CFlyingInstance()
{
	Destroy();
}
	
void CFlyingInstance::__Initialize()
{
	m_qAttachRotation=m_qRot=Math::Quaternion(0.0f, 0.0f, 0.0f, 0.0f);
	m_v3Accel=m_v3LocalVelocity=m_v3Velocity=m_v3Position=Math::Vector3(0.0f, 0.0f, 0.0f);
	
	m_pHandler=NULL;
	m_pData=NULL;
	m_pOwner=NULL;

	m_bAlive=false;
	m_canAttack=false;

	m_dwSkillIndex = 0;

	m_iPierceCount=0;
	
	m_fStartTime=0.0f;
	m_fRemainRange=0.0f;

	m_bTargetHitted = FALSE;
	m_HittedObjectSet.clear();
}

void CFlyingInstance::Clear()
{
	Destroy();
}

void CFlyingInstance::Destroy()
{
	m_FlyTarget.Clear();

	ClearAttachInstance();

	__Initialize();
}


void CFlyingInstance::BuildAttachInstance()
{
	for(int i=0;i<m_pData->GetAttachDataCount();i++)
	{
		CFlyingData::TFlyingAttachData & rfad = m_pData->GetAttachDataReference(i);

		switch(rfad.iType)
		{
			case CFlyingData::FLY_ATTACH_OBJECT:
				// NOT Implemented
				assert(!"FlyingInstance.cpp:BuildAttachInstance Not implemented FLY_ATTACH_OBJECT");
				break;
			case CFlyingData::FLY_ATTACH_EFFECT:
				{
					CEffectManager & rem = CEffectManager::Instance();
					TAttachEffectInstance aei;

					DWORD dwCRC = GetCaseCRC32(rfad.strFilename.c_str(),rfad.strFilename.size());

					aei.iAttachIndex = i;
					aei.dwEffectInstanceIndex = rem.GetEmptyIndex();

					aei.pFlyTrace = NULL;
					if (rfad.bHasTail)
					{
						aei.pFlyTrace = CFlyTrace::New();
						aei.pFlyTrace->Create(rfad);
					}
					rem.CreateEffectInstance(aei.dwEffectInstanceIndex,dwCRC);

					m_vecAttachEffectInstance.push_back(aei);
				}
				break;
		}
	}
}

void CFlyingInstance::Create(CFlyingData* pData, const Math::Vector3& c_rv3StartPos, const CFlyTarget & c_rkTarget, bool canAttack)
{
	m_FlyTarget = c_rkTarget;
	m_canAttack = canAttack;

	__SetDataPointer(pData, c_rv3StartPos);
	__SetTargetDirection(m_FlyTarget);
}

void CFlyingInstance::__SetTargetDirection(const CFlyTarget& c_rkTarget)
{
	Math::Vector3 v3TargetPos=c_rkTarget.GetFlyTargetPosition();

	// 임시 코드
	if (m_pData->m_bMaintainParallel)
	{
		v3TargetPos.z += 50.0f;
	}

	Math::Vector3 v3TargetDir=v3TargetPos-m_v3Position;

	Math::Vec3Normalize(&v3TargetDir, &v3TargetDir);
	__SetTargetNormalizedDirection(v3TargetDir);
}

void CFlyingInstance::__SetTargetNormalizedDirection(const Math::Vector3 & v3NomalizedDirection)
{
	Math::Quaternion q = SafeRotationNormalizedArc(Math::Vector3(0.0f,-1.0f,0.0f),v3NomalizedDirection);
	Math::QuaternionMultiply(&m_qRot,&m_qRot,&q);
	Vec3TransformQuaternion(&m_v3Velocity,&m_v3LocalVelocity,&m_qRot);
	Vec3TransformQuaternion(&m_v3Accel, &m_pData->m_v3Accel, &m_qRot);
}

// 2004. 3. 26. myevan. 기능을 몰라 일단 주석 처리. 적절한 네이밍이 필요. 게임에서 사용하지 않는다면 툴에서 툴 전용으로 상속받아 만들도록 하자
void CFlyingInstance::SetFlyTarget(const CFlyTarget & cr_Target)
{
	//m_pFlyTarget = pTarget;
	m_FlyTarget = cr_Target;
	//SetStartTargetPosition(m_FlyTarget.GetFlyTargetPosition());

	__SetTargetDirection(m_FlyTarget);
}

void CFlyingInstance::AdjustDirectionForHoming(const Math::Vector3 & v3TargetPosition)
{
	Math::Vector3 vTargetDir(v3TargetPosition);
	vTargetDir -= m_v3Position;
	Math::Vec3Normalize(&vTargetDir,&vTargetDir);
	Math::Vector3 vVel;
	Math::Vec3Normalize(&vVel, &m_v3Velocity);

	const auto vv1 = (vVel - vTargetDir);
	if (Math::Vec3LengthSq(&vv1) < 0.001f)
		return;
	
	Math::Quaternion q = SafeRotationNormalizedArc(vVel,vTargetDir);

	if (m_pData->m_fHomingMaxAngle > 180)
	{
		Vec3TransformQuaternionSafe(&m_v3Velocity, &m_v3Velocity, &q);
		Vec3TransformQuaternionSafe(&m_v3Accel, &m_v3Accel, &q);
		Math::QuaternionMultiply(&m_qRot, &q, &m_qRot);
		return;
	}

	float c = cosf(Math::ToRadian(m_pData->m_fHomingMaxAngle));
	float s = sinf(Math::ToRadian(m_pData->m_fHomingMaxAngle));

	if (q.w <= -1.0f + 0.0001f)
	{
		q.x = 0;
		q.y = 0;
		q.z = s;
		q.w = c;
	}
	else if (q.w <= c && q.w <= 1.0f - 0.0001f)
	{
		float factor = s / sqrtf(1.0f - q.w * q.w);
		q.x *= factor;
		q.y *= factor;
		q.z *= factor;
		q.w = c;
	}
	/*else
	{
	}*/
	Vec3TransformQuaternionSafe(&m_v3Velocity, &m_v3Velocity, &q);
	Vec3TransformQuaternionSafe(&m_v3Accel, &m_v3Accel, &q);
	Math::QuaternionMultiply(&m_qRot, &m_qRot, &q);
}

void CFlyingInstance::UpdateAttachInstance()
{
	// Update Attach Rotation
	Math::Quaternion q;
	Math::QuaternionRotationYawPitchRoll(&q,
		Math::ToRadian(m_pData->m_v3AngVel.y)*CTimer::Instance().GetElapsedSecond(),
		Math::ToRadian(m_pData->m_v3AngVel.x)*CTimer::Instance().GetElapsedSecond(),
		Math::ToRadian(m_pData->m_v3AngVel.z)*CTimer::Instance().GetElapsedSecond());

	Math::QuaternionMultiply(&m_qAttachRotation, &m_qAttachRotation, &q);
	Math::QuaternionMultiply(&q, &m_qAttachRotation, &m_qRot);

	CEffectManager & rem = CEffectManager::Instance();
	TAttachEffectInstanceVector::iterator it;
	for(it = m_vecAttachEffectInstance.begin();it!=m_vecAttachEffectInstance.end();++it)
	{
		CFlyingData::TFlyingAttachData & rfad = m_pData->GetAttachDataReference(it->iAttachIndex);
		assert(rfad.iType == CFlyingData::FLY_ATTACH_EFFECT);
		rem.SelectEffectInstance(it->dwEffectInstanceIndex);
		Math::Matrix m;
		switch(rfad.iFlyType)
		{
			case CFlyingData::FLY_ATTACH_TYPE_LINE:
				Math::MatrixRotationQuaternion(&m,&m_qRot);
				//Math::MatrixRotationQuaternion(&m,&q);
				m._41=m_v3Position.x;
				m._42=m_v3Position.y;
				m._43=m_v3Position.z;
				break;
			case CFlyingData::FLY_ATTACH_TYPE_MULTI_LINE:
				{
					Math::Vector3 p(
						-sinf(Math::ToRadian(rfad.fRoll))*rfad.fDistance,
						0.0f,
						-cosf(Math::ToRadian(rfad.fRoll))*rfad.fDistance);
					//Vec3TransformQuaternionSafe(&p,&p,&m_qRot);
					Vec3TransformQuaternionSafe(&p,&p,&q);
					p+=m_v3Position;
					//Math::MatrixRotationQuaternion(&m,&m_qRot);
					Math::MatrixRotationQuaternion(&m,&q);
					m._41=p.x;
					m._42=p.y;
					m._43=p.z;
				}
				break;
			case CFlyingData::FLY_ATTACH_TYPE_SINE:
				{
					//Tracenf("%f",CTimer::Instance().GetCurrentSecond());
					float angle = (CTimer::Instance().GetCurrentSecond() - m_fStartTime)*2*3.1415926535897931f/rfad.fPeriod;
					Math::Vector3 p(
						-sinf(Math::ToRadian(rfad.fRoll))*rfad.fAmplitude*sinf(angle),
						0.0f,
						-cosf(Math::ToRadian(rfad.fRoll))*rfad.fAmplitude*sinf(angle));
					Vec3TransformQuaternionSafe(&p,&p,&q);
					//Vec3TransformQuaternionSafe(&p,&p,&m_qRot);
					p+=m_v3Position;
					Math::MatrixRotationQuaternion(&m,&q);
					//Math::MatrixRotationQuaternion(&m,&m_qRot);
					m._41=p.x;
					m._42=p.y;
					m._43=p.z;
					//assert(!"NOT IMPLEMENTED");
				}
				break;
			case CFlyingData::FLY_ATTACH_TYPE_EXP:
				{
					float dt = CTimer::Instance().GetCurrentSecond() - m_fStartTime;
					float angle = dt/rfad.fPeriod;
					Math::Vector3 p(
						-sinf(Math::ToRadian(rfad.fRoll))*rfad.fAmplitude*exp(-angle)*angle,
						0.0f,
						-cosf(Math::ToRadian(rfad.fRoll))*rfad.fAmplitude*exp(-angle)*angle);
					//Vec3TransformQuaternionSafe(&p,&p,&m_qRot);
					Vec3TransformQuaternionSafe(&p,&p,&q);
					p+=m_v3Position;
					Math::MatrixRotationQuaternion(&m,&q);
					//Math::MatrixRotationQuaternion(&m,&m_qRot);
					m._41=p.x;
					m._42=p.y;
					m._43=p.z;
					//assert(!"NOT IMPLEMENTED");
				}
				break;
		}
		rem.SetEffectInstanceGlobalMatrix(m);
		if (it->pFlyTrace)
			it->pFlyTrace->UpdateNewPosition(Math::Vector3(m._41,m._42,m._43));
	}
}
struct FCheckBackgroundDuringFlying {
	CDynamicSphereInstance s;
	bool bHit;
	FCheckBackgroundDuringFlying(const Math::Vector3 & v1, const Math::Vector3 & v2)
	{
		s.fRadius = 1.0f;
		s.v3LastPosition = v1;
		s.v3Position = v2;
		bHit = false;
	}
	void operator () (CGraphicObjectInstance * p)
	{
		if (!p)
			return;

		if (!bHit && p->GetType() != ACTOR_OBJECT)
		{
			if (p->CollisionDynamicSphere(s))
			{
				bHit = true; 
			}
		}
	}
	bool IsHitted()
	{
		return bHit;
	}
};

struct FCheckAnotherMonsterDuringFlying {
	CDynamicSphereInstance s;
	CGraphicObjectInstance * pInst;
	const IActorInstance * pOwner;
	FCheckAnotherMonsterDuringFlying(const IActorInstance * pOwner, const Math::Vector3 & v1, const Math::Vector3 & v2)
		: pOwner(pOwner)
	{
		s.fRadius = 10.0f;
		s.v3LastPosition = v1;
		s.v3Position = v2;
		pInst = 0;
	}
	void operator () (CGraphicObjectInstance * p)
	{
		if (!p)
			return;

		if (!pInst && p->GetType() == ACTOR_OBJECT)
		{
			IActorInstance * pa = (IActorInstance*) p;
			if (pa != pOwner && pa->TestCollisionWithDynamicSphere(s))
			{
				pInst = p; 
			}
		}
	}
	bool IsHitted()
	{
		return pInst!=0;
	}
	CGraphicObjectInstance * GetHittedObject()
	{
		return pInst;
	}
};


bool CFlyingInstance::Update()
{
	if (!m_bAlive)
		return false;

	if (m_pData->m_bIsHoming &&
		m_pData->m_fHomingStartTime + m_fStartTime < CTimer::Instance().GetCurrentSecond())
	{
		if (m_FlyTarget.IsObject())
			AdjustDirectionForHoming(m_FlyTarget.GetFlyTargetPosition());
	}

	Math::Vector3 v3LastPosition = m_v3Position;

	m_v3Velocity += m_v3Accel*CTimer::Instance().GetElapsedSecond();
	m_v3Velocity.z+=m_pData->m_fGravity * CTimer::Instance().GetElapsedSecond();
	Math::Vector3 v3Movement = m_v3Velocity * CTimer::Instance().GetElapsedSecond();
	float _fMoveDistance = Math::Vec3Length(&v3Movement);
	float fCollisionSphereRadius = std::max(_fMoveDistance*2, m_pData->m_fCollisionSphereRadius);
	m_fRemainRange -= _fMoveDistance;
	m_v3Position += v3Movement;

	UpdateAttachInstance();

	if (m_fRemainRange<0)
	{
		if (m_pHandler)
			m_pHandler->OnExplodingOutOfRange();

		__Explode(false);
		return false;
	}

	if (m_FlyTarget.IsObject())
	{
		if (!m_bTargetHitted)
		{
			if (square_distance_between_linesegment_and_point(m_v3Position,v3LastPosition,m_FlyTarget.GetFlyTargetPosition())<m_pData->m_fBombRange*m_pData->m_fBombRange)
			{
				m_bTargetHitted = TRUE;

				if (m_canAttack)
				{
					IFlyTargetableObject* pVictim=m_FlyTarget.GetFlyTarget();
					if (pVictim)
					{
						pVictim->OnShootDamage();
					}
				}

				if (m_pHandler)
				{
					m_pHandler->OnExplodingAtTarget(m_dwSkillIndex);
				}

				if (m_iPierceCount)
				{
					m_iPierceCount--;
					__Bomb();
				}
				else
				{
					__Explode();
					return false;
				}

				return true;
			}
		}
	}
	else if (m_FlyTarget.IsPosition())
	{
		if (square_distance_between_linesegment_and_point(m_v3Position,v3LastPosition,m_FlyTarget.GetFlyTargetPosition())<m_pData->m_fBombRange*m_pData->m_fBombRange)
		{
			__Explode();
			return false;
		}
	}

	Vector3d vecStart, vecDir;
	vecStart.Set(v3LastPosition.x,v3LastPosition.y,v3LastPosition.z);
	vecDir.Set(v3Movement.x,v3Movement.y,v3Movement.z);

	CCullingManager & rkCullingMgr = CCullingManager::Instance();

	if (m_pData->m_bHitOnAnotherMonster)
	{
		FCheckAnotherMonsterDuringFlying kCheckAnotherMonsterDuringFlying(m_pOwner, v3LastPosition,m_v3Position);
		rkCullingMgr.ForInRange(vecStart,fCollisionSphereRadius, &kCheckAnotherMonsterDuringFlying);
		if (kCheckAnotherMonsterDuringFlying.IsHitted())
		{
			IActorInstance * pHittedInstance = (IActorInstance*)kCheckAnotherMonsterDuringFlying.GetHittedObject();
			if (m_HittedObjectSet.end() == m_HittedObjectSet.find(pHittedInstance))
			{
				m_HittedObjectSet.insert(pHittedInstance);

				if (m_pHandler)
				{
					m_pHandler->OnExplodingAtAnotherTarget(m_dwSkillIndex, pHittedInstance->GetVirtualID());
				}

				if (m_iPierceCount)
				{
					m_iPierceCount--;
					__Bomb();
				}
				else
				{
					__Explode();
					return false;
				}

				return true;
			}
		}
	}

	if (m_pData->m_bHitOnBackground)
	{
		// 지형 충돌

		if (CFlyingManager::Instance().GetMapManagerPtr())
		{
			float fGroundHeight = CFlyingManager::Instance().GetMapManagerPtr()->GetTerrainHeight(m_v3Position.x,-m_v3Position.y);
			if (fGroundHeight>m_v3Position.z)
			{
				if (m_pHandler)
					m_pHandler->OnExplodingAtBackground();

				__Explode();
				return false;
			}
		}

		// 건물+나무 충돌

		FCheckBackgroundDuringFlying kCheckBackgroundDuringFlying(v3LastPosition,m_v3Position);
		rkCullingMgr.ForInRange(vecStart,fCollisionSphereRadius, &kCheckBackgroundDuringFlying);
		
		if (kCheckBackgroundDuringFlying.IsHitted())
		{
			if (m_pHandler)
				m_pHandler->OnExplodingAtBackground();

			__Explode();
			return false;
		}
	}	

	return true;
}

void CFlyingInstance::ClearAttachInstance()
{
	CEffectManager & rkEftMgr = CEffectManager::Instance();

	TAttachEffectInstanceVector::iterator i;
	for(i = m_vecAttachEffectInstance.begin();i!=m_vecAttachEffectInstance.end();++i)
	{
		rkEftMgr.DestroyEffectInstance(i->dwEffectInstanceIndex);

		if (i->pFlyTrace)
			CFlyTrace::Delete(i->pFlyTrace);

		i->iAttachIndex=0;
		i->dwEffectInstanceIndex=0;
		i->pFlyTrace=NULL;
	}
	m_vecAttachEffectInstance.clear();
}

void CFlyingInstance::__Explode(bool bBomb)
{
	if (!m_bAlive)
		return;

	m_bAlive = false;

	if (bBomb)
		__Bomb();
}

void CFlyingInstance::__Bomb()
{
	CEffectManager & rkEftMgr = CEffectManager::Instance();
	if (!m_pData->m_dwBombEffectID)
		return;

	DWORD dwEmptyIndex = rkEftMgr.GetEmptyIndex();
	rkEftMgr.CreateEffectInstance(dwEmptyIndex,m_pData->m_dwBombEffectID);

	Math::Matrix m;
//	Math::MatrixRotationQuaternion(&m,&m_qRot);
	Math::MatrixIdentity(&m);
	m._41 = m_v3Position.x;
	m._42 = m_v3Position.y;
	m._43 = m_v3Position.z;
	rkEftMgr.SelectEffectInstance(dwEmptyIndex);
	rkEftMgr.SetEffectInstanceGlobalMatrix(m);
}

void CFlyingInstance::Render()
{
	if (!m_bAlive)
		return;
	RenderAttachInstance();
}

void CFlyingInstance::RenderAttachInstance()
{
	TAttachEffectInstanceVector::iterator it;
	for(it = m_vecAttachEffectInstance.begin();it!=m_vecAttachEffectInstance.end();++it)
	{
		if (it->pFlyTrace)
			it->pFlyTrace->Render();
	}
}

void CFlyingInstance::SetDataPointer(CFlyingData * pData, const Math::Vector3 & v3StartPosition)
{
	__SetDataPointer(pData, v3StartPosition);
}

void CFlyingInstance::__SetDataPointer(CFlyingData * pData, const Math::Vector3 & v3StartPosition)
{
	m_pData = pData;
	m_qRot = Math::Quaternion(0.0f,0.0f,0.0f,1.0f),
	m_v3Position = (v3StartPosition);
	m_bAlive = (true);

	m_fStartTime = CTimer::Instance().GetCurrentSecond();

	Math::QuaternionRotationYawPitchRoll(&m_qRot,Math::ToRadian(pData->m_fRollAngle-90.0f),0.0f,Math::ToRadian(pData->m_fConeAngle));
	if (pData->m_bSpreading)
	{
		Math::Quaternion q1, q2;
		const auto vv1 = Math::Vector3(0.0f, 0.0f, 1.0f), vv2 = Math::Vector3(0.0f, -1.0f, 0.0f);
		Math::QuaternionRotationAxis(&q2, &vv1,(frandom(-3.141592f/3,+3.141592f/3)+frandom(-3.141592f/3,+3.141592f/3))/2);
		Math::QuaternionRotationAxis(&q1, &vv2, frandom(0,2*3.1415926535897931f));
		Math::QuaternionMultiply(&q1,&q2,&q1);
		Math::QuaternionMultiply(&m_qRot,&q1,&m_qRot);
	}
	m_v3Velocity = m_v3LocalVelocity = Math::Vector3(0.0f,-pData->m_fInitVel,0.0f);
	m_v3Accel = pData->m_v3Accel;
	m_fRemainRange = pData->m_fRange;
	m_qAttachRotation = Math::Quaternion(0.0f,0.0f,0.0f,1.0f);

	BuildAttachInstance();
	UpdateAttachInstance();

	m_iPierceCount = pData->m_iPierceCount;
}
