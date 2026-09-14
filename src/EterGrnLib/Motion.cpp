#include "StdAfx.h"
#include "Motion.h"

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
	return !m_asset;
}

void CGrannyMotion::Destroy()
{
	Initialize();
}

void CGrannyMotion::Initialize()
{
	m_asset = {};
}

bool CGrannyMotion::BindAsset(AssetRuntime::AnimationHandle asset)
{
    if (!asset) return false;
    m_asset = std::move(asset);
    return true;
}


const char * CGrannyMotion::GetName() const
{
	if (const auto* asset = GetAsset()) return asset->name.c_str();
	return "";
}

float CGrannyMotion::GetDuration() const
{
	if (const auto* asset = GetAsset()) return asset->duration;
	return 0.0f;
}

void CGrannyMotion::GetTextTrack(const char* name, int* count, float* times) const
{
    const auto* asset=GetAsset();
    if (!asset || !name || !count || !times) return;
    for (const auto& event:asset->textEvents)
        if (!_stricmp(name,event.text.c_str())) times[(*count)++]=event.time;
}
