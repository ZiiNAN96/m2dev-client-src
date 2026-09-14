#include "StdAfx.h"
#include "FileLoader.h"
#include <assert.h>
#include <utf8.h>

CMemoryTextFileLoader::CMemoryTextFileLoader()
{
}

CMemoryTextFileLoader::~CMemoryTextFileLoader()
{
}

bool CMemoryTextFileLoader::SplitLineByTab(DWORD dwLine, CTokenVector* pstTokenVector)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	const std::string & c_rstLine = GetLineString(dwLine);
	const std::string::size_type c_iLineLength = c_rstLine.length();

	if (0 == c_iLineLength)
		return false;

	std::string::size_type basePos = 0;

	do
	{
		const std::string::size_type beginPos = c_rstLine.find_first_of("\t", basePos);

		pstTokenVector->push_back(c_rstLine.substr(basePos, beginPos-basePos));

		if (beginPos == std::string::npos)
			break;
		basePos = beginPos+1;
	} while (basePos < c_iLineLength);

	return true;
}

int CMemoryTextFileLoader::SplitLine2(DWORD dwLine, CTokenVector* pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = GetLineString(dwLine);

	std::string::size_type basePos = 0;

	do
	{
		std::string::size_type beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);

		if (beginPos == std::string::npos)
			return -1;

		std::string::size_type endPos;

		if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of("\"", beginPos);

			if (endPos == std::string::npos)
				return -2;

			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->push_back(c_rstLine.substr(beginPos, endPos - beginPos));

		// 추가 코드. 맨뒤에 탭이 있는 경우를 체크한다. - [levites]
		if (c_rstLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)
			break;
	} while (basePos < c_rstLine.length());

	return 0;
}

bool CMemoryTextFileLoader::SplitLine(DWORD dwLine, CTokenVector* pstTokenVector, const char * c_szDelimeter)
{
	pstTokenVector->reserve(10);
	pstTokenVector->clear();

	std::string stToken;
	const std::string & c_rstLine = GetLineString(dwLine);

	std::string::size_type basePos = 0;

	do
	{
		std::string::size_type beginPos = c_rstLine.find_first_not_of(c_szDelimeter, basePos);
		if (beginPos == std::string::npos)
			return false;

		std::string::size_type endPos;

		if (c_rstLine[beginPos] == '"')
		{
			++beginPos;
			endPos = c_rstLine.find_first_of("\"", beginPos);

			if (endPos == std::string::npos)
				return false;
			
			basePos = endPos + 1;
		}
		else
		{
			endPos = c_rstLine.find_first_of(c_szDelimeter, beginPos);
			basePos = endPos;
		}

		pstTokenVector->push_back(c_rstLine.substr(beginPos, endPos - beginPos));

		// 추가 코드. 맨뒤에 탭이 있는 경우를 체크한다. - [levites]
		if (c_rstLine.find_first_not_of(c_szDelimeter, basePos) == std::string::npos)
			break;
	} while (basePos < c_rstLine.length());

	return true;
}

DWORD CMemoryTextFileLoader::GetLineCount()
{
	assert(m_stLineVector.size() <= MAXDWORD);
	return m_stLineVector.size() > MAXDWORD ? MAXDWORD : static_cast<DWORD>(m_stLineVector.size());
}

bool CMemoryTextFileLoader::CheckLineIndex(DWORD dwLine)
{
	if (dwLine >= m_stLineVector.size())
		return false;

	return true;
}

const std::string & CMemoryTextFileLoader::GetLineString(DWORD dwLine)
{
	assert(CheckLineIndex(dwLine));
	return m_stLineVector[dwLine];
}

void CMemoryTextFileLoader::Bind(size_t bufSize, const void* c_pvBuf)
{
	m_stLineVector.reserve(128);
	m_stLineVector.clear();

	const char * c_pcBuf = (const char *)c_pvBuf;
	std::string stLine;
	size_t pos = 0;

	while (pos < bufSize)
	{
		const char c = c_pcBuf[pos++];

		if ('\n' == c || '\r' == c)
		{
			if (pos < bufSize)
				if ('\n' == c_pcBuf[pos] || '\r' == c_pcBuf[pos])
					++pos;

			m_stLineVector.push_back(stLine);
			stLine = "";
		}
		else if (c < 0)
		{
			stLine.append(c_pcBuf + (pos-1), 2);
			++pos;
		}
		else
		{
			stLine += c;
		}
	}

	m_stLineVector.push_back(stLine);
}

//////////////////////////////////////////////////////////////////////////////////////////////////
int CMemoryFileLoader::GetSize()
{
	return m_size;
}

int CMemoryFileLoader::GetPosition()
{
	return m_pos;
}

bool CMemoryFileLoader::IsReadableSize(int size)
{
	return size >= 0 && m_pos >= 0 && m_pos <= m_size && size <= m_size - m_pos;
}

bool CMemoryFileLoader::Read(int size, void* pvDst)
{
	if (!IsReadableSize(size))
		return false;

	memcpy(pvDst, GetCurrentPositionPointer(), size);
	m_pos += size;
	return true;
}

const char* CMemoryFileLoader::GetCurrentPositionPointer()
{
	assert(m_pcBase != NULL);
	return (m_pcBase + m_pos);
}

CMemoryFileLoader::CMemoryFileLoader(int size, const void* c_pvMemoryFile)
{
	assert(c_pvMemoryFile != NULL);

	m_pos = 0;
	m_size = size;
	m_pcBase = (const char *) c_pvMemoryFile;
}

CMemoryFileLoader::~CMemoryFileLoader()
{
}

//////////////////////////////////////////////////////////////////////////////////////////////////
int CDiskFileLoader::GetSize()
{
	return m_size;
}

bool CDiskFileLoader::Read(int size, void* pvDst)
{
	assert(m_fp != NULL);
	if (!m_fp || size < 0 || (!pvDst && size > 0))
		return false;

	const size_t ret = fread(pvDst, static_cast<size_t>(size), 1, m_fp);

	if (ret <= 0)
		return false;

	return true;
}

bool CDiskFileLoader::Open(const char* c_szFileName)
{
	Close();

	if (!c_szFileName[0])
		return false;

	// UTF-8 → UTF-16 conversion for Unicode path support
	std::wstring wFileName = Utf8ToWide(c_szFileName);
	m_fp = _wfopen(wFileName.c_str(), L"rb");

	if (!m_fp)
		return false;

	// ZiiNAN: 64-bit safety cleanup
	if (_fseeki64(m_fp, 0, SEEK_END) != 0)
	{
		Close();
		return false;
	}
	const __int64 fileSize = _ftelli64(m_fp);
	if (fileSize < 0 || fileSize > INT_MAX || _fseeki64(m_fp, 0, SEEK_SET) != 0)
	{
		Close();
		return false;
	}
	m_size = static_cast<int>(fileSize);
	return true;
}

void CDiskFileLoader::Close()
{
	if (m_fp)
		fclose(m_fp);

	Initialize();
}

void CDiskFileLoader::Initialize()
{
	m_fp = NULL;
	m_size = 0;
}

CDiskFileLoader::CDiskFileLoader()
{
	Initialize();
}

CDiskFileLoader::~CDiskFileLoader()
{
	Close();
}
