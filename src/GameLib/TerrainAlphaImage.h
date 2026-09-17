#pragma once
#include "Renderer/TerrainRenderData.h"
#include <array>
#include <vector>

// Owns the original terrain alpha source and its five filtered mip levels.
class TerrainAlphaImage
{
public:
    void Build(const uint8_t* source, bool fourBit);
    void Capture(uint32_t level, const void* packed, uint32_t size, size_t pitch, bool fourBit);
    void Clear();
    Renderer::TerrainSplatMaterialPtr Material(const Renderer::TerrainTexturePtr& color, bool modernColor=false);
    const std::vector<uint8_t>& Mip(uint32_t level) const { return m_mips.at(level); }
private:
    std::array<std::vector<uint8_t>,5> m_mips;
    Renderer::TerrainTexturePtr m_texture;
    Renderer::TerrainSplatMaterialPtr m_material;
    Renderer::TerrainSplatMaterialPtr m_modernMaterial;
};
