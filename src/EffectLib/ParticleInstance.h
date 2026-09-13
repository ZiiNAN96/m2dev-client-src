#pragma once
#include "Type.h"
#include "Eterlib/GrpBase.h"
#include "EterLib/Pool.h"

class CParticleProperty;
class CEmitterProperty;

class CParticleInstance
{
	friend class CParticleSystemData;
	friend class CParticleSystemInstance;

	public:
		CParticleInstance();
		~CParticleInstance();

		float GetRadiusApproximation();
		
		BOOL Update(float fElapsedTime, float fAngle);

	private:
		void UpdateRotation(float time, float elapsedTime);
		void UpdateTextureAnimation(float time, float elapsedTime);
		void UpdateScale(float time, float elapsedTime);
		void UpdateColor(float time, float elapsedTime);
		void UpdateGravity(float time, float elapsedTime);
		void UpdateAirResistance(float time, float elapsedTime);

	protected:
		Math::Vector3			m_v3StartPosition;

		Math::Vector3			m_v3Position;
		Math::Vector3			m_v3LastPosition;
		Math::Vector3			m_v3Velocity;

		Math::Vector2			m_v2HalfSize;
		Math::Vector2			m_v2Scale;

		float				m_fRotation;
		Math::Color			m_Color;

		BYTE				m_byTextureAnimationType;
		float				m_fLastFrameTime;
		BYTE				m_byFrameIndex;
		float				m_fFrameTime;

		float				m_fLifeTime;
		float				m_fLastLifeTime;

		CParticleProperty *	m_pParticleProperty;
		CEmitterProperty *	m_pEmitterProperty;

		BYTE				m_rotationType;

		float				m_fAirResistance;
		float				m_fRotationSpeed;
		float				m_fGravity;

	public:
		static CParticleInstance* New();
		static void DestroySystem();

		void Transform(const Math::Matrix * c_matLocal=NULL);
		void Transform(const Math::Matrix * c_matLocal, const float c_fZRotation);

		TPTVertex * GetParticleMeshPointer();
		
		void DeleteThis();

		void Destroy();

	protected:
		void __Initialize();
		TPTVertex			m_ParticleMesh[4];
	public:
		static CDynamicPool<CParticleInstance> ms_kPool;
		
};
