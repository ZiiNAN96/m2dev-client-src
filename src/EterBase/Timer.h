#pragma once

#include "Singleton.h"

#include <cstdint>

class CTimer : public CSingleton<CTimer>
{
	public:
		CTimer();
		virtual ~CTimer();

		void	Advance();
		void	Adjust(int iTimeGap);
		void	SetBaseTime();

		float	GetCurrentSecond();
		std::uint32_t GetCurrentMillisecond();

		float	GetElapsedSecond();
		std::uint32_t GetElapsedMilliecond();

		void	UseCustomTime();

	protected:
		bool	m_bUseRealTime;
		std::uint32_t m_dwBaseTime;
		std::uint32_t m_dwCurrentTime;
		float	m_fCurrentTime;
		std::uint32_t m_dwElapsedTime;
		int		m_index;
};

bool ELTimer_Init();

std::uint32_t ELTimer_GetMSec();

void ELTimer_SetServerMSec(std::uint32_t dwServerTime);
std::uint32_t ELTimer_GetServerMSec();
std::uint32_t ELTimer_GetServerFrameMSec();

void ELTimer_SetFrameMSec();
std::uint32_t ELTimer_GetFrameMSec();
