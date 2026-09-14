#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cassert>

// PacketReader: Safe, bounds-checked packet deserialization from a read-only buffer.
// Used to parse incoming packets after the dispatch loop validates the header and length.
//
// Usage:
//   PacketReader r(packetData, packetLength);
//   uint16_t header = r.ReadU16();
//   uint16_t length = r.ReadU16();
//   uint8_t chatType = r.ReadU8();
//   char message[128];
//   r.ReadString(message, sizeof(message));

class PacketReader
{
public:
	PacketReader(const void* data, size_t size)
		: m_buf(static_cast<const uint8_t*>(data))
		, m_size(size)
		, m_pos(0)
	{
		assert(data != nullptr || size == 0);
	}

	// --- Read primitives ---

	uint8_t ReadU8()
	{
		assert(HasBytes(sizeof(uint8_t)));
		return m_buf[m_pos++];
	}

	uint16_t ReadU16()
	{
		assert(HasBytes(sizeof(uint16_t)));
		uint16_t v;
		std::memcpy(&v, m_buf + m_pos, sizeof(v));
		m_pos += sizeof(v);
		return v;
	}

	uint32_t ReadU32()
	{
		assert(HasBytes(sizeof(uint32_t)));
		uint32_t v;
		std::memcpy(&v, m_buf + m_pos, sizeof(v));
		m_pos += sizeof(v);
		return v;
	}

	int8_t ReadI8() { return static_cast<int8_t>(ReadU8()); }
	int16_t ReadI16() { return static_cast<int16_t>(ReadU16()); }
	int32_t ReadI32() { return static_cast<int32_t>(ReadU32()); }

	uint64_t ReadU64()
	{
		assert(HasBytes(sizeof(uint64_t)));
		uint64_t v;
		std::memcpy(&v, m_buf + m_pos, sizeof(v));
		m_pos += sizeof(v);
		return v;
	}

	float ReadFloat()
	{
		assert(HasBytes(sizeof(float)));
		float v;
		std::memcpy(&v, m_buf + m_pos, sizeof(v));
		m_pos += sizeof(v);
		return v;
	}

	// Read raw bytes
	bool ReadBytes(void* dest, size_t len)
	{
		if (!HasBytes(len))
			return false;
		if (len == 0)
			return true;
		std::memcpy(dest, m_buf + m_pos, len);
		m_pos += len;
		return true;
	}

	// Read a fixed-length string (null-terminated in dest)
	bool ReadString(char* dest, size_t fixedLen)
	{
		if (fixedLen == 0 || !HasBytes(fixedLen))
			return false;
		std::memcpy(dest, m_buf + m_pos, fixedLen);
		dest[fixedLen - 1] = '\0'; // ensure null termination
		m_pos += fixedLen;
		return true;
	}

	// Skip bytes without reading
	bool Skip(size_t len)
	{
		if (!HasBytes(len))
			return false;
		m_pos += len;
		return true;
	}

	// --- Peek (non-consuming) ---

	uint8_t PeekU8() const
	{
		assert(HasBytes(sizeof(uint8_t)));
		return m_buf[m_pos];
	}

	uint16_t PeekU16() const
	{
		assert(HasBytes(sizeof(uint16_t)));
		uint16_t v;
		std::memcpy(&v, m_buf + m_pos, sizeof(v));
		return v;
	}

	uint16_t PeekU16At(size_t offset) const
	{
		assert(offset <= m_size && sizeof(uint16_t) <= m_size - offset);
		uint16_t v;
		std::memcpy(&v, m_buf + offset, sizeof(v));
		return v;
	}

	// --- Read struct (for backward compatibility during migration) ---
	// Reads a struct directly via memcpy. Use sparingly — prefer typed reads.
	template<typename T>
	bool ReadStruct(T& out)
	{
		if (!HasBytes(sizeof(T)))
			return false;
		std::memcpy(&out, m_buf + m_pos, sizeof(T));
		m_pos += sizeof(T);
		return true;
	}

	// --- State ---

	// ZiiNAN: Cross-platform bootstrap
	bool HasBytes(size_t n) const { return n <= m_size - m_pos; }
	size_t Remaining() const { return m_size - m_pos; }
	size_t Position() const { return m_pos; }
	size_t Size() const { return m_size; }
	const uint8_t* Current() const { return m_buf + m_pos; }

	void Reset() { m_pos = 0; }

private:
	const uint8_t* m_buf;
	size_t m_size;
	size_t m_pos;
};
