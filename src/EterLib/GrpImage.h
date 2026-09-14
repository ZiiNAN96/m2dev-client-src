#ifndef __INC_GRPIMAGE_H__
#define __INC_GRPIMAGE_H__

#include "Ref.h"
#include "Resource.h"
#include "GrpImageTexture.h"
#include "Renderer/TerrainRenderData.h"
#include <unordered_map>

struct TDecodedImageData;
namespace AssetRuntime { struct EncodedImage; }

class CGraphicImage : public CResource
{
	public:
		typedef CRef<CGraphicImage> TRef;

	public:
		static TType Type();

	public:
		CGraphicImage(const char* c_szFileName);
		virtual ~CGraphicImage();

		virtual bool CreateDeviceObjects();
		virtual void DestroyDeviceObjects();

		int GetWidth() const;
		int GetHeight() const;

		const RECT & GetRectReference() const;

		const CGraphicTexture & GetTextureReference() const;
		CGraphicTexture * GetTexturePointer();

		bool OnLoadFromDecodedData(const TDecodedImageData& decodedImage);
		bool LoadEncodedImage(std::shared_ptr<const AssetRuntime::EncodedImage> image);
		bool MatchesEncodedImage(const AssetRuntime::EncodedImage& image) const;
		Renderer::TerrainTexturePtr GetAssetTexture(Renderer::ITextureUploader& uploader);
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
		Renderer::TerrainTexturePtr m_uiTexture;
		std::shared_ptr<const AssetRuntime::EncodedImage> m_encodedImage;
		struct UploadedAssetTexture {
			std::weak_ptr<const void> uploaderLifetime;
			Renderer::TerrainTexturePtr texture;
		};
		std::unordered_map<const Renderer::ITextureUploader*, UploadedAssetTexture> m_assetTextures;
};

#endif
