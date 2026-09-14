#ifndef __INC_CRC32_H__
#define __INC_CRC32_H__

#include <cstdint>
#include <cstddef>

// ZiiNAN: Platform abstraction
std::uint32_t GetCRC32(const char* buffer, size_t count);
std::uint32_t GetCaseCRC32(const char * buf, size_t len);
std::uint32_t GetFileCRC32(const wchar_t* c_szFileName);
std::uint32_t GetFileCRC32(const char* fileUtf8);
std::uint32_t GetFileSize(const char* c_szFileName);

#endif
