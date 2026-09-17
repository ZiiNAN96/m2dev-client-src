#pragma once
namespace Renderer
{
// Authored terrain diffuse and splat coverage, lit without invented PBR data.
inline constexpr char modernTerrainShader[]=R"(
#include "SRGBUtilities.fxh"
cbuffer ModernTerrain {
 row_major float4x4 World;row_major float4x4 View;row_major float4x4 Projection;
 row_major float4x4 ColorTransform;row_major float4x4 AlphaTransform;
 float4 TextureFactor;float4 FogColor;float4 FogParameters;uint4 Modes;
};
cbuffer ModernLighting {float4 SunDirection;float4 SunColor;float4 AmbientColor;float4 CameraPosition;float4 EnvironmentColor;};
Texture2D ColorTexture;Texture2D AlphaTexture;SamplerState ColorSampler;SamplerState AlphaSampler;
struct TerrainOutput {float4 position:SV_POSITION;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 colorUV:TEXCOORD2;float2 alphaUV:TEXCOORD3;float4 diffuse:COLOR0;};
TerrainOutput TerrainVS(float3 position:ATTRIB0,float3 normal:ATTRIB1
#if !GDX_SHADOW
,float4 diffuse:ATTRIB2,float fog:ATTRIB3,float2 colorUV:ATTRIB4,float2 alphaUV:ATTRIB5
#endif
) {
 TerrainOutput o;o.world=mul(float4(position,1),World).xyz;
 o.normal=mul(normal,(float3x3)World);
 float4 camera=mul(float4(o.world,1),View);o.position=mul(camera,Projection);
#if !GDX_SHADOW
 o.colorUV=(Modes.x&8)!=0?colorUV:mul(camera,ColorTransform).xy;
 o.alphaUV=(Modes.x&8)!=0?alphaUV:mul(camera,AlphaTransform).xy;o.diffuse=diffuse;
#else
 o.colorUV=o.alphaUV=0;o.diffuse=1;
#endif
 return o;
}
#if GDX_SHADOW
void TerrainPS(TerrainOutput i) {}
#else
struct TerrainTargets {float4 direct:SV_TARGET0;float4 indirect:SV_TARGET1;float4 emission:SV_TARGET2;float4 normal:SV_TARGET3;};
TerrainTargets TerrainPS(TerrainOutput i) {
 float4 sampled=GDX_SOLID?TextureFactor:ColorTexture.Sample(ColorSampler,i.colorUV);float alpha=sampled.a;
 if(Modes.y==1)alpha=AlphaTexture.Sample(AlphaSampler,i.alphaUV).r;
 if(Modes.y==2)alpha=i.diffuse.a;
 if(Modes.y==3)alpha*=i.diffuse.a;
 if(Modes.w!=0&&alpha<=float(Modes.w-1)/255.0)discard;
 float3 color=FastSRGBToLinear(sampled.rgb);
 // STP diffuse RGB already contains CPU sunlight. Modern evaluates sunlight
 // once on the GPU; diffuse alpha still selects the existing splat layers.
 // BlendDiffuseAlpha in STP was the legacy fog blend, not a material layer.
 // Modern keeps the original texture; texture masks still select the splats.
 float3 N=normalize(i.normal);
 TerrainTargets o;o.direct=float4(color*SunColor.rgb*(saturate(dot(N,-SunDirection.xyz))/3.14159265359),alpha);
 o.indirect=float4(AmbientColor.rgb*color,alpha);
 o.emission=float4(0,0,0,alpha);o.normal=float4(N,alpha);
#if GDX_SOLID
 // Existing far-terrain solid fog fill is already an environment color.
 o.direct.rgb=o.indirect.rgb=0;o.emission.rgb=color;
#endif
 return o;
}
#endif
)";
}
