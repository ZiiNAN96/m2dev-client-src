#pragma once
#include "TerrainTextureData.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <limits>
#include <string>

namespace Renderer
{
// ZiiNAN: Backend-neutral graphics resource ownership
inline std::atomic<size_t> liveSourceTextures{0}, liveSourceBuffers{0};

struct TextureDesc
{
    uint32_t width = 0, height = 0, mipLevels = 0;
    TerrainTextureFormat format = TerrainTextureFormat::Unknown;
};

class TextureResource final : public TerrainTexture
{
public:
    struct Mip { std::vector<uint8_t> pixels; size_t rowStride = 0; bool locked = false; };
    TextureResource() { ++liveSourceTextures; }
    ~TextureResource() { --liveSourceTextures; }
    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;
    TextureDesc desc;
    std::string asset;
    std::vector<Mip> mips;
    uint64_t revision = 0;

    TerrainTextureData View() const
    {
        TerrainTextureData data{desc.width, desc.height, desc.format, {}};
        for (const auto& mip : mips) data.mips.push_back({mip.pixels.data(), mip.pixels.size(), mip.rowStride});
        return data;
    }
    bool Lock(size_t level, int* pitch, void** pixels)
    {
        if (pixels) *pixels = nullptr;
        if (!pitch || !pixels || level >= mips.size() || mips[level].locked ||
            mips[level].rowStride > size_t((std::numeric_limits<int>::max)())) return false;
        auto& mip = mips[level];
        mip.locked = true; *pitch = int(mip.rowStride); *pixels = mip.pixels.data(); return true;
    }
    bool Unlock(size_t level)
    {
        if (level >= mips.size() || !mips[level].locked) return false;
        mips[level].locked = false; ++revision; return true;
    }
    static std::shared_ptr<TextureResource> Copy(const TerrainTextureData& data)
    {
        if (!data.width || !data.height || data.mips.empty() || data.format == TerrainTextureFormat::Unknown) return {};
        auto result = std::make_shared<TextureResource>();
        uint32_t width = data.width, height = data.height;
        for (const auto& src : data.mips)
        {
            const bool bc = data.format == TerrainTextureFormat::BC1 || data.format == TerrainTextureFormat::BC2 || data.format == TerrainTextureFormat::BC3;
            const size_t bytes = bc ? (data.format == TerrainTextureFormat::BC1 ? 8 : 16) :
                (data.format == TerrainTextureFormat::Alpha8 ? 1 : (data.format == TerrainTextureFormat::B5G5R5A1 ? 2 : 4));
            const size_t rows = bc ? (size_t(height)+3)/4 : height;
            const size_t columns = bc ? (size_t(width)+3)/4 : width;
            if (!src.data || src.rowStride < columns*bytes || rows > src.size/src.rowStride) return {};
            Mip mip; mip.rowStride = src.rowStride;
            const auto* begin = static_cast<const uint8_t*>(src.data);
            mip.pixels.assign(begin, begin + rows*src.rowStride); result->mips.push_back(std::move(mip));
            width = (std::max)(1u,width/2); height = (std::max)(1u,height/2);
        }
        result->desc = {data.width,data.height,uint32_t(result->mips.size()),data.format};
        return result;
    }
    static std::shared_ptr<TextureResource> Dynamic(uint32_t width, uint32_t height, TerrainTextureFormat format)
    {
        if (!width || !height || width > 16384 || height > 16384 ||
            (format != TerrainTextureFormat::BGRA8 && format != TerrainTextureFormat::RGBA8 && format != TerrainTextureFormat::Alpha8)) return {};
        const size_t stride = size_t(width)*(format == TerrainTextureFormat::Alpha8 ? 1 : 4);
        auto result = std::make_shared<TextureResource>();
        result->desc = {width,height,1,format};
        result->mips.push_back({std::vector<uint8_t>(stride*height),stride,false});
        return result;
    }
};

class CpuBuffer
{
public:
    CpuBuffer() = default;
    ~CpuBuffer() { Clear(); }
    CpuBuffer(const CpuBuffer&) = delete;
    CpuBuffer& operator=(const CpuBuffer&) = delete;
    bool Create(size_t size)
    {
        Clear(); if (!size) return false;
        bytes.resize(size); ++liveSourceBuffers; return true;
    }
    void Clear()
    {
        if (!bytes.empty()) --liveSourceBuffers;
        std::vector<uint8_t>().swap(bytes); locked = false;
    }
    size_t Size() const { return bytes.size(); }
    bool Lock(size_t offset, size_t size, void** output)
    {
        if (output) *output = nullptr;
        if (!output || locked || bytes.empty() || offset >= bytes.size()) return false;
        if (!size) size = bytes.size()-offset;
        if (size > bytes.size()-offset) return false;
        locked = true; *output = bytes.data()+offset; return true;
    }
    bool Unlock() { if (!locked) return false; locked = false; return true; }
private:
    std::vector<uint8_t> bytes;
    bool locked = false;
};
}
