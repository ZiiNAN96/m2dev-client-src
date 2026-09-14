#include "Providers.h"
#include "GlTF/GlTFAssetProvider.h"
#include "GR2/GR2AssetProvider.h"
#include <algorithm>

namespace AssetRuntime
{
std::span<const std::string_view> ModelExtensions()
{
    static constexpr std::array<std::string_view, 2> extensions{"gr2", "glb"};
    return extensions;
}
LoadResult LoadModel(AssetId id, std::span<const std::byte> bytes)
{
    const auto dot = id.find_last_of('.');
    if (dot == std::string::npos) return {{}, AssetError::UnsupportedLayout};
    std::string extension = id.substr(dot + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
        return static_cast<char>(c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c);
    });
    AssetProvider* provider = nullptr;
    if (extension == "gr2") provider = &GetGR2AssetProvider();
    else if (extension == "glb") provider = &GetGlTFAssetProvider();
    if (!provider) return {{}, AssetError::UnsupportedLayout};
    return LoadModel(std::move(id), bytes, *provider);
}
}
