// ZiiNAN: Shared read-only fixed-function snapshot for effects and world surfaces.
#include "StdAfx.h"
#include "EterLib/NativeStateView.h"
#include "NativeMaterialSnapshot.h"
#include "StateManager.h"

bool CaptureNativeMaterial(Renderer::EffectDraw& d,std::string& error,bool allowSecondary)
{
    bool ok=true;
    const auto rs=[&](D3DRENDERSTATETYPE t) { DWORD v=0; ok=SUCCEEDED(NativeStateView().GetRenderState(t,&v)) && ok; return v; };
    const auto ts=[&](D3DTEXTURESTAGESTATETYPE t) { DWORD v=0; ok=SUCCEEDED(NativeStateView().GetTextureStageState(0,t,&v)) && ok; return v; };
    const auto ss=[&](D3DSAMPLERSTATETYPE t) { DWORD v=0; ok=SUCCEEDED(NativeStateView().GetSamplerState(0,t,&v)) && ok; return v; };
    const auto asFloat=[](DWORD v) { float f; memcpy(&f,&v,4); return f; };
    d.blend=rs(D3DRS_ALPHABLENDENABLE)!=0; d.src=rs(D3DRS_SRCBLEND); d.dst=rs(D3DRS_DESTBLEND); d.blendOp=rs(D3DRS_BLENDOP);
    d.depthTest=rs(D3DRS_ZENABLE)!=0; d.depthWrite=rs(D3DRS_ZWRITEENABLE)!=0; d.depthFunction=rs(D3DRS_ZFUNC);
    d.cull=rs(D3DRS_CULLMODE)-1; d.alphaTest=rs(D3DRS_ALPHATESTENABLE)!=0; d.alphaFunction=rs(D3DRS_ALPHAFUNC); d.alphaReference=rs(D3DRS_ALPHAREF);
    d.colorOp=ts(D3DTSS_COLOROP); d.colorArg1=ts(D3DTSS_COLORARG1); d.colorArg2=ts(D3DTSS_COLORARG2);
    d.alphaOp=ts(D3DTSS_ALPHAOP); d.alphaArg1=ts(D3DTSS_ALPHAARG1); d.alphaArg2=ts(D3DTSS_ALPHAARG2);
    d.textureCoordinates=ts(D3DTSS_TEXCOORDINDEX); d.textureTransformFlags=ts(D3DTSS_TEXTURETRANSFORMFLAGS);
    D3DXCOLOR factor(rs(D3DRS_TEXTUREFACTOR)); d.factor={factor.r,factor.g,factor.b,factor.a};
    d.sampler.addressU=ss(D3DSAMP_ADDRESSU); d.sampler.addressV=ss(D3DSAMP_ADDRESSV);
    d.sampler.min=ss(D3DSAMP_MINFILTER); d.sampler.mag=ss(D3DSAMP_MAGFILTER); d.sampler.mip=ss(D3DSAMP_MIPFILTER);
    d.sampler.anisotropy=std::clamp(ss(D3DSAMP_MAXANISOTROPY),DWORD(1),DWORD(16));
    d.sampler.maxMip=ss(D3DSAMP_MAXMIPLEVEL); d.sampler.lodBias=asFloat(ss(D3DSAMP_MIPMAPLODBIAS)); d.sampler.border=ss(D3DSAMP_BORDERCOLOR);
    for(auto entry:{std::pair<Renderer::MatrixSlot,std::array<float,16>*>(Renderer::MatrixWorld,&d.matrices.world),
         {Renderer::MatrixView,&d.matrices.view},{Renderer::MatrixProjection,&d.matrices.projection},{Renderer::MatrixTexture0,&d.textureTransform}}) {
        D3DXMATRIX matrix; ok=SUCCEEDED(NativeStateView().GetTransform(entry.first,&matrix)) && ok; memcpy(entry.second->data(),&matrix,64);
    }
    if(rs(D3DRS_FOGENABLE)) {
        if(rs(D3DRS_FOGTABLEMODE)!=D3DFOG_NONE) { error="native fog table="+std::to_string(rs(D3DRS_FOGTABLEMODE)); return false; }
        d.fog=rs(D3DRS_FOGVERTEXMODE); d.rangeFog=rs(D3DRS_RANGEFOGENABLE)!=0;
        d.fogParameters={asFloat(rs(D3DRS_FOGSTART)),asFloat(rs(D3DRS_FOGEND)),asFloat(rs(D3DRS_FOGDENSITY)),0};
        D3DXCOLOR color(rs(D3DRS_FOGCOLOR)); d.fogColor={color.r,color.g,color.b,color.a};
        if(d.fog==3 && d.fogParameters[0]==d.fogParameters[1]) return false;
    }
    IDirect3DVertexShader9* vs=nullptr; IDirect3DPixelShader9* ps=nullptr;
    NativeStateView().GetVertexShader(&vs); NativeStateView().GetPixelShader(&ps);
    const bool shaders=vs || ps; if(vs) vs->Release(); if(ps) ps->Release();
    const auto second=NativeStateView().GetTextureBinding(1);
    DWORD secondOp=0; NativeStateView().GetTextureStageState(1,D3DTSS_COLOROP,&secondOp);
    const bool secondUsed=second && secondOp!=D3DTOP_DISABLE;
    if(allowSecondary && secondUsed) {
        const auto ts1=[&](D3DTEXTURESTAGESTATETYPE t) { DWORD v=0; ok=SUCCEEDED(NativeStateView().GetTextureStageState(1,t,&v)) && ok; return v; };
        const auto ss1=[&](D3DSAMPLERSTATETYPE t) { DWORD v=0; ok=SUCCEEDED(NativeStateView().GetSamplerState(1,t,&v)) && ok; return v; };
        d.secondaryColorOp=ts1(D3DTSS_COLOROP); d.secondaryColorArg1=ts1(D3DTSS_COLORARG1); d.secondaryColorArg2=ts1(D3DTSS_COLORARG2);
        d.secondaryAlphaOp=ts1(D3DTSS_ALPHAOP); d.secondaryAlphaArg1=ts1(D3DTSS_ALPHAARG1); d.secondaryAlphaArg2=ts1(D3DTSS_ALPHAARG2);
        d.secondaryCoordinates=ts1(D3DTSS_TEXCOORDINDEX); d.secondaryTransformFlags=ts1(D3DTSS_TEXTURETRANSFORMFLAGS);
        auto& s=d.secondarySampler; s.addressU=ss1(D3DSAMP_ADDRESSU); s.addressV=ss1(D3DSAMP_ADDRESSV);
        s.min=ss1(D3DSAMP_MINFILTER); s.mag=ss1(D3DSAMP_MAGFILTER); s.mip=ss1(D3DSAMP_MIPFILTER);
        s.anisotropy=std::clamp(ss1(D3DSAMP_MAXANISOTROPY),DWORD(1),DWORD(16));
        s.maxMip=ss1(D3DSAMP_MAXMIPLEVEL); s.lodBias=asFloat(ss1(D3DSAMP_MIPMAPLODBIAS)); s.border=ss1(D3DSAMP_BORDERCOLOR);
        D3DXMATRIX matrix; ok=SUCCEEDED(NativeStateView().GetTransform(Renderer::MatrixTexture1,&matrix)) && ok; memcpy(d.secondaryTransform.data(),&matrix,64);
    }
    d.opaqueTargetAlpha=true;
    // ZiiNAN: Diligent text rendering integration; LCD passes preserve target alpha.
    d.colorWriteMask=rs(D3DRS_COLORWRITEENABLE)&15u;
    const bool valid=ok && !shaders && (!secondUsed || allowSecondary) && !rs(D3DRS_SEPARATEALPHABLENDENABLE) && !rs(D3DRS_STENCILENABLE) &&
        d.colorWriteMask<=15 && rs(D3DRS_FILLMODE)==D3DFILL_SOLID;
    if(!valid) error="native snapshot shaders="+std::to_string(shaders)+" second="+std::to_string(secondUsed)+
        " separate="+std::to_string(rs(D3DRS_SEPARATEALPHABLENDENABLE))+" stencil="+std::to_string(rs(D3DRS_STENCILENABLE))+
        " colorwrite="+std::to_string(rs(D3DRS_COLORWRITEENABLE))+" fill="+std::to_string(rs(D3DRS_FILLMODE));
    return valid;
}

// ZiiNAN: Water's no-normal FVF still receives native ambient/emissive lighting at far NULL-texture draws.
bool ResolveNativeWaterDiffuse(const Renderer::EffectVertex* input,uint32_t count,std::vector<Renderer::EffectVertex>& output)
{
    DWORD lighting=0; if(FAILED(NativeStateView().GetRenderState(D3DRS_LIGHTING,&lighting))) return false;
    if(!lighting) return true;
    D3DMATERIAL9 material{}; D3DXMATRIX world;
    if(FAILED(NativeStateView().GetMaterial(&material)) || FAILED(NativeStateView().GetTransform(Renderer::MatrixWorld,&world))) return false;
    DWORD ambient=0,colorVertex=0,ambientSource=0,emissiveSource=0,diffuseSource=0;
    for(auto entry:{std::pair{D3DRS_AMBIENT,&ambient},{D3DRS_COLORVERTEX,&colorVertex},
        {D3DRS_AMBIENTMATERIALSOURCE,&ambientSource},{D3DRS_EMISSIVEMATERIALSOURCE,&emissiveSource},{D3DRS_DIFFUSEMATERIALSOURCE,&diffuseSource}})
        if(FAILED(NativeStateView().GetRenderState(entry.first,entry.second))) return false;
    struct Light { D3DLIGHT9 value; };
    std::vector<Light> lights;
    for(DWORD i=0;i<8;++i) {
        BOOL enabled=FALSE; if(FAILED(NativeStateView().GetLightEnable(i,&enabled)) || !enabled) continue;
        D3DLIGHT9 light{}; if(FAILED(NativeStateView().GetLight(i,&light))) return false;
        if(light.Type<D3DLIGHT_POINT || light.Type>D3DLIGHT_DIRECTIONAL) return false;
        lights.push_back({light});
    }
    output.assign(input,input+count);
    for(auto& vertex:output) {
        const D3DXCOLOR color(vertex.color);
        const auto source=[&](DWORD selector,const D3DCOLORVALUE& fallback) {
            // COLOR2 is absent from Water's FVF: D3D9 falls back to the material, not black.
            if(!colorVertex || selector!=D3DMCS_COLOR1) return D3DXCOLOR(fallback.r,fallback.g,fallback.b,fallback.a);
            return color;
        };
        const auto a=source(ambientSource,material.Ambient),e=source(emissiveSource,material.Emissive),d=source(diffuseSource,material.Diffuse);
        D3DXCOLOR illumination(ambient);
        D3DXVECTOR3 position(vertex.position.data()); D3DXVec3TransformCoord(&position,&position,&world);
        for(const auto& entry:lights) {
            const auto& light=entry.value; float attenuation=1;
            if(light.Type!=D3DLIGHT_DIRECTIONAL) {
                D3DXVECTOR3 delta=position-D3DXVECTOR3(light.Position.x,light.Position.y,light.Position.z);
                const float distance=D3DXVec3Length(&delta);
                if(distance>light.Range) continue;
                const float denominator=light.Attenuation0+light.Attenuation1*distance+light.Attenuation2*distance*distance;
                attenuation=denominator>0 ? 1/denominator : 0;
                if(light.Type==D3DLIGHT_SPOT) {
                    D3DXVec3Normalize(&delta,&delta);
                    const D3DXVECTOR3 direction(light.Direction.x,light.Direction.y,light.Direction.z);
                    const float rho=D3DXVec3Dot(&delta,&direction),outer=std::cos(light.Phi*.5f),inner=std::cos(light.Theta*.5f);
                    attenuation*=rho<=outer ? 0 : (rho>=inner ? 1 : std::pow((rho-outer)/(inner-outer),light.Falloff));
                }
            }
            illumination.r+=light.Ambient.r*attenuation; illumination.g+=light.Ambient.g*attenuation; illumination.b+=light.Ambient.b*attenuation;
        }
        const auto clamp=[](float f) { return std::clamp(f,0.0f,1.0f); };
        vertex.color=D3DCOLOR_COLORVALUE(clamp(e.r+a.r*illumination.r),clamp(e.g+a.g*illumination.g),clamp(e.b+a.b*illumination.b),clamp(d.a));
    }
    return true;
}
