#pragma once

#include "GrpTexture.h"

struct TDecodedImageData;

class CGraphicImageTexture : public CGraphicTexture
{
	public:
		CGraphicImageTexture();
		virtual ~CGraphicImageTexture();

		void		Destroy();

		bool		Create(UINT width, UINT height, Renderer::TerrainTextureFormat format);
		bool		CreateDeviceObjects();
		
		void		CreateFromTexturePointer(const CGraphicTexture* c_pSrcTexture);
		bool		CreateFromDiskFile(const char* c_szFileName);
		bool		CreateFromMemoryFile(UINT bufSize, const void* c_pvBuf);
		bool		CreateFromEncodedImage(UINT bufSize, const void* c_pvBuf);
		bool		CreateFromDecodedData(const TDecodedImageData& decodedImage);

		void		SetFileName(const char * c_szFileName);
		
		bool		Lock(int* pRetPitch, void** ppRetPixels, int level=0);
		void		Unlock(int level=0);

	protected:
		void		Initialize();
		
		Renderer::TerrainTextureFormat m_format;

		std::string m_stFileName;
};
