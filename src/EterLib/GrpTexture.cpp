#include "StdAfx.h"
#include "EterBase/Stl.h"
#include "GrpTexture.h"
#include "DrawState.h"

void CGraphicTexture::DestroyDeviceObjects()
{
    m_source.reset();
}

void CGraphicTexture::Destroy()
{
	DestroyDeviceObjects();

	Initialize();
}

void CGraphicTexture::Initialize()
{
	m_source.reset();

	m_width = 0;
	m_height = 0;
	m_bEmpty = true;
}

bool CGraphicTexture::IsEmpty() const
{
	return m_bEmpty;
}

void CGraphicTexture::SetTextureStage(int stage) const
{
	DRAWSTATE.SetTexture(stage, GetTextureBinding());
}



int CGraphicTexture::GetWidth() const
{
	return m_width;
}

int CGraphicTexture::GetHeight() const
{
	return m_height;
}

CGraphicTexture::CGraphicTexture()
{
	Initialize();
}

CGraphicTexture::~CGraphicTexture()	
{
}
