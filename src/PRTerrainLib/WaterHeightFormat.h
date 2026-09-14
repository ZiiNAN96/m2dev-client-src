#pragma once

#include <cstdint>
#include <cstring>
#include <span>

namespace TerrainFormat
{
// ZiiNAN: Cross-platform bootstrap
using WaterHeight = std::int32_t;

// Existing water files store either signed 32-bit or legacy unsigned 16-bit heights.
inline bool DecodeWaterHeights(std::span<const std::uint8_t> source,
    std::span<WaterHeight> destination) noexcept
{
    const bool modern = source.size() % sizeof(WaterHeight) == 0 &&
        source.size() / sizeof(WaterHeight) == destination.size();
    const bool legacy = source.size() % sizeof(std::uint16_t) == 0 &&
        source.size() / sizeof(std::uint16_t) == destination.size();
    if (!modern && !legacy)
        return false;

    for (std::size_t index = 0; index < destination.size(); ++index)
    {
        if (modern)
            std::memcpy(&destination[index], source.data() + index * sizeof(WaterHeight), sizeof(WaterHeight));
        else
        {
            std::uint16_t value;
            std::memcpy(&value, source.data() + index * sizeof(value), sizeof(value));
            destination[index] = value;
        }
    }
    return true;
}
}
