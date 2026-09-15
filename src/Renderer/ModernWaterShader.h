#pragma once
namespace Renderer
{
inline constexpr char modernWaterShader[]=R"(
#include "BasicStructures.fxh"
#include "ShaderUtilities.fxh"
#include "PBR_Shading.fxh"
#define PCF_FILTER_SIZE 3
#define FILTER_ACROSS_CASCADES 1
#include "Shadows.fxh"
cbuffer Water {
 CameraAttribs Camera;ShadowMapAttribs Shadows;row_major float4x4 World;
 float4 Wave;float4 Offset;float4 Material;float4 Absorption;float4 DeepColor;
 float4 Options;float4 SunDirection;float4 SunRadiance;float4 Horizon;
};
Texture2D NormalMap;SamplerState NormalSampler;
Texture2D Scene;Texture2D OpaqueDepth;Texture2D WaterDepth;Texture2D WaterNormal;
Texture2D Sky;SamplerState SkySampler;Texture2D Reflection;SamplerState SceneSampler;
Texture2DArray<float> ShadowMap;SamplerComparisonState ShadowSampler;
struct WaterVertex {float4 position:SV_POSITION;float3 world:WORLD;float coverage:COVERAGE;};
WaterVertex WaterVS(float3 position:ATTRIB0,float4 color:ATTRIB1) {
 WaterVertex o;float4 world=mul(float4(position,1),World);
 o.position=mul(world,Camera.mViewProj);o.world=world.xyz;o.coverage=color.a;return o;
}
struct WaterNormalOutput {float4 water:SV_TARGET0;float4 scene:SV_TARGET1;};
WaterNormalOutput WaterNormalPS(WaterVertex i) {
 float2 a=NormalMap.Sample(NormalSampler,i.world.xy*Wave.x+Offset.xy).xy*2-1;
 float2 b=NormalMap.Sample(NormalSampler,i.world.xy*Wave.y+Offset.zw).xy*2-1;
 float2 slope=(a+Wave.w*b)*Wave.z;
 WaterNormalOutput o;o.water=float4(normalize(float3(slope,1)),saturate(i.coverage));o.scene=o.water;return o;
}
float4 WaterScreenVS(uint id:SV_VertexID):SV_POSITION {return float4(id==2?3:-1,id==1?3:-1,0,1);}
float WaterRoughnessPS(float4 pixel:SV_POSITION):SV_TARGET {
 return WaterNormal.Load(int3(int2(pixel.xy),0)).a>0?Material.x:1;
}
float3 WorldPosition(float2 uv,float depth) {
 float4 world=mul(float4(TexUVToNormalizedDeviceXY(uv),DepthToNormalizedDeviceZ(depth),1),Camera.mViewProjInv);
 return world.xyz/world.w;
}
float4 WaterCompositePS(float4 pixel:SV_POSITION):SV_TARGET {
 int2 coord=int2(pixel.xy);float2 uv=pixel.xy*Camera.f4ViewportSize.zw;
 float4 normal=WaterNormal.Load(int3(coord,0));if(normal.a<=0)discard;
 float waterDepth=WaterDepth.Load(int3(coord,0)).r;
 float depth=OpaqueDepth.Load(int3(coord,0)).r;
 float3 world=WorldPosition(uv,waterDepth),under=WorldPosition(uv,depth);
 float thickness=clamp(world.z-under.z,0,Material.w);
 if(depth>=1)thickness=Material.w;
 float shore=smoothstep(0,Material.z,thickness)*normal.a;
 float3 N=normalize(normal.xyz),V=normalize(Camera.f4Position.xyz-world);
 float2 offset=mul(float4(N.xy,0,0),Camera.mView).xy*float2(1,-1)*Material.y*Camera.f4ViewportSize.zw;
 float2 refractUV=clamp(uv+offset*shore,Camera.f4ViewportSize.zw*.5,1-Camera.f4ViewportSize.zw*.5);
 int2 candidate=int2(refractUV*Camera.f4ViewportSize.xy);
 float candidateDepth=OpaqueDepth.Load(int3(candidate,0)).r;
 // Reject foreground silhouettes and offsets outside the water footprint.
 bool safe=candidateDepth>waterDepth+1e-6 && WaterNormal.Load(int3(candidate,0)).a>0;
 if(Options.x==0||!safe)refractUV=uv;
 float3 original=Scene.Load(int3(coord,0)).rgb;
 float3 transmitted=Scene.SampleLevel(SceneSampler,refractUV,0).rgb;
 if(Options.y!=0) {
  float3 transmittance=exp(-Absorption.rgb*thickness);
  transmitted=transmitted*transmittance+DeepColor.rgb*(1-transmittance);
 }
 float3 direction=reflect(-V,N);
 float2 skyUV=float2(atan2(direction.y,direction.x)/(2*3.14159265359)+.5,1-saturate(direction.z));
 float3 sky=Sky.SampleLevel(SkySampler,skyUV,0).rgb;
 sky=lerp(sky,Horizon.rgb,.12*pow(1-saturate(direction.z),4));
 float4 ssr=Options.z!=0?Reflection.Load(int3(coord,0)):float4(0,0,0,0);
 if(Absorption.w==1)return float4(ssr.rgb,1);
 if(Absorption.w==2)return float4(ssr.aaa,1);
 if(Absorption.w==3)return float4(ssr.rgb,1);
 if(Absorption.w==4)return float4(ssr.rrr,1);
 float3 reflected=lerp(sky,max(ssr.rgb,0),saturate(ssr.a));
 float fresnel=SchlickReflection(saturate(dot(N,V)),.02037318784,1.0);
 SurfaceReflectanceInfo surface=GetSurfaceReflectanceMR(float3(0,0,0),0,Material.x);
 surface.Reflectance0=float3(.02037318784,.02037318784,.02037318784);
 float3 specular=ApplyDirectionalLightGGX(SunDirection.xyz,SunRadiance.rgb,surface,N,V);
 float shadow=1;
 if(Options.w!=0) {
  float3 light=mul(float4(world,1),Shadows.mWorldToLightView).xyz;
  float cameraZ=abs(mul(float4(world,1),Camera.mView).z);
  shadow=FilterShadowMap(Shadows,ShadowMap,ShadowSampler,light,ddx(light),ddy(light),cameraZ).fLightAmount;
 }
 float3 water=transmitted*(1-fresnel)+reflected*fresnel+specular*shadow;
 return float4(lerp(original,water,shore),1);
}
)";
}
