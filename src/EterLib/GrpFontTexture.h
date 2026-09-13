#pragma once

#include "GrpTexture.h"
#include "GrpImageTexture.h"
#include "Renderer/TextRenderData.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <vector>
#include <map>

class CGraphicFontTexture : public CGraphicTexture
{
	public:
		typedef wchar_t TCharacterKey;

		typedef struct SCharacterInfomation
		{
			short index;
			short width;
			short height;
			float left;
			float top;
			float right;
			float bottom;
			float advance;
			float bearingX;
		} TCharacterInfomation;

		typedef std::vector<TCharacterInfomation*> TPCharacterInfomationVector;

	public:
		CGraphicFontTexture();
		virtual ~CGraphicFontTexture();

		void Destroy();
		bool Create(const char* c_szFontName, int fontSize, bool bItalic);

		bool CreateDeviceObjects();
		void DestroyDeviceObjects();

		bool CheckTextureIndex(DWORD dwTexture);
		void SelectTexture(DWORD dwTexture);

		bool UpdateTexture();
		Renderer::TerrainTexturePtr GetTextTexture(IDirect3DBaseTexture9* nativePage);

		TCharacterInfomation* GetCharacterInfomation(wchar_t keyValue);
		TCharacterInfomation* UpdateCharacterInfomation(TCharacterKey keyValue);

		float GetKerning(wchar_t prev, wchar_t cur);

		bool IsEmpty() const;

	protected:
		void Initialize();

		bool AppendTexture();
		void UploadTextPage();

	protected:
		typedef std::vector<CGraphicImageTexture*> TGraphicImageTexturePointerVector;
		typedef std::map<TCharacterKey, TCharacterInfomation> TCharacterInfomationMap;

	protected:
		FT_Face m_ftFace;

		// CPU-side atlas buffer (replaces CGraphicDib)
		DWORD* m_pAtlasBuffer;
		int m_atlasWidth;
		int m_atlasHeight;

		TGraphicImageTexturePointerVector m_pFontTextureVector;
		std::vector<Renderer::TerrainTexturePtr> m_textTextures;

		TCharacterInfomationMap m_charInfoMap;

		int m_x;
		int m_y;
		int m_step;
		bool m_isDirty;

		LONG m_fontSize;
		bool m_bItalic;

		// FreeType metrics cached per-font
		int m_ascender;
		int m_lineHeight;
		bool m_hasKerning;
};
