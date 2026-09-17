#include "PackManager.h"
#include "EterBase/MapLoadTrace.h"
#include "EterLib/BufferPool.h"
#include <cstdint>
#include <fstream>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>
#include <type_traits>
#include "EterBase/Debug.h"

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

CPackManager::CPackManager()
	: m_load_from_pack(true)
	, m_pBufferPool(nullptr)
{
	m_pBufferPool = new CBufferPool();
}

CPackManager::~CPackManager()
{
	if (m_pBufferPool)
	{
		delete m_pBufferPool;
		m_pBufferPool = nullptr;
	}
}

bool CPackManager::AddPack(const std::string& path)
{
	std::shared_ptr<CPack> pack = std::make_shared<CPack>();

	if (!pack->Load(path))
	{
		return false;
	}

	std::lock_guard<std::mutex> lock(m_mutex);
	const auto& index = pack->GetIndex();
	for (const auto& entry : index)
	{
		m_entries[entry.file_name] = std::make_pair(pack, entry);
	}

	return true;
}

bool CPackManager::GetFile(std::string_view path, TPackFile& result)
{
	return GetFileWithPool(path, result, m_pBufferPool);
}

bool CPackManager::GetFileWithPool(std::string_view path, TPackFile& result, CBufferPool* pPool)
{
    MapLoadTrace::Scope p0lScope("File access","file lookup and loose read","io");
    MapLoadTrace::Count("file-request",path);

	thread_local std::string buf;
	NormalizePath(path, buf);

	// First try to load from pack
	if (m_load_from_pack) {
		auto it = m_entries.find(buf);
		if (it != m_entries.end()) {
			return it->second.first->GetFileWithPool(it->second.second, result, pPool);
		}
	}

	// Fallback to disk (for files not in packs, like bgm folder)
    MapLoadTrace::Count("loose-open-attempt",buf);
	std::ifstream ifs(buf, std::ios::binary);
	if (ifs.is_open()) {
		ifs.seekg(0, std::ios::end);
		const std::streampos end_position = ifs.tellg();
		if (end_position == std::streampos(-1))
			return false;

		const std::streamoff end_offset = static_cast<std::streamoff>(end_position);
		if (end_offset < 0)
			return false;

		const uintmax_t unsigned_size = static_cast<uintmax_t>(end_offset);
		size_t size = 0;
		if (!CheckedUnsignedCast(unsigned_size, size) ||
			unsigned_size > static_cast<uintmax_t>((std::numeric_limits<std::streamsize>::max)())) {
			return false;
		}

		if (size > result.max_size())
			return false;

		ifs.seekg(0, std::ios::beg);
		if (!ifs)
			return false;

		if (size == 0) {
			result.clear();
			return true;
		}

		try {
			if (pPool) {
				result = pPool->Acquire(size);
				result.resize(size);
			} else {
				result.resize(size);
			}
		} catch (const std::bad_alloc&) {
			result.clear();
			return false;
		} catch (const std::length_error&) {
			result.clear();
			return false;
		}

        MapLoadTrace::Count("loose-read",buf,size,true);
		if (ifs.read(reinterpret_cast<char*>(result.data()), static_cast<std::streamsize>(size))) {
			return true;
		}

		result.clear();
	}

	return false;
}

bool CPackManager::IsExist(std::string_view path) const
{
    MapLoadTrace::Scope p0lScope("File access","existence lookup","io");
    MapLoadTrace::Count("exist-request",path);

	thread_local std::string buf;
	NormalizePath(path, buf);

	// First check in pack entries
	if (m_load_from_pack) {
		auto it = m_entries.find(buf);
		if (it != m_entries.end()) {
			return true;
		}
	}

	// Fallback to disk (for files not in packs, like bgm folder)
	std::error_code ec; // To avoid exceptions from std::filesystem
	const auto result = std::filesystem::exists(buf, ec);
	if (ec)
	{
		TraceError("std::filesystem::exists failed for path '%s' with error: %s", buf.c_str(), ec.message().c_str());
		return false;
	}

	return result;
}

void CPackManager::NormalizePath(std::string_view in, std::string& out) const
{
	out.resize(in.size());
	for (std::size_t i = 0; i < out.size(); ++i) {
		if (in[i] == '\\')
			out[i] = '/';
		else
			out[i] = static_cast<char>(std::tolower(in[i]));
	}
}
