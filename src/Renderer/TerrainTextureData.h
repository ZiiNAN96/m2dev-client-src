#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace Renderer
{
enum class TerrainTextureFormat { Unknown, RGBA8, BGRA8, BGRX8, BC1, BC2, BC3, Alpha8, B5G5R5A1 };
struct TerrainTextureMip
{
    const void* data = nullptr;
    size_t size = 0;
    size_t rowStride = 0;
};
// Borrowed views, consumed synchronously by UploadTexture. No decoder/backend types.
struct TerrainTextureData
{
    uint32_t width = 0, height = 0;
    TerrainTextureFormat format = TerrainTextureFormat::Unknown;
    std::vector<TerrainTextureMip> mips;
};
struct TerrainTexture { virtual ~TerrainTexture() = default; };
using TerrainTexturePtr = std::shared_ptr<TerrainTexture>;
// Shared image-upload boundary; existing names retained for terrain compatibility.
class ITextureUploader
{
public:
    ITextureUploader() = default;
    ITextureUploader(const ITextureUploader&) : ITextureUploader() {}
    ITextureUploader& operator=(const ITextureUploader&) { cacheLifetime_ = std::make_shared<unsigned char>(); return *this; }
    virtual ~ITextureUploader() = default;
    virtual TerrainTexturePtr UploadTexture(const TerrainTextureData&) = 0;
    std::weak_ptr<const void> TextureCacheLifetime() const { return cacheLifetime_; }
private:
    std::shared_ptr<const void> cacheLifetime_ = std::make_shared<unsigned char>();
};
}
