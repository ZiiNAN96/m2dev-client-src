#pragma once
#include "AssetRuntime.h"

namespace AssetRuntime
{
// ZiiNAN: Modern asset pipeline
std::span<const std::string_view> ModelExtensions();
LoadResult LoadModel(AssetId id, std::span<const std::byte> bytes);
}
