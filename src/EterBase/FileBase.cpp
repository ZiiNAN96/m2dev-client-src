#include "StdAfx.h"
#include "FileBase.h"

CFileBase::CFileBase() : m_dwSize(0)
{
}

CFileBase::~CFileBase()
{
	Destroy();
}

char * CFileBase::GetFileName()
{
	return m_filename;
}

void CFileBase::Destroy()
{
	Close();
	m_dwSize = 0;
}

void CFileBase::Close()
{
	m_file.Close();
}

bool CFileBase::Create(const char* filename, EFileMode mode)
{
	Destroy();
	if (!filename)
		return false;

	// Keep filename internally as UTF-8 (engine side)
	strncpy(m_filename, filename, MAX_PATH);
	m_filename[MAX_PATH - 1] = '\0';

	const auto platformMode = mode == FILEMODE_WRITE
		? Platform::Filesystem::OpenMode::Write
		: Platform::Filesystem::OpenMode::Read;
	if (!m_file.Open(filename, platformMode))
		return false;

	m_dwSize = static_cast<std::uint32_t>(std::min<std::uint64_t>(m_file.Size(), UINT32_MAX));
	m_mode = mode;
	return true;
}

std::uint32_t CFileBase::Size()
{
	return (m_dwSize);
}

void CFileBase::SeekCur(std::uint32_t size)
{
	m_file.Seek(size, Platform::Filesystem::SeekOrigin::Current);
}

void CFileBase::Seek(std::uint32_t offset)
{
	if (offset > m_dwSize)
		offset = m_dwSize;

	m_file.Seek(offset, Platform::Filesystem::SeekOrigin::Begin);
}

std::uint32_t CFileBase::GetPosition()
{
	return static_cast<std::uint32_t>(std::min<std::uint64_t>(m_file.Position(), UINT32_MAX));
}

bool CFileBase::Write(const void* src, int bytes)
{
	if (bytes < 0 || !m_file.Write(src, static_cast<std::size_t>(bytes)))
		return false;

	m_dwSize = static_cast<std::uint32_t>(std::min<std::uint64_t>(m_file.Size(), UINT32_MAX));
	return true;
}

bool CFileBase::Read(void* dest, int bytes)
{
	return bytes >= 0 && m_file.Read(dest, static_cast<std::size_t>(bytes));
}

bool CFileBase::IsNull()
{
	return !m_file.IsOpen();
}
