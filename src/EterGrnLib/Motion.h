#pragma once
#include "AssetRuntime/Granny/NativeTypes.h"
#include "AssetRuntime/AssetRuntime.h"

class CGrannyMotion
{
	public:
		CGrannyMotion();
		virtual ~CGrannyMotion();

		bool				IsEmpty();

		void				Destroy();
		bool				BindGrannyAnimation(granny_animation* pgrnAni);
        bool BindAsset(AssetRuntime::AnimationHandle asset);
        const AssetRuntime::AnimationAsset* GetAsset() const { return m_asset.Get(); }
        const AssetRuntime::AnimationHandle& GetAssetHandle() const { return m_asset; }

		granny_animation *	GetGrannyAnimationPointer() const;

		const char *		GetName() const;
		float				GetDuration() const;
		void				GetTextTrack(const char * c_szTextTrackName, int * pCount, float * pArray) const;

	protected:
		void				Initialize();

	protected:
		AssetRuntime::AnimationHandle m_asset;
		granny_animation *	m_pgrnAni;
};
