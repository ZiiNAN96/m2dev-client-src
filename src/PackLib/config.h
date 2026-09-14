#pragma once
#include "PackFormat.h"
#include <cstddef>
#include <cstdint>
#include <array>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

#include <sodium.h>

static_assert(PACK_KEY_SIZE == crypto_stream_xchacha20_KEYBYTES);
static_assert(PACK_NONCE_SIZE == crypto_stream_xchacha20_NONCEBYTES);

constexpr std::array<uint8_t, PACK_KEY_SIZE> PACK_KEY = {
	0x00,0x11,0x22,0x33, 0x44,0x55,0x66,0x77,
	0x88,0x99,0xAA,0xBB, 0xCC,0xDD,0xEE,0xFF,
	0x01,0x23,0x45,0x67, 0x89,0xAB,0xCD,0xEF,
	0xFE,0xDC,0xBA,0x98, 0x76,0x54,0x32,0x10
};

class CPack;
using TPackFile = std::vector<uint8_t>;
using TPackFileMapEntry = std::pair<std::shared_ptr<CPack>, TPackFileEntry>;
using TPackFileMap = std::unordered_map<std::string, TPackFileMapEntry>;
