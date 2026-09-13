#include "StdAfx.h"
#include "EterLib/NativeResourceAudit.h"
#include "GrpShadowTexture.h"
#include "StateManager.h"

//////////////////////////////////////////////////////////////////////////
void CGraphicShadowTexture::Destroy()
{	
	CGraphicTexture::Destroy();

	if (m_lpd3dShadowSurface)
	{
		m_lpd3dShadowSurface->Release();
		m_lpd3dShadowSurface = NULL;
	}

	if (m_lpd3dDepthSurface)
	{
		m_lpd3dDepthSurface->Release();
		m_lpd3dDepthSurface = NULL;
	}

	if (m_lpd3dShadowTexture)
	{
		m_lpd3dShadowTexture->Release();
		m_lpd3dShadowTexture = NULL;
	}

	Initialize();
}

bool CGraphicShadowTexture::Create(int width, int height)
{
    // ZiiNAN: Legacy D3D9 renderer removed from production path.
    Destroy(); return false; // Unused shadow API; no new shadow feature.
}

void CGraphicShadowTexture::Set(int stage) const
{
	STATEMANAGER.SetTexture(stage, m_lpd3dShadowTexture);
}

const D3DXMATRIX& CGraphicShadowTexture::GetLightVPMatrixReference() const
{
	return m_d3dLightVPMatrix;
}

LPDIRECT3DTEXTURE9 CGraphicShadowTexture::GetD3DTexture() const
{
	return m_lpd3dShadowTexture;
}

void CGraphicShadowTexture::Begin()
{

}

void CGraphicShadowTexture::End()
{

}

void CGraphicShadowTexture::Initialize()
{
	CGraphicTexture::Initialize();

	m_lpd3dShadowSurface = NULL;
	m_lpd3dDepthSurface = NULL;	
	m_lpd3dOldBackBufferSurface = NULL;
	m_lpd3dOldDepthBufferSurface = NULL;
	m_lpd3dShadowTexture = NULL;
}

CGraphicShadowTexture::CGraphicShadowTexture()
{
	Initialize();
}

CGraphicShadowTexture::~CGraphicShadowTexture()
{
	Destroy();
}
