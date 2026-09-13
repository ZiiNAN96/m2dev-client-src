// Decal.cpp: implementation of the CDecal class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "Decal.h"
#include "DrawState.h"

//////////////////////////////////////////////////////////////////////
// CDecal
//////////////////////////////////////////////////////////////////////

CDecal::CDecal():m_cfDecalEpsilon(0.25f)
{
	Clear();
}

CDecal::~CDecal()
{
	Clear();
}

void CDecal::Clear()
{
	m_v3Center = Math::Vector3(0.0f, 0.0f, 0.0f);
	m_v3Normal = Math::Vector3(0.0f, 0.0f, 0.0f);

	m_v4LeftPlane = Math::Plane(0.0f, 0.0f, 0.0f, 0.0f);
	m_v4RightPlane = Math::Plane(0.0f, 0.0f, 0.0f, 0.0f);
	m_v4TopPlane = Math::Plane(0.0f, 0.0f, 0.0f, 0.0f);
	m_v4BottomPlane = Math::Plane(0.0f, 0.0f, 0.0f, 0.0f);
	m_v4FrontPlane = Math::Plane(0.0f, 0.0f, 0.0f, 0.0f);
	m_v4BackPlane = Math::Plane(0.0f, 0.0f, 0.0f, 0.0f);

	m_dwVertexCount = 0;
	m_dwPrimitiveCount = 0;
	
	m_TriangleFanStructVector.clear();

	memset(m_Vertices, 0, sizeof(m_Vertices));
	memset(m_Indices, 0, sizeof(m_Indices));
}

void CDecal::ClipMesh(DWORD dwPrimitiveCount, const Math::Vector3 *c_pv3Vertex, const Math::Vector3 *c_pv3Normal)
{
	Math::Vector3		v3NewVertex[9];
	Math::Vector3		v3NewNormal[9];
	
	// Clip one triangle at a time
	for(DWORD dwi = 0; dwi < dwPrimitiveCount; ++dwi)
	{
		const Math::Vector3 & v3_1 = c_pv3Vertex[3 * dwi];
		const Math::Vector3 & v3_2 = c_pv3Vertex[3 * dwi + 1];
		const Math::Vector3 & v3_3 = c_pv3Vertex[3 * dwi + 2];
		
		Math::Vector3 v3Cross;
		const auto vv_ = (v3_2 - v3_1);
		const auto vv_2 = (v3_3 - v3_1);
		Math::Vec3Cross(&v3Cross, &vv_, &vv_2);
		if (Math::Vec3Dot(&m_v3Normal, &v3Cross) > ( m_cfDecalEpsilon ) * Math::Vec3Length(&v3Cross))
		{
			v3NewVertex[0] = v3_1;
			v3NewVertex[1] = v3_2;
			v3NewVertex[2] = v3_3;
			
			v3NewNormal[0] = c_pv3Normal[3 * dwi];
			v3NewNormal[1] = c_pv3Normal[3 * dwi + 1];
			v3NewNormal[2] = c_pv3Normal[3 * dwi + 2];
			
			DWORD dwCount = ClipPolygon(3, v3NewVertex, v3NewNormal, v3NewVertex, v3NewNormal);
			if ((dwCount != 0) && (!AddPolygon(dwCount, v3NewVertex, v3NewNormal))) break;
 		}
	}
}

bool CDecal::AddPolygon(DWORD dwAddCount, const Math::Vector3 *c_pv3Vertex, const Math::Vector3  * /*c_pv3Normal */)
{
	if (m_dwVertexCount + dwAddCount >= MAX_DECAL_VERTICES)
		return false;

	TTRIANGLEFANSTRUCT aTriangleFanStruct;
	aTriangleFanStruct.m_wMinIndex = m_dwVertexCount;
	aTriangleFanStruct.m_dwVertexCount = dwAddCount;
	aTriangleFanStruct.m_dwPrimitiveCount = dwAddCount - 2;
	aTriangleFanStruct.m_dwVBOffset = m_dwVertexCount;

	m_TriangleFanStructVector.push_back(aTriangleFanStruct);

	DWORD dwCount = m_dwVertexCount;

	// Add polygon as a triangle fan
	WORD * wIndex = m_Indices + dwCount;

	m_dwPrimitiveCount += dwAddCount - 2;
	//float fOne_over_1MinusDecalEpsilon = 1.0f / (1.0f - m_cfDecalEpsilon);
	
	// Assign vertex colors
	for (DWORD dwVertexNum = 0; dwVertexNum < dwAddCount; ++dwVertexNum)
	{
		*wIndex++ = (WORD) dwCount;
		m_Vertices[dwCount].position = c_pv3Vertex[dwVertexNum];
		//const Math::Vector3 & v3Normal = c_pv3Normal[dwVertexNum];
		//float fAlpha = (Math::Vec3Dot(&m_v3Normal, &v3Normal) / Math::Vec3Length(&v3Normal) - m_cfDecalEpsilon) * fOne_over_1MinusDecalEpsilon;
		//m_Vertices[dwCount].diffuse = Math::Color(1.0f, 1.0f, 1.0f, (fAlpha > 0.0f) ? fAlpha : 0.0f);
		m_Vertices[dwCount].diffuse = 0xFFFFFFFF;
		++dwCount;
	}

	m_dwVertexCount = dwCount;
	return true;
}

DWORD CDecal::ClipPolygon(DWORD dwVertexCount, 
						 const Math::Vector3 *c_pv3Vertex,
						 const Math::Vector3 *c_pv3Normal,
						 Math::Vector3 *c_pv3NewVertex,
						 Math::Vector3 *c_pv3NewNormal) const
{
	Math::Vector3		v3TempVertex[9];
	Math::Vector3		v3TempNormal[9];
	
	// Clip against all six planes
	DWORD dwCount = ClipPolygonAgainstPlane(m_v4LeftPlane, dwVertexCount, c_pv3Vertex, c_pv3Normal, v3TempVertex, v3TempNormal);
	if (dwCount != 0)
	{
		dwCount = ClipPolygonAgainstPlane(m_v4RightPlane, dwCount, v3TempVertex, v3TempNormal, c_pv3NewVertex, c_pv3NewNormal);
		if (dwCount != 0)
		{
			dwCount = ClipPolygonAgainstPlane(m_v4BottomPlane, dwCount, c_pv3NewVertex, c_pv3NewNormal, v3TempVertex, v3TempNormal);
			if (dwCount != 0)
			{
				dwCount = ClipPolygonAgainstPlane(m_v4TopPlane, dwCount, v3TempVertex, v3TempNormal, c_pv3NewVertex, c_pv3NewNormal);
				if (dwCount != 0)
				{
					dwCount = ClipPolygonAgainstPlane(m_v4BackPlane, dwCount, c_pv3NewVertex, c_pv3NewNormal, v3TempVertex, v3TempNormal);
					if (dwCount != 0)
					{
						dwCount = ClipPolygonAgainstPlane(m_v4FrontPlane, dwCount, v3TempVertex, v3TempNormal, c_pv3NewVertex, c_pv3NewNormal);
					}
				}
			}
		}
	}
	return dwCount;
}

DWORD CDecal::ClipPolygonAgainstPlane(const Math::Plane& c_rv4Plane,
									  DWORD dwVertexCount,
									  const Math::Vector3 *c_pv3Vertex,
									  const Math::Vector3 *c_pv3Normal,
									  Math::Vector3 *c_pv3NewVertex,
									  Math::Vector3 *c_pv3NewNormal)
{
	bool bNegative[10];
	
	// Classify vertices
	DWORD dwNegativeCount = 0;
	for (DWORD dwi = 0; dwi < dwVertexCount; ++dwi)
	{
		bool bNeg = (Math::PlaneDotCoord(&c_rv4Plane, &c_pv3Vertex[dwi]) < 0.0F);
		bNegative[dwi] = bNeg;
		dwNegativeCount += bNeg;
	}
	
	// Discard this polygon if it's completely culled
	if (dwNegativeCount == dwVertexCount)
		return 0;
	
	DWORD dwCount = 0;
	for (DWORD dwCurIndex = 0; dwCurIndex < dwVertexCount; ++dwCurIndex)
	{
		// dwPrevIndex is the index of the previous vertex
		DWORD dwPrevIndex = (dwCurIndex != 0) ? dwCurIndex - 1 : dwVertexCount - 1;
		
		if (bNegative[dwCurIndex])
		{
			if (!bNegative[dwPrevIndex])
			{
				// Current vertex is on negative side of plane,
				// but previous vertex is on positive side.
				const Math::Vector3& v3_1 = c_pv3Vertex[dwPrevIndex];
				const Math::Vector3& v3_2 = c_pv3Vertex[dwCurIndex];
				float ft = Math::PlaneDotCoord(&c_rv4Plane, &v3_1) / (c_rv4Plane.a * (v3_1.x - v3_2.x) + c_rv4Plane.b * (v3_1.y - v3_2.y) + c_rv4Plane.c * (v3_1.z - v3_2.z));
 				c_pv3NewVertex[dwCount] = v3_1 * (1.0f - ft) + v3_2 * ft;
				const Math::Vector3& v3_n1 = c_pv3Normal[dwPrevIndex];
				const Math::Vector3& v3_n2 = c_pv3Normal[dwCurIndex];
 				c_pv3NewNormal[dwCount] = v3_n1 * (1.0f - ft) + v3_n2 * ft;
				++dwCount;
			}
		}
		else
		{
			if (bNegative[dwPrevIndex])
			{
				// Current vertex is on positive side of plane,
				// but previous vertex is on negative side.
				const Math::Vector3& v3_1 = c_pv3Vertex[dwCurIndex];
				const Math::Vector3& v3_2 = c_pv3Vertex[dwPrevIndex];
				float ft = Math::PlaneDotCoord(&c_rv4Plane, &v3_1) / (c_rv4Plane.a * (v3_1.x - v3_2.x) + c_rv4Plane.b * (v3_1.y - v3_2.y) + c_rv4Plane.c * (v3_1.z - v3_2.z));
 				c_pv3NewVertex[dwCount] = v3_1 * (1.0f - ft) + v3_2 * ft;
				const Math::Vector3& v3_n1 = c_pv3Normal[dwCurIndex];
				const Math::Vector3& v3_n2 = c_pv3Normal[dwPrevIndex];
 				c_pv3NewNormal[dwCount] = v3_n1 * (1.0f - ft) + v3_n2 * ft;
				++dwCount;
			}
			
			// Include current vertex
 			c_pv3NewVertex[dwCount] = c_pv3Vertex[dwCurIndex];
 			c_pv3NewNormal[dwCount] = c_pv3Normal[dwCurIndex];
			++dwCount;
		}
	}
	
	// Return number of vertices in clipped polygon
	return dwCount;
}

/*
void CDecal::Update()
{
}
*/

void CDecal::Render()
{
	Math::Matrix matWorld;
	Math::MatrixIdentity(&matWorld);
	DRAWSTATE.SetTransform(Renderer::MatrixWorld, &matWorld);
}
