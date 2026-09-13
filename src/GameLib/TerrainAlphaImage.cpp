#include "StdAfx.h"
#include "TerrainAlphaImage.h"

// ZiiNAN: Backend-neutral graphics resource ownership
void TerrainAlphaImage::Build(const uint8_t* source, bool fourBit)
{
    Clear();
    if (!source) return;
    std::vector<uint8_t> raw(256*256);
    for (uint32_t y=0;y<256;++y) for (uint32_t x=0;x<256;++x) {
        const auto* p=source+y*258+x;
        raw[y*256+x]=uint8_t((((p[0]+p[1]+p[2]+p[258]+p[260]+p[516]+p[517]+p[518])>>3)+p[259])>>1);
    }
    uint32_t size=256;
    for (auto& mip:m_mips) {
        mip=raw;
        if (fourBit) for (auto& alpha:mip) alpha=uint8_t((alpha>>4)*17);
        if (size==16) break;
        std::vector<uint8_t> next(size*size/4);
        for (uint32_t y=0;y<size;y+=2) for (uint32_t x=0;x<size;x+=2) {
            const auto* p=raw.data()+y*size+x;
            next[(y/2)*(size/2)+x/2]=uint8_t((p[0]+p[1]+p[size]+p[size+1])>>2);
        }
        raw.swap(next); size/=2;
    }
}
void TerrainAlphaImage::Capture(uint32_t level, const void* packed, uint32_t size, size_t pitch, bool fourBit)
{
    if(level>=m_mips.size() || size!=(256u>>level) || !packed || pitch<size*(fourBit ? 2u : 4u)) return;
    auto& pixels=m_mips[level]; pixels.resize(size*size);
    for(uint32_t y=0;y<size;++y) for(uint32_t x=0;x<size;++x)
    {
        const auto* row=static_cast<const uint8_t*>(packed)+y*pitch;
        pixels[y*size+x]=fourBit ? uint8_t((row[x*2+1]>>4)*17) : row[x*4+3];
    }
}
void TerrainAlphaImage::Clear()
{
    if(Renderer::terrainRenderer)
    {
        Renderer::terrainRenderer->ReleaseSplatMaterial(m_material);
        Renderer::terrainRenderer->ReleaseTexture(m_texture);
    }
    else { m_material.reset(); m_texture.reset(); }
    for(auto& mip:m_mips) std::vector<uint8_t>().swap(mip);
}
Renderer::TerrainSplatMaterialPtr TerrainAlphaImage::Material(const Renderer::TerrainTexturePtr& color)
{
    auto* renderer=Renderer::terrainRenderer;
    if(!renderer) return {};
    if(m_material) return m_material;
    Renderer::TerrainTextureData data;
    data.width=data.height=256; data.format=Renderer::TerrainTextureFormat::Alpha8;
    for(uint32_t level=0;level<m_mips.size();++level)
    {
        const uint32_t size=256u>>level;
        if(m_mips[level].size()!=size*size) { renderer->UploadTexture({}); return {}; }
        data.mips.push_back({m_mips[level].data(),m_mips[level].size(),size});
    }
    m_texture=renderer->UploadTexture(data);
    if(m_texture) m_material=renderer->CreateSplatMaterial(color,m_texture);
    if(m_material) for(auto& mip:m_mips) std::vector<uint8_t>().swap(mip);
    return m_material;
}
