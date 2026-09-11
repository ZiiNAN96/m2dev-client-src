#pragma once
#include "Renderer/TerrainRenderData.h"
#include <array>
#include <vector>

// Owns only the CPU alpha values already written by PutImage32/16, not a generator.
class TerrainAlphaImage
{
public:
    void Capture(uint32_t level, const void* packed, uint32_t size, size_t pitch, bool fourBit);
    void Clear();
    Renderer::TerrainSplatMaterialPtr Material(const Renderer::TerrainTexturePtr& color);
    const std::vector<uint8_t>& Mip(uint32_t level) const { return m_mips.at(level); }
private:
    std::array<std::vector<uint8_t>,5> m_mips;
    Renderer::TerrainTexturePtr m_texture;
    Renderer::TerrainSplatMaterialPtr m_material;
};
