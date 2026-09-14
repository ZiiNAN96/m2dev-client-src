#include "StdAfx.h"
#include "GrpImage.h"
#include "DecodedImageData.h"
#include "StaticObjectTextureLoader.h"
#include "AssetRuntime/AssetRuntime.h"
#include <limits>

CGraphicImage::CGraphicImage(const char * c_szFileName) :
CResource(c_szFileName)
{
	m_rect.bottom = m_rect.right = m_rect.top = m_rect.left = 0;
}

CGraphicImage::~CGraphicImage()
{
}

bool CGraphicImage::CreateDeviceObjects()
{
	m_uiTexture.reset();
	m_assetTextures.clear();
	if (m_encodedImage) {
		m_imageTexture.SetFileName(GetFileName());
		return m_imageTexture.CreateFromMemoryFile(static_cast<UINT>(m_encodedImage->bytes.size()), m_encodedImage->bytes.data());
	}
	return m_imageTexture.CreateDeviceObjects();
}

void CGraphicImage::DestroyDeviceObjects()
{
	m_uiTexture.reset();
	m_assetTextures.clear();
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
	m_assetTextures.clear();
	m_encodedImage.reset();
	if (!c_pvBuf)
		return false;

	m_imageTexture.SetFileName(CResource::GetFileName());

	if (!m_imageTexture.CreateFromMemoryFile(iSize,c_pvBuf))
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
	m_assetTextures.clear();
	m_encodedImage.reset();
	if (!decodedImage.IsValid())
		return false;

	m_imageTexture.SetFileName(CResource::GetFileName());
	if (!m_imageTexture.CreateFromDecodedData(decodedImage))
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
	m_assetTextures.clear();
	m_encodedImage.reset();
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
	if (m_encodedImage) return GetAssetTexture(uploader);
	if(!m_uiTexture && !IsEmpty()) m_uiTexture=LoadStaticObjectTextureFile(GetFileName(),uploader);
	return m_uiTexture;
}

bool CGraphicImage::LoadEncodedImage(std::shared_ptr<const AssetRuntime::EncodedImage> image)
{
	if (!image || image->bytes.empty() || image->bytes.size() > size_t((std::numeric_limits<int>::max)())) return false;
	if (!OnLoad(static_cast<int>(image->bytes.size()), image->bytes.data())) { me_state = STATE_ERROR; return false; }
	m_encodedImage = std::move(image);
	me_state = STATE_EXIST;
	return true;
}

bool CGraphicImage::MatchesEncodedImage(const AssetRuntime::EncodedImage& image) const
{
	return m_encodedImage && m_encodedImage->id == image.id && m_encodedImage->mimeType == image.mimeType &&
		(m_encodedImage.get() == &image || m_encodedImage->bytes == image.bytes);
}

Renderer::TerrainTexturePtr CGraphicImage::GetAssetTexture(Renderer::ITextureUploader& uploader)
{
	const auto& source = m_imageTexture.GetSource();
	if (!source) return {};
	std::erase_if(m_assetTextures, [](const auto& entry) { return entry.second.uploaderLifetime.expired(); });
	auto& entry = m_assetTextures[&uploader];
	if (!entry.texture) {
		entry.uploaderLifetime = uploader.TextureCacheLifetime();
		entry.texture = uploader.UploadTexture(source->View());
	}
	return entry.texture;
}
