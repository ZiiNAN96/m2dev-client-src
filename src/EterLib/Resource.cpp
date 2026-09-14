#include "StdAfx.h"
#include "PackLib/PackManager.h"
#include "EterBase/Stl.h"
#include "EterBase/CRC32.h"
#include "EterBase/Timer.h"

#include "Resource.h"
#include "ResourceManager.h"
#include "AssetRuntime/AnimationStallAudit.h"

#include <limits>

bool CResource::ms_bDeleteImmediately = false;

CResource::CResource(const char* c_szFileName) : me_state(STATE_EMPTY)
{
	SetFileName(c_szFileName);
}

CResource::~CResource()
{
}

void CResource::SetDeleteImmediately(bool isSet)
{
	ms_bDeleteImmediately = isSet;
}

void CResource::OnConstruct()
{
	Load();
}

void CResource::OnSelfDestruct()
{	
	if (ms_bDeleteImmediately)
		Clear();
	else
		CResourceManager::Instance().ReserveDeletingResource(this);
}

void CResource::Load()
{
	if (me_state != STATE_EMPTY)
		return;

	const char * c_szFileName = GetFileName();

	DWORD		dwStart = ELTimer_GetMSec();
	TPackFile	file;

	//Tracenf("Load %s", c_szFileName);

    AssetRuntime::AnimationStallAudit::WorkScope readAudit(AssetRuntime::AnimationStallAudit::Work::GR2Read,
        std::string_view(c_szFileName).ends_with(".gr2"));
    const bool found=CPackManager::Instance().GetFile(c_szFileName, file);
    readAudit.Stop();
	if (found)
	{
		m_dwLoadCostMiliiSecond = ELTimer_GetMSec() - dwStart;
		//Tracef("CResource::Load %s (%d bytes) in %d ms\n", c_szFileName, file.Size(), m_dwLoadCostMiliiSecond);

		// ZiiNAN: 64-bit safety cleanup
		if (file.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			TraceError("CResource::Load: file too large for legacy resource API: %s (%zu bytes)", c_szFileName, file.size());
			me_state = STATE_ERROR;
			return;
		}

		if (OnLoad(static_cast<int>(file.size()), file.data()))
		{
			me_state = STATE_EXIST;
		}
		else
		{
			Tracef("CResource::Load Error %s\n", c_szFileName);
			me_state = STATE_ERROR;
			return;
		}
	}
	else
	{
		if (OnLoad(0, NULL))
			me_state = STATE_EXIST;
		else
		{
			Tracef("CResource::Load file not exist %s\n", c_szFileName);
			me_state = STATE_ERROR;
		}
	}
}

void CResource::Reload()
{
	Clear();
	Tracef("CResource::Reload %s\n", GetFileName());

	TPackFile	file;
    AssetRuntime::AnimationStallAudit::WorkScope readAudit(AssetRuntime::AnimationStallAudit::Work::GR2Read,
        std::string_view(GetFileName()).ends_with(".gr2"));
    const bool found=CPackManager::Instance().GetFile(GetFileName(), file);
    readAudit.Stop();
	if (found)
	{
		if (file.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
		{
			TraceError("CResource::Reload: file too large for legacy resource API: %s (%zu bytes)", GetFileName(), file.size());
			me_state = STATE_ERROR;
			return;
		}

		if (OnLoad(static_cast<int>(file.size()), file.data()))
		{
			me_state = STATE_EXIST;
		}
		else
		{
			me_state = STATE_ERROR;
			return;
		}
	}
	else
	{
		if (OnLoad(0, NULL))
			me_state = STATE_EXIST;
		else
		{
			me_state = STATE_ERROR;
		}
	}
}

CResource::TType CResource::StringToType(const char* c_szType)
{
	return GetCRC32(c_szType, strlen(c_szType));
}

int CResource::ConvertPathName(const char * c_szPathName, char * pszRetPathName, int retLen)
{
	const char * pc;
	int len = 0;

	for (pc = c_szPathName; *pc && len < retLen; ++pc, ++len)
	{
		if (*pc == '/')
			*(pszRetPathName++) = '\\';
		else
			*(pszRetPathName++) = (char) ascii_tolower(*pc);
	}

	*pszRetPathName = '\0';
	return len;
}

void CResource::SetFileName(const char* c_szFileName)
{
	// 2004. 2. 1. myevan. 쓰레드가 사용되는 상황에서 static 변수는 사용하지 않는것이 좋다.
	// 2004. 2. 1. myevan. 파일 이름 처리를 std::string 사용
	m_stFileName=c_szFileName;
}

void CResource::Clear()
{
	OnClear();
	me_state = STATE_EMPTY;
}

bool CResource::IsType(TType type)
{
	return OnIsType(type);
}

CResource::TType CResource::Type()
{
	static TType s_type = StringToType("CResource");
	return s_type;
}

bool CResource::OnIsType(TType type)
{
	if (CResource::Type() == type)
		return true;
	
	return false;
}

bool CResource::IsData() const
{
	return me_state != STATE_EMPTY;
}

bool CResource::IsEmpty() const
{
	return OnIsEmpty();
}

bool CResource::CreateDeviceObjects()
{
	return true;
}

void CResource::DestroyDeviceObjects()
{
}
