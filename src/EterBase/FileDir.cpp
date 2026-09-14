#include "StdAfx.h"
#include "FileDir.h"
#include "Platform/PlatformFilesystem.h"
#include <string>

CDir::CDir()
{
	Initialize();
}

CDir::~CDir()
{
	Destroy();
}

void CDir::Destroy()
{
	Initialize();
}

bool CDir::Create(const char* c_szFilter, const char* c_szPath, bool bCheckedExtension)
{
	Destroy();

	std::string stPath = c_szPath ? c_szPath : "";

	if (!stPath.empty())
	{
		char end = stPath.back();
		if (end != '\\')
			stPath += '\\';
	}

	std::vector<Platform::Filesystem::DirectoryEntry> entries;
	if (!Platform::Filesystem::ListDirectory(stPath, entries))
		return true;

	for (const auto& entry : entries)
	{
		const std::string& fileNameUtf8 = entry.name;

		if (!fileNameUtf8.empty() && fileNameUtf8[0] == '.')
			continue;

		m_isFolder = entry.isDirectory;
		if (IsFolder())
		{
			if (!OnFolder(c_szFilter, stPath.c_str(), fileNameUtf8.c_str()))
				return false;
		}
		else
		{
			const char* c_szExtension = strchr(fileNameUtf8.c_str(), '.');
			if (!c_szExtension)
				continue;

			// NOTE : 임시 변수 - [levites]
			//        최종적으로는 무조건 TRUE 형태로 만든다.
			//        그전에 전 프로젝트의 CDir을 사용하는 곳에서 Extension을 "wav", "gr2" 이런식으로 넣게끔 한다. - [levites]
			if (bCheckedExtension)
			{
				std::string strFilter = c_szFilter ? c_szFilter : "";
				int iPos = (int)strFilter.find_first_of(';', 0);

				if (iPos > 0)
				{
					std::string first = strFilter.substr(0, iPos);
					std::string second = strFilter.substr(iPos + 1);

					if (0 != first.compare(c_szExtension + 1) &&
						0 != second.compare(c_szExtension + 1))
						continue;
				}
				else
				{
					if (0 != _stricmp(c_szExtension + 1, c_szFilter))
						continue;
				}
			}

			if (!OnFile(stPath.c_str(), fileNameUtf8.c_str()))
				return false;
		}
	}

	return true;
}

bool CDir::IsFolder()
{
	return m_isFolder;
}

void CDir::Initialize()
{
	m_isFolder = false;
}
