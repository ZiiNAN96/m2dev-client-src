#pragma once

#include <cstdint>

class CNetworkAddress
{
	public:
		static bool GetHostName(char* szName, int size);

	public:
		CNetworkAddress();
		~CNetworkAddress();

		void Clear();

		bool Set(const char* c_szAddr, int port);

		void SetLocalIP();
		void SetIP(std::uint32_t ip);
		void SetIP(const char* c_szIP);
		bool SetDNS(const char* c_szDNS);

		void SetPort(int port);
		
		int GetPort();
		void GetIP(char* szIP, int len);

		std::uint32_t GetIP();

	private:
		bool IsIP(const char* c_szAddr);

	private:
		std::uint32_t m_address;
		std::uint16_t m_port;
};
