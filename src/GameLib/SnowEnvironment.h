#pragma once

#include "EterLib/GrpScreen.h"
#include "Renderer/EffectRenderData.h" // ZiiNAN: Only existing snow geometry, no blur renderer.

class CSnowParticle;

class CSnowEnvironment : public CScreen
{
	public:
		CSnowEnvironment();
		virtual ~CSnowEnvironment();

		bool Create();
		void Destroy();

		void Enable();
		void Disable();

		void Update(const Math::Vector3 & c_rv3Pos);
		void Deform();
		void Render();

	protected:		
		void __Initialize();

	protected:




		Math::Vector3 m_v3Center;

		WORD m_wBlurTextureSize;
		CGraphicImageInstance * m_pImageInstance;
		std::vector<CSnowParticle*> m_kVct_pkParticleSnow;

		DWORD m_dwParticleMaxNum;

		BOOL m_bSnowEnable;
        Renderer::EffectResources m_effectResources;
};
