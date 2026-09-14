#include "StdAfx.h"
#include "NetAddress.h"
#include "Platform/PlatformNetworking.h"

#include <cstdio>

bool CNetworkAddress::GetHostName(char* szName, int size)
{
	return size > 0 && Platform::Networking::GetLocalHostName(szName, static_cast<std::size_t>(size));
}

CNetworkAddress::CNetworkAddress()
{
	Clear();
}

CNetworkAddress::~CNetworkAddress()
{
}

void CNetworkAddress::Clear()
{
	m_address = 0;
	m_port = 0;
}

bool CNetworkAddress::IsIP(const char* c_szAddr)
{
	if (c_szAddr[0]<'0' || c_szAddr[0]>'9')
		return false;
	return true;
}

bool CNetworkAddress::Set(const char* c_szAddr, int port)
{
	if (IsIP(c_szAddr))
	{
		SetIP(c_szAddr);
	}
	else
	{
		if (!SetDNS(c_szAddr))
			return false;
	}

	SetPort(port);
	return true;
}

void CNetworkAddress::SetLocalIP()
{
	SetIP(std::uint32_t{0});
}

void CNetworkAddress::SetIP(std::uint32_t ip)
{
	m_address = ip;
}

void CNetworkAddress::SetIP(const char* c_szIP)
{
	std::uint32_t address = 0;
	if (c_szIP && Platform::Networking::ResolveIPv4(c_szIP, address))
		m_address = address;
}

bool CNetworkAddress::SetDNS(const char* c_szDNS)
{
	std::uint32_t address = 0;
	if (!c_szDNS || !Platform::Networking::ResolveIPv4(c_szDNS, address))
		return false;
	m_address = address;
	return true;
}

void CNetworkAddress::SetPort(int port)
{
	m_port = static_cast<std::uint16_t>(port);
}

std::uint32_t CNetworkAddress::GetIP()
{
	return m_address;
}

void CNetworkAddress::GetIP(char* szIP, int len)
{
	if (!szIP || len <= 0)
		return;
	_snprintf(szIP, len, "%u.%u.%u.%u",
		(m_address >> 24) & 0xff,
		(m_address >> 16) & 0xff,
		(m_address >> 8) & 0xff,
		m_address & 0xff);
}
			
int CNetworkAddress::GetPort()
{
	return m_port;
}
