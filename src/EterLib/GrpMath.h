#pragma once

float CrossProduct2D(float x1, float y1, float x2, float y2);

bool IsInTriangle2D(float ax, float ay, float bx, float by, float cx, float cy, float tx, float ty);

Math::Vector3* RotateVector3(Math::Vector3* pvtOut, const Math::Vector3* c_pvtSrc, const Math::Quaternion* c_pqtRot);
Math::Vector3* TranslateVector3(Math::Vector3* pvtOut, const Math::Vector3* c_pvtSrc, const Math::Vector3* c_pvtTrans);

void GetRotationFromMatrix(Math::Vector3 * pRotation, const Math::Matrix * c_pMatrix);
void GetPivotAndRotationFromMatrix(Math::Matrix * pMatrix, Math::Vector3 * pPivot, Math::Vector3 * pRotation);
void ExtractMovement(Math::Matrix * pTargetMatrix, Math::Matrix * pSourceMatrix);

inline Math::Vector3* BlendVector3(Math::Vector3* pvtOut, const Math::Vector3* c_pvtSrc1, const Math::Vector3* c_pvtSrc2, float d)
{
	pvtOut->x=c_pvtSrc1->x+d*(c_pvtSrc2->x-c_pvtSrc1->x);
	pvtOut->y=c_pvtSrc1->y+d*(c_pvtSrc2->y-c_pvtSrc1->y);
	pvtOut->z=c_pvtSrc1->z+d*(c_pvtSrc2->z-c_pvtSrc1->z);

	return pvtOut;
}

inline Math::Quaternion* BlendQuaternion(Math::Quaternion* pqtOut, const Math::Quaternion* c_pqtSrc1, const Math::Quaternion* c_pqtSrc2, float d)
{
	pqtOut->x=c_pqtSrc1->x+d*(c_pqtSrc2->x-c_pqtSrc1->x);
	pqtOut->y=c_pqtSrc1->y+d*(c_pqtSrc2->y-c_pqtSrc1->y);
	pqtOut->z=c_pqtSrc1->z+d*(c_pqtSrc2->z-c_pqtSrc1->z);
	pqtOut->w=c_pqtSrc1->w+d*(c_pqtSrc2->w-c_pqtSrc1->w);
	return pqtOut;
}

inline float ClampDegree(float fDegree)
{
	if (fDegree >= 360.0f)
		fDegree -= 360.0f;
	if (fDegree < 0.0f)
		fDegree += 360.0f;

	return fDegree;
}

inline float GetVector3Distance(const Math::Vector3 & c_rv3Source, const Math::Vector3 & c_rv3Target)
{
	return (c_rv3Source.x-c_rv3Target.x)*(c_rv3Source.x-c_rv3Target.x) + (c_rv3Source.y-c_rv3Target.y)*(c_rv3Source.y-c_rv3Target.y);
}

inline Math::Quaternion SafeRotationNormalizedArc(const Math::Vector3 & vFrom , const Math::Vector3 & vTo)
{
	if (vFrom == vTo)
		return Math::Quaternion(0.0f,0.0f,0.0f,1.0f);
	if (vFrom == -vTo)
		return Math::Quaternion(0.0f,0.0f,1.0f,0.0f);
	Math::Vector3 c;
	Math::Vec3Cross(&c, &vFrom, &vTo);
	float d = Math::Vec3Dot(&vFrom, &vTo);
	float s = sqrtf((1+d)*2);
	
	return Math::Quaternion(c.x/s,c.y/s,c.z/s,s*0.5f);
}

inline Math::Quaternion RotationNormalizedArc(const Math::Vector3 & vFrom , const Math::Vector3 & vTo)

{
	Math::Vector3 c;
	Math::Vec3Cross(&c, &vFrom, &vTo);
	float d = Math::Vec3Dot(&vFrom, &vTo);
	float s = sqrtf((1+d)*2);

	return Math::Quaternion(c.x/s,c.y/s,c.z/s,s*0.5f);
}

inline Math::Quaternion RotationArc(const Math::Vector3 & vFrom , const Math::Vector3 & vTo)
{
	Math::Vector3 vnFrom, vnTo;
	Math::Vec3Normalize(&vnFrom, &vFrom);
	Math::Vec3Normalize(&vnTo, &vTo);
	return RotationNormalizedArc(vnFrom, vnTo);
}

inline float square_distance_between_linesegment_and_point(const Math::Vector3& p1,const Math::Vector3& p2,const Math::Vector3& x)
{
	const auto v1 = p2 - p1;
	float l = Math::Vec3LengthSq(&v1);
	const auto v2 = x - p1;
	const auto v3 = p2 - p1;
	float d = Math::Vec3Dot(&(v2),&(v3));
	if (d<=0.0f)
	{
		return Math::Vec3LengthSq(&(v2));
	}
	else if (d>=l)
	{
		const auto v4 = x - p2;
		return Math::Vec3LengthSq(&(v4));
	}
	else
	{
		Math::Vector3 c;
		return Math::Vec3LengthSq(Math::Vec3Cross(&c,&(v2),&(v3)))/l;
	}
}

inline Math::Vector3 * Vec3TransformQuaternionSafe(Math::Vector3* pvout, const Math::Vector3* pv, const Math::Quaternion* pq)
{
	Math::Vector3 v;
	Math::Vec3Cross(&v,pv,(Math::Vector3*)pq);
	v *= -2*pq->w;
	v += (pq->w*pq->w - Math::Vec3LengthSq((Math::Vector3*)pq))*(*pv);
	v += 2*Math::Vec3Dot((Math::Vector3*)pq,pv)*(*(Math::Vector3*)pq);
	*pvout = v;
	return pvout;
}

inline Math::Vector3 * Vec3TransformQuaternion(Math::Vector3* pvout, const Math::Vector3* pv, const Math::Quaternion* pq)
{
	Math::Vec3Cross(pvout,pv,(Math::Vector3*)pq);
	*pvout *= -2*pq->w;
	*pvout += (pq->w*pq->w - Math::Vec3LengthSq((Math::Vector3*)pq))*(*pv);
	*pvout += 2*Math::Vec3Dot((Math::Vector3*)pq,pv)*(*(Math::Vector3*)pq);
	
	return pvout;
}
