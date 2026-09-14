#pragma once
#include "Math/Math.h"
#include <cstdint>
namespace Renderer {
// ZiiNAN: Removed final D3D9 compile-time dependency.
// CPU material vocabulary. Asset-facing numeric values are intentionally stable.
enum RenderStateKey : uint32_t {
    StateAlphaBlendEnable=27,
    StateAlphaFunc=25,
    StateAlphaRef=24,
    StateAlphaTestEnable=15,
    StateAmbient=139,
    StateAmbientMaterialSource=147,
    StateBlendOp=171,
    StateBlendOpAlpha=209,
    StateClipping=136,
    StateClipPlaneEnable=152,
    StateColorVertex=141,
    StateColorWriteEnable=168,
    StateCullMode=22,
    StateDestBlend=20,
    StateDestBlendAlpha=208,
    StateDiffuseMaterialSource=145,
    StateDitherEnable=26,
    StateEmissiveMaterialSource=148,
    StateFillMode=8,
    StateFogColor=34,
    StateFogDensity=38,
    StateFogEnable=28,
    StateFogEnd=37,
    StateFogStart=36,
    StateFogTableMode=35,
    StateFogVertexMode=140,
    StateIndexedVertexBlendEnable=167,
    StateLastPixel=16,
    StateLighting=137,
    StateLocalViewer=142,
    StateMultisampleAntialias=161,
    StateMultisampleMask=162,
    StateNormalizeNormals=143,
    StatePatchEdgeStyle=163,
    StateRangeFogEnable=48,
    StateScissorTestEnable=174,
    StateSeparateAlphaBlendEnable=206,
    StateShadeMode=9,
    StateSpecularEnable=29,
    StateSpecularMaterialSource=146,
    StateSrcBlend=19,
    StateSrcBlendAlpha=207,
    StateStencilEnable=52,
    StateStencilWriteMask=59,
    StateTextureFactor=60,
    StateVertexBlend=151,
    StateWrap0=128,
    StateWrap1=129,
    StateWrap2=130,
    StateWrap3=131,
    StateWrap4=132,
    StateWrap5=133,
    StateWrap6=134,
    StateWrap7=135,
    StateZEnable=7,
    StateZFunc=23,
    StateZWriteEnable=14
};
enum SamplerStateKey : uint32_t {
    SamplerAddressU=1,
    SamplerAddressV=2,
    SamplerAddressW=3,
    SamplerBorderColor=4,
    SamplerMagFilter=5,
    SamplerMaxAnisotropy=10,
    SamplerMaxMipLevel=9,
    SamplerMinFilter=6,
    SamplerMipFilter=7,
    SamplerMipMapLodBias=8
};
enum TextureStageKey : uint32_t {
    StageAlphaArg0=27,
    StageAlphaArg1=5,
    StageAlphaArg2=6,
    StageAlphaOp=4,
    StageColorArg0=26,
    StageColorArg1=2,
    StageColorArg2=3,
    StageColorOp=1,
    StageResultArg=28,
    StageTciCameraSpacePosition=0x00020000,
    StageTciCameraSpaceReflectionVector=0x00030000,
    StageTexCoordIndex=11,
    StageTextureTransformFlags=24
};
enum BlendFactor : uint32_t {
    BlendInvDestColor=10,
    BlendInvSrcAlpha=6,
    BlendInvSrcColor=4,
    BlendOne=2,
    BlendSrcAlpha=5,
    BlendSrcColor=3,
    BlendZero=1
};
enum BlendOperation : uint32_t {
    BlendOpAdd=1
};
enum CompareFunction : uint32_t {
    CompareGreater=5,
    CompareGreaterEqual=7,
    CompareLess=2,
    CompareLessEqual=4,
    CompareNotEqual=6
};
enum FaceCulling : uint32_t {
    CullCcw=3,
    CullCw=2,
    CullNone=1
};
enum FillMode : uint32_t {
    FillPoint=1,
    FillSolid=3,
    FillWireframe=2
};
enum FogMode : uint32_t {
    FogExp=1,
    FogLinear=3,
    FogNone=0
};
enum LightType : uint32_t {
    LightDirectional=3,
    LightPoint=1,
    LightSpot=2
};
enum MaterialSource : uint32_t {
    MaterialColor1=1,
    MaterialMaterial=0
};
enum TextureArgument : uint32_t {
    ArgCurrent=0x00000001,
    ArgDiffuse=0x00000000,
    ArgTexture=0x00000002,
    ArgTFactor=0x00000003
};
enum TextureOperation : uint32_t {
    TextureOpAdd=7,
    TextureOpBlendDiffuseAlpha=12,
    TextureOpDisable=1,
    TextureOpModulate=4,
    TextureOpModulateAlphaAddColor=18,
    TextureOpModulateInvAlphaAddColor=20,
    TextureOpSelectArg1=2,
    TextureOpSelectArg2=3
};
enum TextureAddress : uint32_t {
    AddressMirror=2,
    AddressBorder=4,
    AddressClamp=3,
    AddressWrap=1
};
enum TextureFilter : uint32_t {
    FilterAnisotropic=3,
    FilterLinear=2,
    FilterNone=0,
    FilterPoint=1
};
enum TextureTransform : uint32_t {
    TexTransformCount2=2,
    TexTransformDisable=0
};
enum ShadeMode : uint32_t {
    ShadeFlat=1,
    ShadeGouraud=2
};
enum PrimitiveTopology : uint32_t {
    TopologyTriangleList=4,
    TopologyTriangleStrip=5
};
enum ColorWriteMask : uint32_t {
    WriteBlue=4,
    WriteGreen=2,
    WriteRed=1
};
enum PatchEdge : uint32_t {
    PatchEdgeContinuous=1
};
enum VertexBlend : uint32_t {
    VertexBlendDisable=0
};

struct MaterialValues { Math::Color Diffuse,Ambient,Specular,Emissive; float Power; };
struct LightValues {
    LightType Type; Math::Color Diffuse,Specular,Ambient; Math::Vector3 Position,Direction;
    float Range,Falloff,Attenuation0,Attenuation1,Attenuation2,Theta,Phi;
};
enum class IndexFormat { UInt16,UInt32 };
enum VertexLayout : uint32_t { VertexPosition=2,VertexNormal=16,VertexColor=64,VertexTex1=256,VertexTex2=512 };
inline uint32_t VertexStride(uint32_t layout) {
    return (layout&VertexPosition ? 12u:0u)+(layout&VertexNormal?12u:0u)+(layout&VertexColor?4u:0u)+
        ((layout>>8)&15u)*8u;
}
inline uint32_t PackColor(float r,float g,float b,float a) {
    return ((uint32_t(a*255)&255)<<24)|((uint32_t(r*255)&255)<<16)|((uint32_t(g*255)&255)<<8)|(uint32_t(b*255)&255);
}
}
