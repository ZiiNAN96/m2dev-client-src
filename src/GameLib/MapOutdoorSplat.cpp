#include "StdAfx.h"
#include "MapOutdoor.h"
#include "AreaTerrain.h"
#include "EterLib/DrawState.h"
#include "EterLib/TerrainTextureLoader.h"
#include "Renderer/GraphicsConfig.h"
#include <fstream>

namespace
{
DWORD SamplerState(uint32_t stage, Renderer::SamplerStateKey type)
{
    DWORD value=0; DRAWSTATE.GetSamplerState(stage,type,&value); return value;
}
DWORD StageState(uint32_t stage, Renderer::TextureStageKey type)
{
    DWORD value=0; DRAWSTATE.GetTextureStageState(stage,type,&value); return value;
}
std::array<float,4> Color(DWORD packed)
{
    const Math::Color color(packed);
    return {color.r,color.g,color.b,color.a};
}
float FloatState(Renderer::RenderStateKey type)
{
    const DWORD packed=DRAWSTATE.GetRenderState(type);
    float value; memcpy(&value,&packed,sizeof(value)); return value;
}
bool Sampling(uint32_t stage, Renderer::TerrainSampling& result)
{
    const auto u=SamplerState(stage,Renderer::SamplerAddressU);
    const auto v=SamplerState(stage,Renderer::SamplerAddressV);
    if((u!=Renderer::AddressWrap && u!=Renderer::AddressClamp) || (v!=Renderer::AddressWrap && v!=Renderer::AddressClamp)) return false;
    result.wrapU=u==Renderer::AddressWrap; result.wrapV=v==Renderer::AddressWrap;
    result.linearMin=SamplerState(stage,Renderer::SamplerMinFilter)>=Renderer::FilterLinear;
    result.linearMag=SamplerState(stage,Renderer::SamplerMagFilter)>=Renderer::FilterLinear;
    const auto mip=SamplerState(stage,Renderer::SamplerMipFilter);
    result.linearMip=mip==Renderer::FilterLinear; result.useMips=mip!=Renderer::FilterNone;
    return true;
}
}

void CMapOutdoor::SubmitTerrainSplat(long patchnum, CTerrain* terrain, uint32_t layer)
{
    using namespace Renderer;
    auto* renderer=terrainRenderer;
    if(!renderer) return;
    const auto fail=[&]() {
        TraceError("Terrain splat: unsupported state/resource (layer=%u, software=%d)",layer,CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE);
        renderer->UploadTexture({});
    };
    if(!terrain || !layer || layer>=m_terrainTextures.size()) { fail(); return; }
    const bool modernColor=GetGraphicsRuntimeConfig().style==Graphics::GraphicsStyle::Modern &&
        layer<m_modernTerrainColors.size() && !m_modernTerrainColors[layer].filename.empty();
    auto& color=modernColor ? m_modernTerrainColors[layer].texture : m_terrainTextures[layer];
    if(!color) {
        const auto& filename=modernColor ? m_modernTerrainColors[layer].filename : m_TextureSet.GetTexture(layer).stFilename;
        color=LoadTerrainTextureFile(filename.c_str(),*renderer);
        if(modernColor && color) std::ofstream("terrain-content.log",std::ios::app)
            << "modern color map=" << GetName() << " slot=" << layer << " source=" << filename << '\n';
    }
    if(!color) return;
    const auto material=terrain->GetSplatMaterial(layer,color,modernColor);
    if(!material) { fail(); return; }
    TerrainSplatParameters params;
    params.vertexUV=CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE;
    if(!params.vertexUV)
    {
        Math::Matrix transform;
        DRAWSTATE.GetTransform(Renderer::MatrixTexture0,&transform);
        memcpy(params.colorTransform.data(),&transform,sizeof(transform));
        DRAWSTATE.GetTransform(Renderer::MatrixTexture1,&transform);
        memcpy(params.alphaTransform.data(),&transform,sizeof(transform));
    }
    switch(StageState(0,Renderer::StageColorOp))
    {
    case Renderer::TextureOpSelectArg1: params.colorOp=TerrainColorOp::Texture; break;
    case Renderer::TextureOpModulate: params.colorOp=TerrainColorOp::ModulateDiffuse; break;
    case Renderer::TextureOpBlendDiffuseAlpha: params.colorOp=TerrainColorOp::BlendDiffuseAlpha; break;
    default: fail(); return;
    }
    const DWORD alpha0=StageState(0,Renderer::StageAlphaOp);
    params.alphaOp=alpha0==Renderer::TextureOpModulate ? TerrainAlphaOp::TextureTimesDiffuse : TerrainAlphaOp::Texture;
    if(alpha0!=Renderer::TextureOpModulate && alpha0!=Renderer::TextureOpSelectArg1) { fail(); return; }
    if(StageState(1,Renderer::StageColorOp)!=Renderer::TextureOpDisable)
    {
        switch(StageState(1,Renderer::StageAlphaOp))
        {
        case Renderer::TextureOpDisable: break; // Existing HTP first-pass behavior, GPU-reference tested.
        case Renderer::TextureOpSelectArg1: params.alphaOp=TerrainAlphaOp::Mask; break;
        case Renderer::TextureOpSelectArg2:
            if(StageState(1,Renderer::StageAlphaArg2)!=Renderer::ArgDiffuse) { fail(); return; }
            params.alphaOp=TerrainAlphaOp::Diffuse; break;
        default: fail(); return;
        }
    }
    params.blend=DRAWSTATE.GetRenderState(Renderer::StateAlphaBlendEnable)!=FALSE;
    if(params.blend && (DRAWSTATE.GetRenderState(Renderer::StateSrcBlend)!=Renderer::BlendSrcAlpha ||
       DRAWSTATE.GetRenderState(Renderer::StateDestBlend)!=Renderer::BlendInvSrcAlpha ||
       DRAWSTATE.GetRenderState(Renderer::StateBlendOp)!=Renderer::BlendOpAdd)) { fail(); return; }
    params.alphaReference=-1;
    if(DRAWSTATE.GetRenderState(Renderer::StateAlphaTestEnable))
    {
        if(DRAWSTATE.GetRenderState(Renderer::StateAlphaFunc)!=Renderer::CompareGreater) { fail(); return; }
        params.alphaReference=DRAWSTATE.GetRenderState(Renderer::StateAlphaRef)&255;
    }
    params.textureFactor=Color(DRAWSTATE.GetRenderState(Renderer::StateTextureFactor));
    params.fogColor=Color(DRAWSTATE.GetRenderState(Renderer::StateFogColor));
    if(DRAWSTATE.GetRenderState(Renderer::StateFogEnable))
    {
        if(DRAWSTATE.GetRenderState(Renderer::StateFogTableMode)!=Renderer::FogNone) { fail(); return; }
        params.fog=params.vertexUV ? TerrainFog::Vertex : TerrainFog(DRAWSTATE.GetRenderState(Renderer::StateFogVertexMode));
        params.fogStart=FloatState(Renderer::StateFogStart); params.fogEnd=FloatState(Renderer::StateFogEnd);
        params.fogDensity=FloatState(Renderer::StateFogDensity);
        params.rangeFog=DRAWSTATE.GetRenderState(Renderer::StateRangeFogEnable)!=FALSE;
    }
    if(!Sampling(0,params.colorSampling) || !Sampling(1,params.alphaSampling)) { fail(); return; }
    renderer->DrawSplat(m_pTerrainPatchProxyList[patchnum].GetTerrainGeometry(),m_terrainIndices[m_terrainGeometryLOD],
                        m_wNumIndices[m_terrainGeometryLOD],m_terrainGeometryLOD==0,material,params);
}
