#include "StdAfx.h"
#include "EterBase/Stl.h"
#include "EterBase/FileLoader.h"
#include "GrpSubImage.h"
#include "ResourceManager.h"

char CGraphicSubImage::m_SearchPath[256] = "D:/Ymir Work/UI/";

CGraphicSubImage::TType CGraphicSubImage::Type()
{
	static TType s_type = StringToType("CGraphicSubImage");
	return s_type;
}

CGraphicSubImage::CGraphicSubImage(const char* c_szFileName) : CGraphicImage(c_szFileName)
{
}

CGraphicSubImage::~CGraphicSubImage()
{
	m_roImage = NULL;
}

bool CGraphicSubImage::CreateDeviceObjects()
{
	m_imageTexture.CreateFromTexturePointer(m_roImage->GetTexturePointer());
	return true;
}

void CGraphicSubImage::SetImagePointer(CGraphicImage* pImage)
{
	m_roImage = pImage;
	CreateDeviceObjects();
}

bool CGraphicSubImage::SetImageFileName(const char* c_szFileName)
{
	CResource* pResource = CResourceManager::Instance().GetResourcePointer(c_szFileName);

	if (!pResource->IsType(CGraphicImage::Type()))
		return false;

	SetImagePointer(static_cast<CGraphicImage*>(pResource));
	return true;
}

void CGraphicSubImage::SetRectPosition(int left, int top, int right, int bottom)
{
	m_rect.left = left;
	m_rect.top = top;
	m_rect.right = right;
	m_rect.bottom = bottom;
}

void CGraphicSubImage::SetRectReference(const RECT& c_rRect)
{
	m_rect = c_rRect;
}

void CGraphicSubImage::SetSearchPath(const char * c_szFileName)
{
	strncpy(m_SearchPath, c_szFileName, sizeof(m_SearchPath)-1);
}

bool CGraphicSubImage::OnLoad(int iSize, const void* c_pvBuf)
{
	if (!c_pvBuf || iSize <= 0)
		return false;

	CTokenVector stTokenVector;
	std::map<std::string, std::string> stTokenMap;

	CMemoryTextFileLoader textFileLoader;

	textFileLoader.Bind(static_cast<size_t>(iSize), c_pvBuf);

	for (DWORD i = 0; i < textFileLoader.GetLineCount(); ++i)
	{
		if (!textFileLoader.SplitLine(i, &stTokenVector))
			continue;

		if (stTokenVector.size() != 2)
			return false;

		stl_lowers(stTokenVector[0]);
		stl_lowers(stTokenVector[1]);

		stTokenMap[stTokenVector[0]] = stTokenVector[1];
	}

	const std::string& c_rstTitle = stTokenMap["title"];
	const std::string& c_rstVersion = stTokenMap["version"];
	const std::string& c_rstImage = stTokenMap["image"];
	const std::string& c_rstLeft = stTokenMap["left"];
	const std::string& c_rstTop = stTokenMap["top"];
	const std::string& c_rstRight = stTokenMap["right"];
	const std::string& c_rstBottom = stTokenMap["bottom"];

	if (c_rstTitle != "subimage")
		return false;
	
	// ZiiNAN: 64-bit safety cleanup
	std::string imageFileName;
	if ("2.0"==c_rstVersion)
	{
		const std::string& c_rstSubFileName=GetFileNameString();
		const std::string::size_type separatorPos = c_rstSubFileName.find_last_of('\\');
		if (separatorPos != std::string::npos)
			imageFileName.assign(c_rstSubFileName, 0, separatorPos + 1);
		imageFileName += c_rstImage;
	}
	else
	{
		imageFileName = m_SearchPath;
		imageFileName += c_rstImage;
	}

	SetImageFileName(imageFileName.c_str());

	SetRectPosition(atoi(c_rstLeft.c_str()),
					atoi(c_rstTop.c_str()),
					atoi(c_rstRight.c_str()),
					atoi(c_rstBottom.c_str()));

	return true;
}

void CGraphicSubImage::OnClear()
{
	m_roImage = NULL;
	memset(&m_rect, 0, sizeof(m_rect));
}

bool CGraphicSubImage::OnIsEmpty() const
{
	if (!m_roImage.IsNull())		
		if (!m_roImage->IsEmpty())
			return false;

	return true;
}

bool CGraphicSubImage::OnIsType(TType type)
{
	if (CGraphicSubImage::Type() == type)
		return true;

	return CGraphicImage::OnIsType(type);
}

// ZiiNAN: Subimages share the parent's original atlas and its resource lifetime.
Renderer::TerrainTexturePtr CGraphicSubImage::GetUITexture(Renderer::ITextureUploader& uploader)
{
	return m_roImage.IsNull() ? Renderer::TerrainTexturePtr{} : m_roImage->GetUITexture(uploader);
}
