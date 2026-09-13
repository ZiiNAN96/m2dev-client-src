#include "StdAfx.h"
#include "GrpImage.h"
#include "DecodedImageData.h"
#include "StaticObjectTextureLoader.h"

CGraphicImage::CGraphicImage(const char * c_szFileName, DWORD dwFilter) : 
CResource(c_szFileName),
m_dwFilter(dwFilter)
{
	m_rect.bottom = m_rect.right = m_rect.top = m_rect.left = 0;
}

CGraphicImage::~CGraphicImage()
{
}

bool CGraphicImage::CreateDeviceObjects()
{
	m_uiTexture.reset();
	return m_imageTexture.CreateDeviceObjects();
}

void CGraphicImage::DestroyDeviceObjects()
{
	m_uiTexture.reset();
	m_imageTexture.DestroyDeviceObjects();
}

CGraphicImage::TType CGraphicImage::Type()
{
	static TType s_type = StringToType("CGraphicImage");
	return s_type;
}

bool CGraphicImage::OnIsType(TType type)
{
	if (CGraphicImage::Type() == type)
		return true;

	return CResource::OnIsType(type);
}

int CGraphicImage::GetWidth() const
{
	return m_rect.right - m_rect.left;
}

int CGraphicImage::GetHeight() const
{
	return m_rect.bottom - m_rect.top;
}

const CGraphicTexture& CGraphicImage::GetTextureReference() const
{
	return m_imageTexture;
}

CGraphicTexture* CGraphicImage::GetTexturePointer()
{
	return &m_imageTexture;
}

const RECT& CGraphicImage::GetRectReference() const
{
	return m_rect;
}

bool CGraphicImage::OnLoad(int iSize, const void * c_pvBuf)
{
	m_uiTexture.reset();
	if (!c_pvBuf)
		return false;

	m_imageTexture.SetFileName(CResource::GetFileName());

	// 특정 컴퓨터에서 Unknown으로 '안'하면 튕기는 현상이 있음-_-; -비엽
	if (!m_imageTexture.CreateFromMemoryFile(iSize, c_pvBuf, D3DFMT_UNKNOWN, m_dwFilter))
		return false;

	m_rect.left = 0;
	m_rect.top = 0;
	m_rect.right = m_imageTexture.GetWidth();
	m_rect.bottom = m_imageTexture.GetHeight();
	return true;
}

bool CGraphicImage::OnLoadFromDecodedData(const TDecodedImageData& decodedImage)
{
	m_uiTexture.reset();
	if (!decodedImage.IsValid())
		return false;

	m_imageTexture.SetFileName(CResource::GetFileName());

	if (!m_imageTexture.CreateFromDecodedData(decodedImage, D3DFMT_UNKNOWN, m_dwFilter))
		return false;

	m_rect.left = 0;
	m_rect.top = 0;
	m_rect.right = m_imageTexture.GetWidth();
	m_rect.bottom = m_imageTexture.GetHeight();
	return true;
}

void CGraphicImage::OnClear()
{
	m_uiTexture.reset();
//	Tracef("Image Destroy : %s\n", m_pszFileName);
	m_imageTexture.Destroy();
	memset(&m_rect, 0, sizeof(m_rect));
}

bool CGraphicImage::OnIsEmpty() const
{
	return m_imageTexture.IsEmpty();
}

Renderer::TerrainTexturePtr CGraphicImage::GetUITexture(Renderer::ITextureUploader& uploader)
{
	if(!m_uiTexture && !IsEmpty()) m_uiTexture=LoadStaticObjectTextureFile(GetFileName(),uploader);
	return m_uiTexture;
}
