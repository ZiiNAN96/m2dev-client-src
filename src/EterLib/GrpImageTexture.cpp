#include "StdAfx.h"

#include "PackLib/PackManager.h"
#include "GrpImageTexture.h"

#include "DecodedImageData.h"
#include "TextureSource.h"


bool CGraphicImageTexture::Lock(int* pRetPitch, void** ppRetPixels, int level)
{
    return m_source && m_source->Lock(size_t(level), pRetPitch, ppRetPixels);
}

void CGraphicImageTexture::Unlock(int level)
{
    if (m_source) m_source->Unlock(size_t(level));
}

void CGraphicImageTexture::Initialize()
{
	CGraphicTexture::Initialize();

	m_stFileName = "";

	m_format=Renderer::TerrainTextureFormat::Unknown;
}

void CGraphicImageTexture::Destroy()
{
	CGraphicTexture::Destroy();

	Initialize();
}

bool CGraphicImageTexture::CreateDeviceObjects()
{
    // ZiiNAN: Removed final D3D9 compile-time dependency.
    if (!m_source && m_stFileName.empty()) {
        const auto format=m_format==Renderer::TerrainTextureFormat::Alpha8 ? Renderer::TerrainTextureFormat::Alpha8 : Renderer::TerrainTextureFormat::BGRA8;
        m_source=Renderer::TextureResource::Dynamic(m_width,m_height,format);
    } else if (!m_source) {
        TPackFile file;
        if (!CPackManager::Instance().GetFile(m_stFileName,file)) return false;
        m_source=DecodeTextureSource(file.data(),file.size(),m_stFileName.c_str());
    }
    if (!m_source) return false;
    m_width=m_source->desc.width; m_height=m_source->desc.height; m_bEmpty=false;
    return true;
}

bool CGraphicImageTexture::Create(UINT width, UINT height, Renderer::TerrainTextureFormat format)
{
	Destroy();

	m_width = width;
	m_height = height;
	m_format = format;

	return CreateDeviceObjects();
}

void CGraphicImageTexture::CreateFromTexturePointer(const CGraphicTexture* c_pSrcTexture)
{
    m_source=c_pSrcTexture->GetSource();
    m_width=c_pSrcTexture->GetWidth(); m_height=c_pSrcTexture->GetHeight();
    m_bEmpty=!m_source;
}

bool CGraphicImageTexture::CreateFromEncodedImage(UINT bufSize, const void* c_pvBuf)
{
    m_source=DecodeTextureSource(c_pvBuf,bufSize,m_stFileName.c_str());
    if (!m_source) return false;
    m_width=m_source->desc.width; m_height=m_source->desc.height; m_bEmpty=false;
    return true;
}



bool CGraphicImageTexture::CreateFromMemoryFile(UINT bufSize, const void * c_pvBuf)
{
    m_bEmpty=true;
    return CreateFromEncodedImage(bufSize,c_pvBuf);
}

void CGraphicImageTexture::SetFileName(const char * c_szFileName)
{
	m_stFileName=c_szFileName;
}

bool CGraphicImageTexture::CreateFromDiskFile(const char * c_szFileName)
{
	Destroy();

	SetFileName(c_szFileName);

	return CreateDeviceObjects();
}

bool CGraphicImageTexture::CreateFromDecodedData(const TDecodedImageData& decodedImage)
{
    if (!decodedImage.IsValid()) return false;
    m_bEmpty=true;
    if (decodedImage.isDDS) return CreateFromEncodedImage(decodedImage.pixels.size(),decodedImage.pixels.data());
    if (decodedImage.format!=TDecodedImageData::FORMAT_RGBA8) return false;
    Renderer::TerrainTextureData data{uint32_t(decodedImage.width),uint32_t(decodedImage.height),Renderer::TerrainTextureFormat::RGBA8,
        {{decodedImage.pixels.data(),decodedImage.pixels.size(),size_t(decodedImage.width)*4}}};
    m_source=Renderer::TextureResource::Copy(data);
    if (!m_source) return false;
    m_source->asset=m_stFileName; m_width=decodedImage.width; m_height=decodedImage.height; m_bEmpty=false;
    return true;
}

CGraphicImageTexture::CGraphicImageTexture()
{
	Initialize();
}

CGraphicImageTexture::~CGraphicImageTexture()
{
	Destroy();
}
