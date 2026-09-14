#pragma once

#include "Model.h"
#include "Motion.h"
#include "AssetRuntime/AssetRuntime.h"

class CGraphicThing : public CResource
{
	public:
		typedef CRef<CGraphicThing> TRef;

	public:
		static CGraphicThing::TType Type();

	public:
		CGraphicThing(const char * c_szFileName);
		virtual ~CGraphicThing();

		virtual bool			CreateDeviceObjects();
		virtual void			DestroyDeviceObjects();

		bool					CheckModelIndex(int iModel) const;
		CGrannyModel *			GetModelPointer(int iModel);
		int						GetModelCount() const;

		bool					CheckMotionIndex(int iMotion) const;
		CGrannyMotion *			GetMotionPointer(int iMotion);
		int						GetMotionCount() const;

        // ZiiNAN: Diligent actor attachment rendering
        void MarkActorAttachment() { m_actorAttachment=true; }
        const AssetRuntime::AssetHandle& GetAsset() const { return m_asset; }

	protected:
		void					Initialize();

		bool					LoadModels();
		bool					LoadMotions();

	protected:
		bool					OnLoad(int iSize, const void* c_pvBuf);
		void					OnClear();
		bool					OnIsEmpty() const;
		bool					OnIsType(TType type);

	protected:
        AssetRuntime::AssetHandle m_asset;
        bool m_actorAttachment=false;

		CGrannyModel *			m_models;
		CGrannyMotion *			m_motions;
};
