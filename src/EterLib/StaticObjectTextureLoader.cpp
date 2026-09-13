#include "StdAfx.h"
#include "StaticObjectTextureLoader.h"
#include "TerrainTextureLoader.h"
#include "EterImageLib/DDSImageData.h"
#include "PackLib/PackManager.h"

Renderer::TerrainTexturePtr LoadStaticObjectTextureMemory(const void* data, size_t size, Renderer::ITextureUploader& renderer)
{
    ImageData::DDS2DView dds;
    if(data && SUCCEEDED(ImageData::GetDDS2DView(static_cast<const uint8_t*>(data),size,dds)) &&
       dds.format==ImageData::DDSFormat::B5G5R5A1) {
        Renderer::TerrainTextureData upload;
        upload.width=dds.width; upload.height=dds.height;
        upload.format=Renderer::TerrainTextureFormat::B5G5R5A1;
        for(uint32_t i=0;i<dds.mipCount;++i)
            upload.mips.push_back({dds.mips[i].data,dds.mips[i].size,dds.mips[i].rowPitch});
        return renderer.UploadTexture(upload);
    }
    return LoadTerrainTextureMemory(data,size,renderer);
}

Renderer::TerrainTexturePtr LoadStaticObjectTextureFile(const char* filename, Renderer::ITextureUploader& renderer)
{
    TPackFile file;
    if(!filename || !CPackManager::Instance().GetFile(filename,file)) {
        TraceError("Static object texture unavailable: %s",filename ? filename : "null");
        return renderer.UploadTexture({});
    }
    return LoadStaticObjectTextureMemory(file.data(),file.size(),renderer);
}
