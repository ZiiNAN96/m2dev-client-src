#include "StdAfx.h"
#include "Renderer/TerrainPresentation.h"
#include "EterLib/DrawStateView.h"
#include "EterBase/Utils.h"
#include "EterBase/Timer.h"
#include "GrpBase.h"
#include "Camera.h"
#include "DrawState.h"

void PixelPositionToWorldPosition(const Math::Vector3& c_rkPPosSrc, Math::Vector3* pv3Dst)
{
	pv3Dst->x=+c_rkPPosSrc.x;
	pv3Dst->y=-c_rkPPosSrc.y;
	pv3Dst->z=+c_rkPPosSrc.z;
}

void WorldPositionToPixelPosition(const Math::Vector3& c_rv3Src, Math::Vector3* pv3Dst)
{
	pv3Dst->x=+c_rv3Src.x;
	pv3Dst->y=-c_rv3Src.y;
	pv3Dst->z=+c_rv3Src.z;
}




Math::MatrixStack CGraphicBase::ms_matrixStack;

Math::Viewport			CGraphicBase::ms_Viewport;

HRESULT					CGraphicBase::ms_hLastResult = NULL;

int						CGraphicBase::ms_iWidth;
int						CGraphicBase::ms_iHeight;

DWORD					CGraphicBase::ms_faceCount = 0;









Math::Matrix				CGraphicBase::ms_matIdentity;

Math::Matrix				CGraphicBase::ms_matView;
Math::Matrix				CGraphicBase::ms_matProj;
Math::Matrix				CGraphicBase::ms_matInverseView;
Math::Matrix				CGraphicBase::ms_matInverseViewYAxis;

Math::Matrix				CGraphicBase::ms_matWorld;
Math::Matrix				CGraphicBase::ms_matWorldView;

Math::Matrix				CGraphicBase::ms_matScreen0;
Math::Matrix				CGraphicBase::ms_matScreen1;
Math::Matrix				CGraphicBase::ms_matScreen2;

Math::Vector3				CGraphicBase::ms_vtPickRayOrig;
Math::Vector3				CGraphicBase::ms_vtPickRayDir;

float					CGraphicBase::ms_fFieldOfView;
float					CGraphicBase::ms_fNearY;
float					CGraphicBase::ms_fFarY;
float					CGraphicBase::ms_fAspect;

DWORD					CGraphicBase::ms_dwWavingEndTime;
int						CGraphicBase::ms_iWavingPower;
DWORD					CGraphicBase::ms_dwFlashingEndTime;
Math::Color				CGraphicBase::ms_FlashingColor;

// Terrain picking용 Ray... CCamera 이용하는 버전.. 기존의 Ray와 통합 필요...
CRay					CGraphicBase::ms_Ray;
bool					CGraphicBase::ms_bSupportDXT = true;
bool					CGraphicBase::ms_isLowTextureMemory = false;
bool					CGraphicBase::ms_isHighTextureMemory = false;

// 2004.11.18.myevan.DynamicVertexBuffer로 교체
/*
std::vector<TIndex>		CGraphicBase::ms_lineIdxVector;
std::vector<TIndex>		CGraphicBase::ms_lineTriIdxVector;
std::vector<TIndex>		CGraphicBase::ms_lineRectIdxVector;
std::vector<TIndex>		CGraphicBase::ms_lineCubeIdxVector;

std::vector<TIndex>		CGraphicBase::ms_fillTriIdxVector;
std::vector<TIndex>		CGraphicBase::ms_fillRectIdxVector;
std::vector<TIndex>		CGraphicBase::ms_fillCubeIdxVector;
*/








bool CGraphicBase::IsLowTextureMemory()
{
	return ms_isLowTextureMemory;
}

bool CGraphicBase::IsHighTextureMemory()
{
	return ms_isHighTextureMemory;
}

bool CGraphicBase::IsFastTNL()
{
    return true; // Production rendering uses Diligent GPU transforms.
}

bool CGraphicBase::IsTLVertexClipping()
{
    return true;
}

void CGraphicBase::GetBackBufferSize(UINT* puWidth, UINT* puHeight)
{
    *puWidth=ms_iWidth; *puHeight=ms_iHeight;
}



bool CGraphicBase::ValidatePDTVertices(SPDTVertex* pVertices, UINT uVtxCount)
{
	return ValidatePDTVertices((SPDTVertexRaw*)pVertices, uVtxCount);
}

bool CGraphicBase::ValidatePDTVertices(SPDTVertexRaw* pSrcVertices, UINT uVtxCount)
{
    return pSrcVertices && uVtxCount && uVtxCount < PDT_VERTEX_NUM;
}

DWORD CGraphicBase::GetAvailableTextureMemory()
{
    return 0; // D3D11 has no equivalent free-texture-memory query; unknown, not a fabricated budget.
}

const Math::Matrix& CGraphicBase::GetViewMatrix()
{
	return ms_matView;
}

const Math::Matrix & CGraphicBase::GetIdentityMatrix()
{
	return ms_matIdentity;
}

void CGraphicBase::SetEyeCamera(float xEye, float yEye, float zEye,
								float xCenter, float yCenter, float zCenter,
								float xUp, float yUp, float zUp)
{
	Math::Vector3 vectorEye(xEye, yEye, zEye);
	Math::Vector3 vectorCenter(xCenter, yCenter, zCenter);
	Math::Vector3 vectorUp(xUp, yUp, zUp);

//	CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_PERSPECTIVE_CAMERA);
	CCameraManager::Instance().GetCurrentCamera()->SetViewParams(vectorEye, vectorCenter, vectorUp);
	UpdateViewMatrix();
}

void CGraphicBase::SetSimpleCamera(float x, float y, float z, float pitch, float roll)
{
	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	Math::Vector3 vectorEye(x, y, z);

	pCamera->SetViewParams(Math::Vector3(0.0f, y, 0.0f), Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 0.0f, 1.0f));
	pCamera->RotateEyeAroundTarget(pitch, roll);
	pCamera->Move(vectorEye);

	UpdateViewMatrix();

	// This is levites's virtual(?) code which you should not trust.
	DrawStateView().GetTransform(Renderer::MatrixWorld, &ms_matWorld);
	Math::MatrixMultiply(&ms_matWorldView, &ms_matWorld, &ms_matView);
}

void CGraphicBase::SetAroundCamera(float distance, float pitch, float roll, float lookAtZ)
{
	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	pCamera->SetViewParams(Math::Vector3(0.0f, -distance, 0.0f), Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 0.0f, 1.0f));
	pCamera->RotateEyeAroundTarget(pitch, roll);
	Math::Vector3 v3Target = pCamera->GetTarget();
	v3Target.z = lookAtZ;
	pCamera->SetTarget(v3Target);
// 	pCamera->Move(v3Target);

	UpdateViewMatrix();

	// This is levites's virtual(?) code which you should not trust.
	DrawStateView().GetTransform(Renderer::MatrixWorld, &ms_matWorld);
	Math::MatrixMultiply(&ms_matWorldView, &ms_matWorld, &ms_matView);
}

void CGraphicBase::SetPositionCamera(float fx, float fy, float fz, float distance, float pitch, float roll)
{
	// I wanna downward this code to the game control level. - [levites]
	if (ms_dwWavingEndTime > CTimer::Instance().GetCurrentMillisecond())
	{
		if (ms_iWavingPower>0)
		{
			fx += float(rand() % ms_iWavingPower) / 10.0f;
			fy += float(rand() % ms_iWavingPower) / 10.0f;
			fz += float(rand() % ms_iWavingPower) / 10.0f;
		}
	}

	CCamera * pCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCamera)
		return;

	pCamera->SetViewParams(Math::Vector3(0.0f, -distance, 0.0f), Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 0.0f, 1.0f));
	pitch = fMIN(80.0f, fMAX(-80.0f, pitch) );
//	Tracef("SetPosition Camera : %f, %f\n", pitch, roll);
	pCamera->RotateEyeAroundTarget(pitch, roll);
	pCamera->Move(Math::Vector3(fx, fy, fz));

	UpdateViewMatrix();

	// This is levites's virtual(?) code which you should not trust.
	DRAWSTATE.GetTransform(Renderer::MatrixWorld, &ms_matWorld);
	Math::MatrixMultiply(&ms_matWorldView, &ms_matWorld, &ms_matView);
}

void CGraphicBase::SetOrtho2D(float hres, float vres, float zres)
{
	//CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_ORTHO_CAMERA);
	Math::MatrixOrthoOffCenterRH(&ms_matProj, 0, hres, vres, 0, 0, zres);
	//UpdatePipeLineMatrix();
	UpdateProjMatrix();
}

void CGraphicBase::SetOrtho3D(float hres, float vres, float zmin, float zmax)
{
	//CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_PERSPECTIVE_CAMERA);
	Math::MatrixOrthoRH(&ms_matProj, hres, vres, zmin, zmax);
	//UpdatePipeLineMatrix();
	UpdateProjMatrix();
}

void CGraphicBase::SetPerspective(float fov, float aspect, float nearz, float farz)
{
	ms_fFieldOfView = fov;




	//else
	ms_fAspect = aspect;

	ms_fNearY = nearz;
	ms_fFarY = farz;

	//CCameraManager::Instance().SetCurrentCamera(CCameraManager::DEFAULT_PERSPECTIVE_CAMERA);
	Math::MatrixPerspectiveFovRH(&ms_matProj, Math::ToRadian(fov), ms_fAspect, nearz, farz);
	//UpdatePipeLineMatrix();
	UpdateProjMatrix();
}

void CGraphicBase::UpdateProjMatrix()
{
	DRAWSTATE.SetTransform(Renderer::MatrixProjection, &ms_matProj);
}

void CGraphicBase::UpdateViewMatrix()
{
	CCamera* pkCamera=CCameraManager::Instance().GetCurrentCamera();
	if (!pkCamera)
		return;

	ms_matView = pkCamera->GetViewMatrix();
	DRAWSTATE.SetTransform(Renderer::MatrixView, &ms_matView);

	Math::MatrixInverse(&ms_matInverseView, NULL, &ms_matView);
	ms_matInverseViewYAxis._11 = ms_matInverseView._11;
	ms_matInverseViewYAxis._12 = ms_matInverseView._12;
	ms_matInverseViewYAxis._21 = ms_matInverseView._21;
	ms_matInverseViewYAxis._22 = ms_matInverseView._22;
}

void CGraphicBase::UpdatePipeLineMatrix()
{
	UpdateProjMatrix();
	UpdateViewMatrix();
}

void CGraphicBase::SetViewport(DWORD dwX, DWORD dwY, DWORD dwWidth, DWORD dwHeight, float fMinZ, float fMaxZ)
{
	ms_Viewport.X = dwX;
	ms_Viewport.Y = dwY;
	ms_Viewport.Width = dwWidth;
	ms_Viewport.Height = dwHeight;
	ms_Viewport.MinZ = fMinZ;
	ms_Viewport.MaxZ = fMaxZ;
}

void CGraphicBase::GetTargetPosition(float * px, float * py, float * pz)
{
	*px = CCameraManager::Instance().GetCurrentCamera()->GetTarget().x;
	*py = CCameraManager::Instance().GetCurrentCamera()->GetTarget().y;
	*pz = CCameraManager::Instance().GetCurrentCamera()->GetTarget().z;
}

void CGraphicBase::GetCameraPosition(float * px, float * py, float * pz)
{
	*px = CCameraManager::Instance().GetCurrentCamera()->GetEye().x;
	*py = CCameraManager::Instance().GetCurrentCamera()->GetEye().y;
	*pz = CCameraManager::Instance().GetCurrentCamera()->GetEye().z;
}

void CGraphicBase::GetMatrix(Math::Matrix* pRetMatrix) const
{

	*pRetMatrix = *ms_matrixStack.GetTop();
}

const Math::Matrix* CGraphicBase::GetMatrixPointer() const
{

	return ms_matrixStack.GetTop();
}

void CGraphicBase::GetSphereMatrix(Math::Matrix * pMatrix, float fValue)
{
	Math::MatrixIdentity(pMatrix);
	pMatrix->_11 = fValue * ms_matWorldView._11;
	pMatrix->_21 = fValue * ms_matWorldView._21;
	pMatrix->_31 = fValue * ms_matWorldView._31;
	pMatrix->_41 = fValue;
	pMatrix->_12 = -fValue * ms_matWorldView._12;
	pMatrix->_22 = -fValue * ms_matWorldView._22;
	pMatrix->_32 = -fValue * ms_matWorldView._32;
	pMatrix->_42 = -fValue;
}

float CGraphicBase::GetFOV()
{
	return ms_fFieldOfView;
}

void CGraphicBase::PushMatrix()
{
	ms_matrixStack.Push();
}

void CGraphicBase::Scale(float x, float y, float z)
{
	ms_matrixStack.Scale(x, y, z);
}

void CGraphicBase::Rotate(float degree, float x, float y, float z)
{
	Math::Vector3 vec(x, y, z);
	ms_matrixStack.RotateAxis(&vec, Math::ToRadian(degree));
}

void CGraphicBase::RotateLocal(float degree, float x, float y, float z)
{
	Math::Vector3 vec(x, y, z);
	ms_matrixStack.RotateAxisLocal(&vec, Math::ToRadian(degree));
}

void CGraphicBase::MultMatrix( const Math::Matrix* pMat)
{
	ms_matrixStack.MultMatrix(pMat);
}

void CGraphicBase::MultMatrixLocal( const Math::Matrix* pMat)
{
	ms_matrixStack.MultMatrixLocal(pMat);
}

void CGraphicBase::RotateYawPitchRollLocal(float fYaw, float fPitch, float fRoll)
{
	ms_matrixStack.RotateYawPitchRollLocal(Math::ToRadian(fYaw), Math::ToRadian(fPitch), Math::ToRadian(fRoll));
}

void CGraphicBase::Translate(float x, float y, float z)
{
	ms_matrixStack.Translate(x, y, z);
}

void CGraphicBase::LoadMatrix(const Math::Matrix& c_rSrcMatrix)
{
	ms_matrixStack.LoadMatrix(&c_rSrcMatrix);
}

void CGraphicBase::PopMatrix()
{
	ms_matrixStack.Pop();
}

DWORD CGraphicBase::GetColor(float r, float g, float b, float a)
{
	BYTE argb[4] =
	{
		(BYTE) (255.0f * b),
		(BYTE) (255.0f * g),
		(BYTE) (255.0f * r),
		(BYTE) (255.0f * a)
	};

	return *((DWORD *) argb);
}

void CGraphicBase::InitScreenEffect()
{
	ms_dwWavingEndTime = 0;
	ms_dwFlashingEndTime = 0;
	ms_iWavingPower = 0;
	ms_FlashingColor = Math::Color(0.0f, 0.0f, 0.0f, 0.0f);
}

void CGraphicBase::SetScreenEffectWaving(float fDuringTime, int iPower)
{
	ms_dwWavingEndTime = CTimer::Instance().GetCurrentMillisecond() + long(fDuringTime * 1000.0f);
	ms_iWavingPower = iPower;
}

void CGraphicBase::SetScreenEffectFlashing(float fDuringTime, const Math::Color & c_rColor)
{
	ms_dwFlashingEndTime = CTimer::Instance().GetCurrentMillisecond() + long(fDuringTime * 1000.0f);
	ms_FlashingColor = c_rColor;
}

DWORD CGraphicBase::GetFaceCount()
{
	return ms_faceCount;
}

void CGraphicBase::ResetFaceCount()
{
	ms_faceCount = 0;
}

HRESULT CGraphicBase::GetLastResult()
{
	return ms_hLastResult;
}

CGraphicBase::CGraphicBase()
{
}

CGraphicBase::~CGraphicBase()
{
}
