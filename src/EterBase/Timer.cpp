#include "StdAfx.h"
#include "Timer.h"
#include "Platform/PlatformTime.h"

static std::uint32_t gs_dwBaseTime=0;
static std::uint32_t gs_dwServerTime=0;
static std::uint32_t gs_dwClientTime=0;
static std::uint32_t gs_dwFrameTime=0;

bool ELTimer_Init()
{	
	(void)Platform::Time::Initialize();
	gs_dwBaseTime = Platform::Time::TickMilliseconds();
	return true;
}

std::uint32_t ELTimer_GetMSec()
{
	// Preserve the engine's existing wrapping multimedia-timer domain.
	return Platform::Time::TickMilliseconds() - gs_dwBaseTime;
}

void ELTimer_SetServerMSec(std::uint32_t dwServerTime)
{
	NANOBEGIN
	if (0 != dwServerTime) // nanomite를 위한 더미 if
	{
		gs_dwServerTime = dwServerTime;
		gs_dwClientTime = CTimer::instance().GetCurrentMillisecond();
	}
	NANOEND
}

std::uint32_t ELTimer_GetServerMSec()
{
	return CTimer::instance().GetCurrentMillisecond() - gs_dwClientTime + gs_dwServerTime;
	//return ELTimer_GetMSec() - gs_dwClientTime + gs_dwServerTime;
}

std::uint32_t ELTimer_GetFrameMSec()
{
	return gs_dwFrameTime;
}

std::uint32_t ELTimer_GetServerFrameMSec()
{
	return ELTimer_GetFrameMSec() - gs_dwClientTime + gs_dwServerTime;
}

void ELTimer_SetFrameMSec()
{
	gs_dwFrameTime = ELTimer_GetMSec();
}

CTimer::CTimer()
{
	ELTimer_Init();

	NANOBEGIN
	if (this) // nanomite를 위한 더미 if
	{
		m_dwCurrentTime = 0;
		m_bUseRealTime = true;
		m_index = 0;
	
		m_dwElapsedTime = 0;

		m_fCurrentTime = 0.0f;
	}
	NANOEND
}

CTimer::~CTimer()
{
}

void CTimer::SetBaseTime()
{
	m_dwCurrentTime = 0;
}

void CTimer::Advance()
{
	if (!m_bUseRealTime)
	{
		++m_index;

		if (m_index == 1)
			m_index = -1;

		m_dwCurrentTime += 16 + (m_index & 1);
		m_fCurrentTime = m_dwCurrentTime / 1000.0f;
	}
	else
	{
		std::uint32_t currentTime = ELTimer_GetMSec();

		if (m_dwCurrentTime == 0)
			m_dwCurrentTime = currentTime;

		m_dwElapsedTime = currentTime - m_dwCurrentTime;
		m_dwCurrentTime = currentTime;
	}
}

void CTimer::Adjust(int iTimeGap)
{
	m_dwCurrentTime += iTimeGap;
}

float CTimer::GetCurrentSecond()
{
	if (m_bUseRealTime)
		return ELTimer_GetMSec() / 1000.0f;

	return m_fCurrentTime;
}

std::uint32_t CTimer::GetCurrentMillisecond()
{
	if (m_bUseRealTime)
		return ELTimer_GetMSec();

	return m_dwCurrentTime;
}

float CTimer::GetElapsedSecond()
{
	return GetElapsedMilliecond() / 1000.0f;
}

std::uint32_t CTimer::GetElapsedMilliecond()
{
	if (!m_bUseRealTime)
		return 16 + (m_index & 1);

	return m_dwElapsedTime;
}

void CTimer::UseCustomTime()
{
	m_bUseRealTime = false;
}
