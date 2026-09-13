#ifndef __INC_GRPIMAGE_H__
#define __INC_GRPIMAGE_H__

#include "Ref.h"
#include "Resource.h"
#include "GrpImageTexture.h"
#include "Renderer/TerrainRenderData.h"

struct TDecodedImageData;

class CGraphicImage : public CResource
{
	public:
		typedef CRef<CGraphicImage> TRef;

	public:
		static TType Type();

	public:
		CGraphicImage(const char* c_szFileName, DWORD dwFilter = D3DX_FILTER_LINEAR);
		virtual ~CGraphicImage();

		virtual bool CreateDeviceObjects();
		virtual void DestroyDeviceObjects();

		int GetWidth() const;
		int GetHeight() const;

		const RECT & GetRectReference() const;

		const CGraphicTexture & GetTextureReference() const;
		CGraphicTexture * GetTexturePointer();

		bool OnLoadFromDecodedData(const TDecodedImageData& decodedImage);
		// ZiiNAN: Lazy UI texture ownership follows the original image, not a global atlas cache.
		virtual Renderer::TerrainTexturePtr GetUITexture(Renderer::ITextureUploader& uploader);

	protected:
		bool OnLoad(int iSize, const void * c_pvBuf);
		
		void OnClear();	
		bool OnIsEmpty() const;
		bool OnIsType(TType type);

	protected:
		CGraphicImageTexture	m_imageTexture;
		RECT					m_rect;
		DWORD					m_dwFilter;
		Renderer::TerrainTexturePtr m_uiTexture;
};

#endif
