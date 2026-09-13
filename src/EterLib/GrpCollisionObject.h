#pragma once

#include "GrpBase.h"

class CGraphicCollisionObject : public CGraphicBase
{
	public:
		CGraphicCollisionObject();
		virtual ~CGraphicCollisionObject();

	protected:
		bool IntersectTriangle(const Math::Vector3& c_orig, const Math::Vector3& c_dir, const Math::Vector3& c_v0, const Math::Vector3& c_v1, const Math::Vector3& c_v2, float* pu, float* pv, float* pt);
		bool IntersectBoundBox(const Math::Matrix* c_pmatWorld, const TBoundBox& c_rboundBox, float* pu, float* pv, float* pt);
		bool IntersectCube(const Math::Matrix* c_pmatWorld, float sx, float sy, float sz, float ex, float ey, float ez, Math::Vector3 & RayOriginal, Math::Vector3 & RayDirection, float* pu, float* pv, float* pt);
		bool IntersectIndexedMesh(const Math::Matrix* c_pmatWorld, const void* vertices, int step, int vtxCount, const void* indices, int idxCount, Math::Vector3 & RayOriginal, Math::Vector3 & RayDirection, float* pu, float* pv, float* pt);
		bool IntersectMesh(const Math::Matrix * c_pmatWorld, const void * vertices, DWORD dwStep, DWORD dwvtxCount, Math::Vector3 & RayOriginal, Math::Vector3 & RayDirection, float* pu, float* pv, float* pt);

		bool IntersectSphere(const Math::Vector3 & c_rv3Position, float fRadius, const Math::Vector3 & c_rv3RayOriginal, const Math::Vector3 & c_rv3RayDirection);
		bool IntersectCylinder(const Math::Vector3 & c_rv3Position, float fRadius, float fHeight, const Math::Vector3 & c_rv3RayOriginal, const Math::Vector3 & c_rv3RayDirection);

		// NOTE : ms_vtPickRayOrig와 ms_vtPickRayDir를 CGraphicBGase가 가지고 있는데
		//        굳이 인자로 넣어줘야 하는 이유가 있는가? Customize를 위해서? - [levites]
		bool IntersectSphere(const Math::Vector3 & c_rv3Position, float fRadius);
		bool IntersectCylinder(const Math::Vector3 & c_rv3Position, float fRadius, float fHeight);
};
