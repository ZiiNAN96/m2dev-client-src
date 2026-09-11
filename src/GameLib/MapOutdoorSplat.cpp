#include "StdAfx.h"
#include "MapOutdoor.h"
#include "AreaTerrain.h"
#include "EterLib/StateManager.h"
#include "EterLib/TerrainTextureLoader.h"

namespace
{
DWORD SamplerState(uint32_t stage, D3DSAMPLERSTATETYPE type)
{
    DWORD value=0; STATEMANAGER.GetSamplerState(stage,type,&value); return value;
}
DWORD StageState(uint32_t stage, D3DTEXTURESTAGESTATETYPE type)
{
    DWORD value=0; STATEMANAGER.GetTextureStageState(stage,type,&value); return value;
}
std::array<float,4> Color(DWORD packed)
{
    const D3DXCOLOR color(packed);
    return {color.r,color.g,color.b,color.a};
}
float FloatState(D3DRENDERSTATETYPE type)
{
    const DWORD packed=STATEMANAGER.GetRenderState(type);
    float value; memcpy(&value,&packed,sizeof(value)); return value;
}
bool Sampling(uint32_t stage, Renderer::TerrainSampling& result)
{
    const auto u=SamplerState(stage,D3DSAMP_ADDRESSU);
    const auto v=SamplerState(stage,D3DSAMP_ADDRESSV);
    if((u!=D3DTADDRESS_WRAP && u!=D3DTADDRESS_CLAMP) || (v!=D3DTADDRESS_WRAP && v!=D3DTADDRESS_CLAMP)) return false;
    result.wrapU=u==D3DTADDRESS_WRAP; result.wrapV=v==D3DTADDRESS_WRAP;
    result.linearMin=SamplerState(stage,D3DSAMP_MINFILTER)>=D3DTEXF_LINEAR;
    result.linearMag=SamplerState(stage,D3DSAMP_MAGFILTER)>=D3DTEXF_LINEAR;
    const auto mip=SamplerState(stage,D3DSAMP_MIPFILTER);
    result.linearMip=mip==D3DTEXF_LINEAR; result.useMips=mip!=D3DTEXF_NONE;
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
    auto& color=m_terrainTextures[layer];
    if(!color) color=LoadTerrainTextureFile(m_TextureSet.GetTexture(layer).stFilename.c_str(),*renderer);
    if(!color) return;
    const auto material=terrain->GetSplatMaterial(layer,color);
    if(!material) { fail(); return; }
    TerrainSplatParameters params;
    params.vertexUV=CTerrainPatch::SOFTWARE_TRANSFORM_PATCH_ENABLE;
    if(!params.vertexUV)
    {
        D3DXMATRIX transform;
        STATEMANAGER.GetTransform(D3DTS_TEXTURE0,&transform);
        memcpy(params.colorTransform.data(),&transform,sizeof(transform));
        STATEMANAGER.GetTransform(D3DTS_TEXTURE1,&transform);
        memcpy(params.alphaTransform.data(),&transform,sizeof(transform));
    }
    switch(StageState(0,D3DTSS_COLOROP))
    {
    case D3DTOP_SELECTARG1: params.colorOp=TerrainColorOp::Texture; break;
    case D3DTOP_MODULATE: params.colorOp=TerrainColorOp::ModulateDiffuse; break;
    case D3DTOP_BLENDDIFFUSEALPHA: params.colorOp=TerrainColorOp::BlendDiffuseAlpha; break;
    default: fail(); return;
    }
    const DWORD alpha0=StageState(0,D3DTSS_ALPHAOP);
    params.alphaOp=alpha0==D3DTOP_MODULATE ? TerrainAlphaOp::TextureTimesDiffuse : TerrainAlphaOp::Texture;
    if(alpha0!=D3DTOP_MODULATE && alpha0!=D3DTOP_SELECTARG1) { fail(); return; }
    if(StageState(1,D3DTSS_COLOROP)!=D3DTOP_DISABLE)
    {
        switch(StageState(1,D3DTSS_ALPHAOP))
        {
        case D3DTOP_DISABLE: break; // Existing HTP first-pass behavior, GPU-reference tested.
        case D3DTOP_SELECTARG1: params.alphaOp=TerrainAlphaOp::Mask; break;
        case D3DTOP_SELECTARG2:
            if(StageState(1,D3DTSS_ALPHAARG2)!=D3DTA_DIFFUSE) { fail(); return; }
            params.alphaOp=TerrainAlphaOp::Diffuse; break;
        default: fail(); return;
        }
    }
    params.blend=STATEMANAGER.GetRenderState(D3DRS_ALPHABLENDENABLE)!=FALSE;
    if(params.blend && (STATEMANAGER.GetRenderState(D3DRS_SRCBLEND)!=D3DBLEND_SRCALPHA ||
       STATEMANAGER.GetRenderState(D3DRS_DESTBLEND)!=D3DBLEND_INVSRCALPHA ||
       STATEMANAGER.GetRenderState(D3DRS_BLENDOP)!=D3DBLENDOP_ADD)) { fail(); return; }
    params.alphaReference=-1;
    if(STATEMANAGER.GetRenderState(D3DRS_ALPHATESTENABLE))
    {
        if(STATEMANAGER.GetRenderState(D3DRS_ALPHAFUNC)!=D3DCMP_GREATER) { fail(); return; }
        params.alphaReference=STATEMANAGER.GetRenderState(D3DRS_ALPHAREF)&255;
    }
    params.textureFactor=Color(STATEMANAGER.GetRenderState(D3DRS_TEXTUREFACTOR));
    params.fogColor=Color(STATEMANAGER.GetRenderState(D3DRS_FOGCOLOR));
    if(STATEMANAGER.GetRenderState(D3DRS_FOGENABLE))
    {
        if(STATEMANAGER.GetRenderState(D3DRS_FOGTABLEMODE)!=D3DFOG_NONE) { fail(); return; }
        params.fog=params.vertexUV ? TerrainFog::Vertex : TerrainFog(STATEMANAGER.GetRenderState(D3DRS_FOGVERTEXMODE));
        params.fogStart=FloatState(D3DRS_FOGSTART); params.fogEnd=FloatState(D3DRS_FOGEND);
        params.fogDensity=FloatState(D3DRS_FOGDENSITY);
        params.rangeFog=STATEMANAGER.GetRenderState(D3DRS_RANGEFOGENABLE)!=FALSE;
    }
    if(!Sampling(0,params.colorSampling) || !Sampling(1,params.alphaSampling)) { fail(); return; }
    renderer->DrawSplat(m_pTerrainPatchProxyList[patchnum].GetTerrainGeometry(),m_terrainIndices[m_terrainGeometryLOD],
                        m_wNumIndices[m_terrainGeometryLOD],m_terrainGeometryLOD==0,material,params);
}
