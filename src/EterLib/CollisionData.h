#pragma once

// Collision Detection
typedef struct SSphereData
{
	Math::Vector3 v3Position;
	float		fRadius;
} TSphereData;

typedef struct SPlaneData
{
	Math::Vector3 v3Position;
	Math::Vector3 v3Normal;
	
	Math::Vector3 v3QuadPosition[4];
	Math::Vector3 v3InsideVector[4];
} TPlaneData;

typedef struct SAABBData
{
	Math::Vector3 v3Min;
	Math::Vector3 v3Max;

} TAABBData;

typedef struct SOBBData
{
	Math::Vector3 v3Min;
	Math::Vector3 v3Max;
	Math::Matrix matRot;

} TOBBData;

typedef struct SCylinderData
{
	Math::Vector3 v3Position;
	float fRadius;
	float fHeight;
} TCylinderData;

enum ECollisionType
{
	COLLISION_TYPE_PLANE,
	COLLISION_TYPE_BOX,
	COLLISION_TYPE_SPHERE,
	COLLISION_TYPE_CYLINDER,
	COLLISION_TYPE_AABB,
	COLLISION_TYPE_OBB,
};

struct CDynamicSphereInstance
{
	Math::Vector3 v3Position;
	Math::Vector3 v3LastPosition;

	float fRadius;
};

class CStaticCollisionData
{
public:
	DWORD dwType;
	char szName[32+1];
	
	Math::Vector3 v3Position;
	float fDimensions[3];
	Math::Quaternion quatRotation;
};

void DestroyCollisionInstanceSystem();

typedef std::vector<CStaticCollisionData> CStaticCollisionDataVector;

/////////////////////////////////////////////
// Base
class CBaseCollisionInstance
{
	public:
		virtual void Render(Renderer::FillMode fillMode = Renderer::FillSolid) = 0;

		bool MovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const
		{
			return OnMovementCollisionDynamicSphere(s);
		}
		bool CollisionDynamicSphere(const CDynamicSphereInstance & s) const
		{
			return OnCollisionDynamicSphere(s);
		}
		

		Math::Vector3 GetCollisionMovementAdjust(const CDynamicSphereInstance & s) const
		{
			return OnGetCollisionMovementAdjust(s);
		}

		void Destroy();

		static CBaseCollisionInstance * BuildCollisionInstance(const CStaticCollisionData * c_pCollisionData, const Math::Matrix * pMat);

	protected:
		virtual Math::Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const = 0;
		virtual bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const  = 0;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const  = 0;
		virtual void OnDestroy() = 0;
};

/////////////////////////////////////////////
// Sphere
class CSphereCollisionInstance : public CBaseCollisionInstance
{
	public:
		TSphereData & GetAttribute();
		const TSphereData & GetAttribute() const;
		virtual void Render(Renderer::FillMode fillMode = Renderer::FillSolid);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Math::Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TSphereData m_attribute;
};

/////////////////////////////////////////////
// Plane
class CPlaneCollisionInstance : public CBaseCollisionInstance
{
	public:
		TPlaneData & GetAttribute();
		const TPlaneData & GetAttribute() const;
		virtual void Render(Renderer::FillMode fillMode = Renderer::FillSolid);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Math::Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TPlaneData m_attribute;
};

/////////////////////////////////////////////
// AABB (Aligned Axis Bounding Box)
class CAABBCollisionInstance : public CBaseCollisionInstance
{
	public:
		TAABBData & GetAttribute();
		const TAABBData & GetAttribute() const;
		virtual void Render(Renderer::FillMode fillMode = Renderer::FillSolid);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Math::Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TAABBData m_attribute;
};

/////////////////////////////////////////////
// OBB
class COBBCollisionInstance : public CBaseCollisionInstance
{
	public:
		TOBBData & GetAttribute();
		const TOBBData & GetAttribute() const;
		virtual void Render(Renderer::FillMode fillMode = Renderer::FillSolid);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Math::Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

	protected:
		TOBBData m_attribute;
};

/////////////////////////////////////////////
// Cylinder
class CCylinderCollisionInstance : public CBaseCollisionInstance
{
	public:
		TCylinderData & GetAttribute();
		const TCylinderData & GetAttribute() const;
		virtual void Render(Renderer::FillMode fillMode = Renderer::FillSolid);

	protected:
		void OnDestroy();
		bool OnMovementCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual bool OnCollisionDynamicSphere(const CDynamicSphereInstance & s) const;
		virtual Math::Vector3 OnGetCollisionMovementAdjust(const CDynamicSphereInstance & s) const;

		bool CollideCylinderVSDynamicSphere(const TCylinderData & c_rattribute, const CDynamicSphereInstance & s) const;

	protected:
		TCylinderData m_attribute;
};

typedef std::vector<CSphereCollisionInstance> CSphereCollisionInstanceVector;
typedef std::vector<CDynamicSphereInstance> CDynamicSphereInstanceVector;
typedef std::vector<CBaseCollisionInstance*> CCollisionInstanceVector;
