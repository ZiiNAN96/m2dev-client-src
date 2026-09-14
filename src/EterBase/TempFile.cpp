#include "StdAfx.h"
#include "TempFile.h"
#include "Utils.h"
#include "Debug.h"
#include "Platform/PlatformFilesystem.h"

CTempFile::~CTempFile()
{
	Destroy();

	if (m_szFileName[0])
		(void)Platform::Filesystem::RemoveFile(m_szFileName);
}

CTempFile::CTempFile(const char * c_pszPrefix)
{
	strncpy(m_szFileName, CreateTempFileName(c_pszPrefix), 260);
	m_szFileName[260] = '\0';

	if (!Create(m_szFileName, CFileBase::FILEMODE_WRITE))
	{
		TraceError("CTempFile::CTempFile cannot create temporary file. (filename: %s)", m_szFileName);
		return;
	}
}
