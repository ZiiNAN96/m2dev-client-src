#include "StdAfx.h"
#include "TerrainTextureLoader.h"
#include "ImageDecoder.h"
#include "EterImageLib/DDSImageData.h"
#include "PackLib/PackManager.h"
#include <limits>

Renderer::TerrainTexturePtr LoadTerrainTextureMemory(const void* data, size_t size, Renderer::ITextureUploader& renderer)
{
    using namespace Renderer;
    TerrainTextureData upload;
    const auto fail = [&]() { return renderer.UploadTexture({}); }; // Explicit backend error, never silent fallback.
    if (!data || size < 4 || size > static_cast<size_t>(std::numeric_limits<int>::max())) return fail();
    uint32_t magic;
    memcpy(&magic, data, sizeof(magic));
    if (magic == 0x20534444)
    {
        ImageData::DDS2DView dds;
        if (FAILED(ImageData::GetDDS2DView(static_cast<const uint8_t*>(data), size, dds))) return fail();
        switch (dds.format)
        {
        case ImageData::DDSFormat::BC1: upload.format = TerrainTextureFormat::BC1; break;
        case ImageData::DDSFormat::BC2: upload.format = TerrainTextureFormat::BC2; break;
        case ImageData::DDSFormat::BC3: upload.format = TerrainTextureFormat::BC3; break;
        case ImageData::DDSFormat::BGRA8: upload.format = TerrainTextureFormat::BGRA8; break;
        case ImageData::DDSFormat::BGRX8: upload.format = TerrainTextureFormat::BGRX8; break;
        case ImageData::DDSFormat::RGBA8: upload.format = TerrainTextureFormat::RGBA8; break;
        default: return fail();
        }
        upload.width = dds.width;
        upload.height = dds.height;
        for (uint32_t i=0; i<dds.mipCount; ++i)
            upload.mips.push_back({dds.mips[i].data, dds.mips[i].size, dds.mips[i].rowPitch});
        return renderer.UploadTexture(upload);
    }
    TDecodedImageData image;
    if (!CImageDecoder::DecodeImage(data, size, image) || image.format != TDecodedImageData::FORMAT_RGBA8 || !image.IsValid()) return fail();
    upload.width = image.width;
    upload.height = image.height;
    upload.format = TerrainTextureFormat::RGBA8;
    upload.mips.push_back({image.pixels.data(), image.pixels.size(), size_t(image.width)*4});
    return renderer.UploadTexture(upload);
}

Renderer::TerrainTexturePtr LoadTerrainTextureFile(const char* filename, Renderer::ITextureUploader& renderer)
{
    TPackFile file;
    if (!filename || !CPackManager::Instance().GetFile(filename, file))
    {
        TraceError("Terrain single texture: file unavailable (%s)", filename ? filename : "null");
        return renderer.UploadTexture({});
    }
    auto texture = LoadTerrainTextureMemory(file.data(), file.size(), renderer);
    if (!texture) TraceError("Terrain single texture: unsupported or invalid image (%s)", filename);
    return texture;
}
