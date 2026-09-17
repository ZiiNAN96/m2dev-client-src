#include "StdAfx.h"
#include "EterBase/MapLoadTrace.h"
#include "TextureSource.h"
#include "StaticObjectTextureLoader.h"

std::shared_ptr<Renderer::TextureResource> DecodeTextureSource(const void* data, size_t size, const char* asset)
{
    MapLoadTrace::Scope p0lScope("Textures","source decode and copy","cpu");
    MapLoadTrace::Count("texture-decode",asset?asset:"",size,true);

    // Reuse the validated CPU DDS/STB decoder; this uploader creates no GPU objects.
    struct SourceCopy final : Renderer::ITextureUploader
    {
        std::shared_ptr<Renderer::TextureResource> result;
        Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData& view) override
        { result = Renderer::TextureResource::Copy(view); return result; }
    } copy;
    LoadStaticObjectTextureMemory(data,size,copy);
    if (copy.result && asset) copy.result->asset = asset;
    return copy.result;
}
