#include "StdAfx.h"
#include "AssetRuntime/Granny/Native.h"
#include "Motion.h"
#include "AssetRuntime/Granny/GrannyInterop.h"

CGrannyMotion::CGrannyMotion()
{
	Initialize();
}

CGrannyMotion::~CGrannyMotion()
{
	Destroy();
}

bool CGrannyMotion::IsEmpty()
{
	return !m_asset && !m_pgrnAni;
}

void CGrannyMotion::Destroy()
{
	Initialize();
}

void CGrannyMotion::Initialize()
{
	m_asset = {};
	m_pgrnAni = NULL;
}

bool CGrannyMotion::BindAsset(AssetRuntime::AnimationHandle asset)
{
    if (!asset) return false;
    auto* native = AssetRuntime::GrannyInterop::GetAnimation(asset);
    m_asset = std::move(asset);
    m_pgrnAni = native;
    return true;
}

bool CGrannyMotion::BindGrannyAnimation(granny_animation * pgrnAni)
{
	assert(IsEmpty());
	if (!pgrnAni) return false;
    if (!m_asset) m_asset=AssetRuntime::GrannyInterop::CreateLegacyAnimationHandle(pgrnAni);

	m_pgrnAni = pgrnAni;
	return true;
}

granny_animation* CGrannyMotion::GetGrannyAnimationPointer() const
{
	return m_pgrnAni;
}

const char * CGrannyMotion::GetName() const
{
	if (const auto* asset = GetAsset()) return asset->name.c_str();
	return m_pgrnAni ? m_pgrnAni->Name : "";
}

float CGrannyMotion::GetDuration() const
{
	if (const auto* asset = GetAsset()) return asset->duration;
	return m_pgrnAni ? m_pgrnAni->Duration : 0.0f;
}

void CGrannyMotion::GetTextTrack(const char* name, int* count, float* times) const
{
    const auto* asset=GetAsset();
    if (!asset || !name || !count || !times) return;
    for (const auto& event:asset->textEvents)
        if (!_stricmp(name,event.text.c_str())) times[(*count)++]=event.time;
}