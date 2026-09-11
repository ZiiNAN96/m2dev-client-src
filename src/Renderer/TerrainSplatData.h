#pragma once
#include "TerrainTextureData.h"
#include <array>
#include <cstdint>

namespace Renderer
{
struct TerrainSplatMaterial { virtual ~TerrainSplatMaterial() = default; };
using TerrainSplatMaterialPtr = std::shared_ptr<TerrainSplatMaterial>;
enum class TerrainColorOp : uint32_t { Texture, ModulateDiffuse, BlendDiffuseAlpha };
enum class TerrainAlphaOp : uint32_t { Texture, Mask, Diffuse, TextureTimesDiffuse };
enum class TerrainFog : uint32_t { None, Exp, Exp2, Linear, Vertex };
struct TerrainSampling
{
    bool wrapU = true, wrapV = true;
    bool linearMin = true, linearMag = true, linearMip = true, useMips = true;
    bool operator==(const TerrainSampling& rhs) const
    {
        return wrapU==rhs.wrapU && wrapV==rhs.wrapV && linearMin==rhs.linearMin &&
            linearMag==rhs.linearMag && linearMip==rhs.linearMip && useMips==rhs.useMips;
    }
};
// Additional existing STP attributes; geometry/indices and its CPU lighting stay unchanged.
struct TerrainSplatVertex
{
    std::array<float,4> diffuse{1,1,1,1};
    float fog = 1;
    std::array<float,2> colorUV{}, alphaUV{};
};
struct TerrainSplatParameters
{
    // HTP: the actual camera-space TEXTURE0/1 matrices at the legacy draw.
    std::array<float,16> colorTransform{}, alphaTransform{};
    std::array<float,4> textureFactor{1,1,1,1}, fogColor{};
    float fogStart=0, fogEnd=1, fogDensity=0;
    bool rangeFog=false, vertexUV=false, blend=true;
    int alphaReference=0; // -1 means disabled, otherwise GREATER reference/255.
    TerrainColorOp colorOp=TerrainColorOp::Texture;
    TerrainAlphaOp alphaOp=TerrainAlphaOp::Mask;
    TerrainFog fog=TerrainFog::None;
    TerrainSampling colorSampling{}, alphaSampling{false,false};
};
}
