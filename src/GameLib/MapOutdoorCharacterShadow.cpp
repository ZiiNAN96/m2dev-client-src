#include "StdAfx.h"
#include "EterLib/DrawState.h"
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
}


bool CMapOutdoor::BeginRenderCharacterShadowToTexture()
{
    return false;
}

void CMapOutdoor::EndRenderCharacterShadowToTexture()
{

}
