#include "StdAfx.h"
#include "EterLib/NativeResourceAudit.h"
#include "EterLib/StateManager.h"
#include "EterLib/Camera.h"

#include "MapOutdoor.h"

static int recreate = false;

void CMapOutdoor::SetShadowTextureSize(WORD size)
{
	if (m_wShadowMapSize != size)
	{
		recreate = true;
		Tracenf("ShadowTextureSize changed %d -> %d", m_wShadowMapSize, size);
	}

	m_wShadowMapSize = size;
}

void CMapOutdoor::CreateCharacterShadowTexture()
{
    // ZiiNAN: Legacy D3D9 renderer removed from production path.
    // Dynamic shadows remain unsupported; never allocate a legacy target.
    ReleaseCharacterShadowTexture();
}

void CMapOutdoor::ReleaseCharacterShadowTexture()
{
	SAFE_RELEASE(m_lpCharacterShadowMapRenderTargetSurface);
	SAFE_RELEASE(m_lpCharacterShadowMapDepthSurface);
	SAFE_RELEASE(m_lpCharacterShadowMapTexture);
}


bool CMapOutdoor::BeginRenderCharacterShadowToTexture()
{
    return false;
}

void CMapOutdoor::EndRenderCharacterShadowToTexture()
{

}
