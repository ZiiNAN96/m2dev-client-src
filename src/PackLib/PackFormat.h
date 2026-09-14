#pragma once

#include <cstddef>
#include <cstdint>

// ZiiNAN: Cross-platform bootstrap
// Existing on-disk layout; independent of crypto/runtime dependencies.
inline constexpr std::size_t PACK_KEY_SIZE = 32;
inline constexpr std::size_t PACK_NONCE_SIZE = 24;
inline constexpr std::size_t PACK_FILENAME_CAPACITY = 261;

#pragma pack(push, 1)
struct TPackFileHeader
{
    std::uint64_t entry_num;
    std::uint64_t data_begin;
    std::uint8_t nonce[PACK_NONCE_SIZE];
};
struct TPackFileEntry
{
    char file_name[PACK_FILENAME_CAPACITY];
    std::uint64_t offset;
    std::uint64_t file_size;
    std::uint64_t compressed_size;
    std::uint8_t encryption;
    std::uint8_t nonce[PACK_NONCE_SIZE];
};
#pragma pack(pop)

static_assert(sizeof(TPackFileHeader) == 40 && alignof(TPackFileHeader) == 1);
static_assert(offsetof(TPackFileHeader, entry_num) == 0);
static_assert(offsetof(TPackFileHeader, data_begin) == 8);
static_assert(offsetof(TPackFileHeader, nonce) == 16);
static_assert(sizeof(TPackFileEntry) == 310 && alignof(TPackFileEntry) == 1);
static_assert(offsetof(TPackFileEntry, file_name) == 0);
static_assert(offsetof(TPackFileEntry, offset) == 261);
static_assert(offsetof(TPackFileEntry, file_size) == 269);
static_assert(offsetof(TPackFileEntry, compressed_size) == 277);
static_assert(offsetof(TPackFileEntry, encryption) == 285);
static_assert(offsetof(TPackFileEntry, nonce) == 286);
