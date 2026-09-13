// ZiiNAN: CPU material snapshot for effects and world surfaces.
#include "StdAfx.h"
#include "EterLib/DrawStateView.h"
#include "MaterialStateSnapshot.h"
#include "DrawState.h"

bool CaptureMaterialState(Renderer::EffectDraw& d,std::string& error,bool allowSecondary)
{
    bool ok=true;
    const auto rs=[&](Renderer::RenderStateKey t) { DWORD v=0; ok=SUCCEEDED(DrawStateView().GetRenderState(t,&v)) && ok; return v; };
    const auto ts=[&](Renderer::TextureStageKey t) { DWORD v=0; ok=SUCCEEDED(DrawStateView().GetTextureStageState(0,t,&v)) && ok; return v; };
    const auto ss=[&](Renderer::SamplerStateKey t) { DWORD v=0; ok=SUCCEEDED(DrawStateView().GetSamplerState(0,t,&v)) && ok; return v; };
    const auto asFloat=[](DWORD v) { float f; memcpy(&f,&v,4); return f; };
    d.blend=rs(Renderer::StateAlphaBlendEnable)!=0; d.src=rs(Renderer::StateSrcBlend); d.dst=rs(Renderer::StateDestBlend); d.blendOp=rs(Renderer::StateBlendOp);
    d.depthTest=rs(Renderer::StateZEnable)!=0; d.depthWrite=rs(Renderer::StateZWriteEnable)!=0; d.depthFunction=rs(Renderer::StateZFunc);
    d.cull=rs(Renderer::StateCullMode)-1; d.alphaTest=rs(Renderer::StateAlphaTestEnable)!=0; d.alphaFunction=rs(Renderer::StateAlphaFunc); d.alphaReference=rs(Renderer::StateAlphaRef);
    d.colorOp=ts(Renderer::StageColorOp); d.colorArg1=ts(Renderer::StageColorArg1); d.colorArg2=ts(Renderer::StageColorArg2);
    d.alphaOp=ts(Renderer::StageAlphaOp); d.alphaArg1=ts(Renderer::StageAlphaArg1); d.alphaArg2=ts(Renderer::StageAlphaArg2);
    d.textureCoordinates=ts(Renderer::StageTexCoordIndex); d.textureTransformFlags=ts(Renderer::StageTextureTransformFlags);
    Math::Color factor(rs(Renderer::StateTextureFactor)); d.factor={factor.r,factor.g,factor.b,factor.a};
    d.sampler.addressU=ss(Renderer::SamplerAddressU); d.sampler.addressV=ss(Renderer::SamplerAddressV);
    d.sampler.min=ss(Renderer::SamplerMinFilter); d.sampler.mag=ss(Renderer::SamplerMagFilter); d.sampler.mip=ss(Renderer::SamplerMipFilter);
    d.sampler.anisotropy=std::clamp(ss(Renderer::SamplerMaxAnisotropy),DWORD(1),DWORD(16));
    d.sampler.maxMip=ss(Renderer::SamplerMaxMipLevel); d.sampler.lodBias=asFloat(ss(Renderer::SamplerMipMapLodBias)); d.sampler.border=ss(Renderer::SamplerBorderColor);
    for(auto entry:{std::pair<Renderer::MatrixSlot,std::array<float,16>*>(Renderer::MatrixWorld,&d.matrices.world),
         {Renderer::MatrixView,&d.matrices.view},{Renderer::MatrixProjection,&d.matrices.projection},{Renderer::MatrixTexture0,&d.textureTransform}}) {
        Math::Matrix matrix; ok=SUCCEEDED(DrawStateView().GetTransform(entry.first,&matrix)) && ok; memcpy(entry.second->data(),&matrix,64);
    }
    if(rs(Renderer::StateFogEnable)) {
        if(rs(Renderer::StateFogTableMode)!=Renderer::FogNone) { error="native fog table="+std::to_string(rs(Renderer::StateFogTableMode)); return false; }
        d.fog=rs(Renderer::StateFogVertexMode); d.rangeFog=rs(Renderer::StateRangeFogEnable)!=0;
        d.fogParameters={asFloat(rs(Renderer::StateFogStart)),asFloat(rs(Renderer::StateFogEnd)),asFloat(rs(Renderer::StateFogDensity)),0};
        Math::Color color(rs(Renderer::StateFogColor)); d.fogColor={color.r,color.g,color.b,color.a};
        if(d.fog==3 && d.fogParameters[0]==d.fogParameters[1]) return false;
    }

    const auto second=DrawStateView().GetTextureBinding(1);
    DWORD secondOp=0; DrawStateView().GetTextureStageState(1,Renderer::StageColorOp,&secondOp);
    const bool secondUsed=second && secondOp!=Renderer::TextureOpDisable;
    if(allowSecondary && secondUsed) {
        const auto ts1=[&](Renderer::TextureStageKey t) { DWORD v=0; ok=SUCCEEDED(DrawStateView().GetTextureStageState(1,t,&v)) && ok; return v; };
        const auto ss1=[&](Renderer::SamplerStateKey t) { DWORD v=0; ok=SUCCEEDED(DrawStateView().GetSamplerState(1,t,&v)) && ok; return v; };
        d.secondaryColorOp=ts1(Renderer::StageColorOp); d.secondaryColorArg1=ts1(Renderer::StageColorArg1); d.secondaryColorArg2=ts1(Renderer::StageColorArg2);
        d.secondaryAlphaOp=ts1(Renderer::StageAlphaOp); d.secondaryAlphaArg1=ts1(Renderer::StageAlphaArg1); d.secondaryAlphaArg2=ts1(Renderer::StageAlphaArg2);
        d.secondaryCoordinates=ts1(Renderer::StageTexCoordIndex); d.secondaryTransformFlags=ts1(Renderer::StageTextureTransformFlags);
        auto& s=d.secondarySampler; s.addressU=ss1(Renderer::SamplerAddressU); s.addressV=ss1(Renderer::SamplerAddressV);
        s.min=ss1(Renderer::SamplerMinFilter); s.mag=ss1(Renderer::SamplerMagFilter); s.mip=ss1(Renderer::SamplerMipFilter);
        s.anisotropy=std::clamp(ss1(Renderer::SamplerMaxAnisotropy),DWORD(1),DWORD(16));
        s.maxMip=ss1(Renderer::SamplerMaxMipLevel); s.lodBias=asFloat(ss1(Renderer::SamplerMipMapLodBias)); s.border=ss1(Renderer::SamplerBorderColor);
        Math::Matrix matrix; ok=SUCCEEDED(DrawStateView().GetTransform(Renderer::MatrixTexture1,&matrix)) && ok; memcpy(d.secondaryTransform.data(),&matrix,64);
    }
    d.opaqueTargetAlpha=true;
    // ZiiNAN: Diligent text rendering integration; LCD passes preserve target alpha.
    d.colorWriteMask=rs(Renderer::StateColorWriteEnable)&15u;
    const bool valid=ok && (!secondUsed || allowSecondary) && !rs(Renderer::StateSeparateAlphaBlendEnable) && !rs(Renderer::StateStencilEnable) &&
        d.colorWriteMask<=15 && rs(Renderer::StateFillMode)==Renderer::FillSolid;
    if(!valid) error="material snapshot second="+std::to_string(secondUsed)+
        " separate="+std::to_string(rs(Renderer::StateSeparateAlphaBlendEnable))+" stencil="+std::to_string(rs(Renderer::StateStencilEnable))+
        " colorwrite="+std::to_string(rs(Renderer::StateColorWriteEnable))+" fill="+std::to_string(rs(Renderer::StateFillMode));
    return valid;
}

// ZiiNAN: Water's no-normal FVF still receives native ambient/emissive lighting at far NULL-texture draws.
bool ResolveWaterDiffuse(const Renderer::EffectVertex* input,uint32_t count,std::vector<Renderer::EffectVertex>& output)
{
    DWORD lighting=0; if(FAILED(DrawStateView().GetRenderState(Renderer::StateLighting,&lighting))) return false;
    if(!lighting) return true;
    Renderer::MaterialValues material{}; Math::Matrix world;
    if(FAILED(DrawStateView().GetMaterial(&material)) || FAILED(DrawStateView().GetTransform(Renderer::MatrixWorld,&world))) return false;
    DWORD ambient=0,colorVertex=0,ambientSource=0,emissiveSource=0,diffuseSource=0;
    for(auto entry:{std::pair{Renderer::StateAmbient,&ambient},{Renderer::StateColorVertex,&colorVertex},
        {Renderer::StateAmbientMaterialSource,&ambientSource},{Renderer::StateEmissiveMaterialSource,&emissiveSource},{Renderer::StateDiffuseMaterialSource,&diffuseSource}})
        if(FAILED(DrawStateView().GetRenderState(entry.first,entry.second))) return false;
    struct Light { Renderer::LightValues value; };
    std::vector<Light> lights;
    for(DWORD i=0;i<8;++i) {
        BOOL enabled=FALSE; if(FAILED(DrawStateView().GetLightEnable(i,&enabled)) || !enabled) continue;
        Renderer::LightValues light{}; if(FAILED(DrawStateView().GetLight(i,&light))) return false;
        if(light.Type<Renderer::LightPoint || light.Type>Renderer::LightDirectional) return false;
        lights.push_back({light});
    }
    output.assign(input,input+count);
    for(auto& vertex:output) {
        const Math::Color color(vertex.color);
        const auto source=[&](DWORD selector,const Math::Color& fallback) {
            // COLOR2 is absent from Water's FVF: the original pipeline falls back to the material, not black.
            if(!colorVertex || selector!=Renderer::MaterialColor1) return Math::Color(fallback.r,fallback.g,fallback.b,fallback.a);
            return color;
        };
        const auto a=source(ambientSource,material.Ambient),e=source(emissiveSource,material.Emissive),d=source(diffuseSource,material.Diffuse);
        Math::Color illumination(ambient);
        Math::Vector3 position(vertex.position.data()); Math::Vec3TransformCoord(&position,&position,&world);
        for(const auto& entry:lights) {
            const auto& light=entry.value; float attenuation=1;
            if(light.Type!=Renderer::LightDirectional) {
                Math::Vector3 delta=position-Math::Vector3(light.Position.x,light.Position.y,light.Position.z);
                const float distance=Math::Vec3Length(&delta);
                if(distance>light.Range) continue;
                const float denominator=light.Attenuation0+light.Attenuation1*distance+light.Attenuation2*distance*distance;
                attenuation=denominator>0 ? 1/denominator : 0;
                if(light.Type==Renderer::LightSpot) {
                    Math::Vec3Normalize(&delta,&delta);
                    const Math::Vector3 direction(light.Direction.x,light.Direction.y,light.Direction.z);
                    const float rho=Math::Vec3Dot(&delta,&direction),outer=std::cos(light.Phi*.5f),inner=std::cos(light.Theta*.5f);
                    attenuation*=rho<=outer ? 0 : (rho>=inner ? 1 : std::pow((rho-outer)/(inner-outer),light.Falloff));
                }
            }
            illumination.r+=light.Ambient.r*attenuation; illumination.g+=light.Ambient.g*attenuation; illumination.b+=light.Ambient.b*attenuation;
        }
        const auto clamp=[](float f) { return std::clamp(f,0.0f,1.0f); };
        vertex.color=Renderer::PackColor(clamp(e.r+a.r*illumination.r),clamp(e.g+a.g*illumination.g),clamp(e.b+a.b*illumination.b),clamp(d.a));
    }
    return true;
}
