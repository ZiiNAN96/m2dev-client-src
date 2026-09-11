#include "StdAfx.h"
#include "TerrainAlphaImage.h"

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
