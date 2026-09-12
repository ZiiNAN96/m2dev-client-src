#pragma once
#include "TerrainRenderData.h"
#include <string>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <tuple>

namespace Renderer
{
// ZiiNAN: Diligent effect rendering integration; synchronous, API-neutral CPU draw contract.
struct EffectVertex
{
    std::array<float,3> position{};
    uint32_t color=0xffffffff;
    std::array<float,2> uv{};
};
static_assert(sizeof(EffectVertex)==24);
enum class EffectPart : uint32_t { Particle, Mesh, WeaponTrace, FlyTrace, Snow };
struct EffectResources { std::unordered_map<std::string,TerrainTexturePtr> textures; };
struct EffectSampler
{
    uint32_t addressU=1,addressV=1,min=2,mag=2,mip=0,anisotropy=1,maxMip=0,border=0;
    float lodBias=0;
    bool operator<(const EffectSampler& b) const
    {
        return std::tie(addressU,addressV,min,mag,mip,anisotropy,maxMip,border,lodBias)<
               std::tie(b.addressU,b.addressV,b.min,b.mag,b.mip,b.anisotropy,b.maxMip,b.border,b.lodBias);
    }
};
struct EffectDraw
{
    TerrainMatrices matrices{};
    std::array<float,16> textureTransform{};
    std::array<float,4> factor{1,1,1,1},fogColor{},fogParameters{};
    EffectSampler sampler;
    bool strip=true,blend=true,depthTest=true,depthWrite=false,alphaTest=false,rangeFog=false,textured=true,opaqueTargetAlpha=false;
    // Original fixed-function value domains, independent of any graphics API headers.
    uint32_t src=5,dst=6,blendOp=1,cull=0,depthFunction=4,alphaFunction=5,alphaReference=0;
    uint32_t colorOp=4,colorArg1=3,colorArg2=2,alphaOp=4,alphaArg1=3,alphaArg2=2;
    uint32_t fog=0,textureCoordinates=0,textureTransformFlags=0;
};
inline bool EffectColorOpSupported(uint32_t op) { return op>=1 && op<=6 || op==8; }
inline bool EffectArgumentSupported(uint32_t arg) { return (arg&15)<=3 && (arg&~63u)==0; }
inline bool EffectDrawValid(const EffectDraw& d,uint32_t count)
{
    return count>=3 && (d.strip || count%3==0) && d.src>=1 && d.src<=13 && d.dst>=1 && (d.dst<=11 || d.dst==13) &&
        d.blendOp>=1 && d.blendOp<=5 && d.cull<=2 && d.depthFunction>=1 && d.depthFunction<=8 &&
        d.alphaFunction>=1 && d.alphaFunction<=8 && d.alphaReference<=255 && d.fog<=3 &&
        (EffectColorOpSupported(d.colorOp) || d.colorOp==20) && EffectColorOpSupported(d.alphaOp) &&
        EffectArgumentSupported(d.colorArg1) && EffectArgumentSupported(d.colorArg2) &&
        EffectArgumentSupported(d.alphaArg1) && EffectArgumentSupported(d.alphaArg2) &&
        (d.textureCoordinates==0 || d.textureCoordinates==0x20000) &&
        (d.textureTransformFlags==0 || d.textureTransformFlags==2) &&
        d.sampler.addressU>=1 && d.sampler.addressU<=5 && d.sampler.addressV>=1 && d.sampler.addressV<=5 &&
        d.sampler.min<=3 && d.sampler.mag<=3 && d.sampler.mip<=2 && d.sampler.anisotropy>=1 && d.sampler.anisotropy<=16 &&
        std::isfinite(d.sampler.lodBias);
}
struct EffectRuntimeCounts { uint32_t instances=0,systems=0,particles=0; };
inline EffectRuntimeCounts effectRuntime;
class IEffectRenderer : public ITextureUploader
{
public:
    virtual void Draw(const EffectVertex*,uint32_t,const TerrainTexturePtr&,const EffectDraw&,EffectPart)=0;
    virtual void ReportFailure()=0;
};
inline IEffectRenderer* effectRenderer=nullptr;
inline bool effectWorldFrame=false;
inline uint64_t effectFrameSerial=0;
inline uint32_t effectVisibleParticles=0;
}
