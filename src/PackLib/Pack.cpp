#include "Pack.h"
#include "EterLib/BufferPool.h"
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <zstd.h>

namespace
{
template <typename Target, typename Source>
bool CheckedUnsignedCast(Source value, Target& result)
{
	static_assert(std::is_unsigned_v<Target> && std::is_unsigned_v<Source>);
	if constexpr ((std::numeric_limits<Source>::max)() > (std::numeric_limits<Target>::max)()) {
		if (value > static_cast<Source>((std::numeric_limits<Target>::max)()))
			return false;
	}

	result = static_cast<Target>(value);
	return true;
}
}

static thread_local ZSTD_DCtx* g_zstdDCtx = nullptr;

static ZSTD_DCtx* GetThreadLocalZSTDContext()
{
	if (!g_zstdDCtx)
	{
		g_zstdDCtx = ZSTD_createDCtx();
	}
	return g_zstdDCtx;
}

void CPack::DecryptData(uint8_t* data, size_t len, const uint8_t* nonce)
{
	crypto_stream_xchacha20_xor(data, data, len, nonce, PACK_KEY.data());
}

bool CPack::Load(const std::string& path)
{
	m_header = {};
	m_index.clear();

	std::error_code ec;
	m_file.map(path, ec);

	if (ec) {
		return false;
	}

	const size_t file_size = m_file.size();
	if (file_size < sizeof(TPackFileHeader)) {
		return false;
	}

	TPackFileHeader header{};
	std::memcpy(&header, m_file.data(), sizeof(header));

	size_t entry_count = 0;
	if (!CheckedUnsignedCast(header.entry_num, entry_count) ||
		entry_count > (file_size - sizeof(TPackFileHeader)) / sizeof(TPackFileEntry)) {
		return false;
	}

	std::vector<TPackFileEntry> index;
	if (entry_count > index.max_size())
		return false;

	const size_t index_end = sizeof(TPackFileHeader) + entry_count * sizeof(TPackFileEntry);
	size_t data_begin = 0;
	if (!CheckedUnsignedCast(header.data_begin, data_begin) || data_begin < index_end || data_begin > file_size)
		return false;

	try {
		index.resize(entry_count);
	} catch (const std::bad_alloc&) {
		return false;
	} catch (const std::length_error&) {
		return false;
	}
	const size_t file_buffer_max = TPackFile{}.max_size();

	for (size_t i = 0; i < entry_count; i++) {
		TPackFileEntry& entry = index[i];
		std::memcpy(&entry, m_file.data() + sizeof(TPackFileHeader) + i * sizeof(TPackFileEntry), sizeof(TPackFileEntry));
		DecryptData(reinterpret_cast<uint8_t*>(&entry), sizeof(entry), header.nonce);

		if (std::memchr(entry.file_name, '\0', sizeof(entry.file_name)) == nullptr)
			return false;

		size_t entry_offset = 0;
		size_t compressed_size = 0;
		size_t unpacked_size = 0;
		const size_t data_size = file_size - data_begin;
		if (!CheckedUnsignedCast(entry.offset, entry_offset) || !CheckedUnsignedCast(entry.compressed_size, compressed_size) ||
			!CheckedUnsignedCast(entry.file_size, unpacked_size) || compressed_size > file_buffer_max ||
			unpacked_size > file_buffer_max || compressed_size == 0 || entry.encryption > 1 ||
			entry_offset > data_size || compressed_size > data_size - entry_offset) {
			return false;
		}
	}

	m_header = header;
	m_index = std::move(index);
	return true;
}

bool CPack::GetFile(const TPackFileEntry& entry, TPackFile& result)
{
	return GetFileWithPool(entry, result, nullptr);
}

bool CPack::GetFileWithPool(const TPackFileEntry& entry, TPackFile& result, CBufferPool* pPool)
{
	const size_t mapped_size = m_file.size();
	size_t data_begin = 0;
	size_t entry_offset = 0;
	size_t compressed_size = 0;
	size_t file_size = 0;
	if (!CheckedUnsignedCast(m_header.data_begin, data_begin) || !CheckedUnsignedCast(entry.offset, entry_offset) ||
		!CheckedUnsignedCast(entry.compressed_size, compressed_size) || !CheckedUnsignedCast(entry.file_size, file_size) ||
		data_begin > mapped_size || entry_offset > mapped_size - data_begin ||
		compressed_size > mapped_size - data_begin - entry_offset || compressed_size == 0 ||
		entry.encryption > 1 || file_size > result.max_size()) {
		return false;
	}

	const size_t offset = data_begin + entry_offset;
	ZSTD_DCtx* dctx = GetThreadLocalZSTDContext();
	if (!dctx)
		return false;

	try {
		result.resize(file_size);
		uint8_t empty_output = 0;
		void* const output = file_size == 0 ? static_cast<void*>(&empty_output) : result.data();

		switch (entry.encryption)
		{
			case 0: {
				size_t decompressed_size = ZSTD_decompressDCtx(dctx, output, result.size(), m_file.data() + offset, compressed_size);
				if (ZSTD_isError(decompressed_size) || decompressed_size != file_size) {
					result.clear();
					return false;
				}
			} break;

			case 1: {
				std::vector<uint8_t> compressed_data;
				if (compressed_size > compressed_data.max_size())
					return false;

				if (pPool) {
					compressed_data = pPool->Acquire(compressed_size);
				}
				compressed_data.resize(compressed_size);

				std::memcpy(compressed_data.data(), m_file.data() + offset, compressed_size);

				DecryptData(compressed_data.data(), compressed_size, entry.nonce);

				size_t decompressed_size = ZSTD_decompressDCtx(dctx, output, result.size(), compressed_data.data(), compressed_data.size());

				if (pPool) {
					pPool->Release(std::move(compressed_data));
				}

				if (ZSTD_isError(decompressed_size) || decompressed_size != file_size) {
					result.clear();
					return false;
				}
			} break;

			default:
				result.clear();
				return false;
		}
	} catch (const std::bad_alloc&) {
		result.clear();
		return false;
	} catch (const std::length_error&) {
		result.clear();
		return false;
	}

	return true;
}
