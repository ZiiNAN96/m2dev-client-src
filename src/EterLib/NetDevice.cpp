#include "StdAfx.h"
#include "NetDevice.h"
#include "Platform/PlatformNetworking.h"

CNetworkDevice::CNetworkDevice()
{
	Initialize();
}

CNetworkDevice::~CNetworkDevice()
{
	Destroy();	
}

void CNetworkDevice::Initialize()
{
	m_isStarted=false;
}

void CNetworkDevice::Destroy()
{
	if (m_isStarted)
	{
		Platform::Networking::Shutdown();
		m_isStarted=false;
	}
}

bool CNetworkDevice::Create()
{
	Destroy();

	Initialize();

	if (!Platform::Networking::Startup())
		return false;

	m_isStarted=true;
	
	return true;
}
