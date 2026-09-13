#pragma once

#include "GrpBase.h"
#include "TextureBinding.h"

class CGraphicTexture : public CGraphicBase
{
	public:
		virtual bool IsEmpty() const;

		int GetWidth() const;
		int GetHeight() const;

		void SetTextureStage(int stage) const;

		TextureBinding GetTextureBinding() const { return TextureBinding(m_source); }
		const std::shared_ptr<Renderer::TextureResource>& GetSource() const { return m_source; }

		void DestroyDeviceObjects();
		
	protected:
		CGraphicTexture();
		virtual	~CGraphicTexture();

		void Destroy();
		void Initialize();

	protected:
		bool m_bEmpty;

		int m_width;
		int m_height;


		std::shared_ptr<Renderer::TextureResource> m_source;
};
