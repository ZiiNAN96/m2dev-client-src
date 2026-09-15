#pragma once
namespace Renderer
{
// Game mesh/card/palette adapter. BRDF, normal matrix and normal mapping are FX.
inline constexpr char modernMeshShader[]=R"(
#include "BasicStructures.fxh"
#include "PBR_Shading.fxh"
#include "VertexProcessing.fxh"
#if GDX_FORWARD
#define PCF_FILTER_SIZE 3
#define FILTER_ACROSS_CASCADES 1
#include "Shadows.fxh"
cbuffer Composite {CameraAttribs Camera;ShadowMapAttribs Shadows;float4 Options;float4 FogColor;float4 FogParameters;float4 DiskDirection;float4 DiskRadiance;};
Texture2DArray<float> ShadowMap;SamplerComparisonState ShadowSampler;Texture2D ScreenAO;
Texture2D Sky;SamplerState SkySampler;
#endif
cbuffer ModernObject {
 row_major float4x4 World;
 row_major float4x4 ViewProjection;
 float4 BaseColor;
 float4 Surface; // roughness, metallic, normal scale, AO strength
 float4 Emissive;
 uint4 Maps; // bitmask, roughness channel, metallic channel, AO channel
 float4 Alpha; // threshold, comparison, sidedness, determinant sign
 float4 CardRight; float4 CardForward; float4 CardUp; float4 Wind; float4 CardPitch;
 uint4 Card;
 row_major float4x4 CameraMaskMatrix;float4 LegacyAlpha;float4 LegacyTint;
};
cbuffer ModernLighting {float4 SunDirection;float4 SunColor;float4 AmbientColor;float4 CameraPosition;float4 EnvironmentColor;};
Texture2D PreintegratedBRDF;TextureCube IrradianceMap;SamplerState IBLSampler;
TextureCube SkyIrradiance;TextureCube SkyEnvironment;
#if GDX_SKIN
cbuffer SkinningPalette {row_major float4x4 Bones[256];};
#endif
Texture2D BaseMap;Texture2D NormalMap;Texture2D RoughnessMap;Texture2D MetallicMap;Texture2D AOMap;Texture2D EmissiveMap;
SamplerState MaterialSampler;
Texture2D CameraAlphaTexture;SamplerState CameraAlphaSampler;
struct ModernVertex {float4 position:SV_POSITION;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;float4 color:COLOR0;float4 tangent:TEXCOORD3;};
ModernVertex ModernVS(float3 position:ATTRIB0,float3 normal:ATTRIB1,float2 uv:ATTRIB2
#if GDX_SKIN
,uint4 weights:ATTRIB3,uint4 indices:ATTRIB4
#elif GDX_AUX
,float4 color:ATTRIB3,float2 uv1:ATTRIB4,float3 pivot:ATTRIB5,float flexibility:ATTRIB6,float3 pitchCos:ATTRIB7,float3 pitchSin:ATTRIB8
#endif
#if GDX_TANGENT
,float4 tangent:ATTRIB9
#endif
) {
 row_major float4x4 transform=World;
#if GDX_SKIN
 row_major float4x4 skin=0;
 [unroll]for(uint k=0;k<4;++k)if(weights[k]!=0)skin+=Bones[indices[k]]*(float(weights[k])*(1.0/255.0));
 transform=mul(skin,World);
#elif GDX_AUX
 float3 offset=position-pivot;
 float sway=sin(Wind.x*Wind.z+pivot.x*.013+pivot.y*.017)*Wind.y*flexibility;
 if(Card.x!=0) {
  float2 rocked=float2(offset.x*cos(sway)-offset.z*sin(sway),offset.x*sin(sway)+offset.z*cos(sway));
  offset.x=rocked.x;offset.z=rocked.y;
  if(Card.x==1)offset+=offset.z*(CardPitch.x*pitchCos+CardPitch.y*pitchSin);
  position=pivot+offset.x*CardRight.xyz+offset.y*CardForward.xyz+offset.z*CardUp.xyz;
  normal=normal.x*CardRight.xyz+normal.y*CardForward.xyz+normal.z*CardUp.xyz;
 } else position.xy+=sway*position.z*CardRight.xy;
#endif
 ModernVertex o;
 o.color=1;
#if GDX_AUX
 o.color=color;
#endif
 o.world=mul(float4(position,1),transform).xyz;
 float3x3 normalMatrix=(float3x3)transform;
 float determinant=dot(cross(normalMatrix[0],normalMatrix[1]),normalMatrix[2]);
 o.normal=abs(determinant)>1e-9?mul(normal,InverseTranspose3x3(normalMatrix)):float3(0,0,0);
 o.tangent=0;
#if GDX_TANGENT
 o.tangent=float4(mul(tangent.xyz,(float3x3)transform),tangent.w*(determinant<0?-1:1));
#endif
 o.position=mul(float4(o.world,1),ViewProjection);o.uv=uv;return o;
}
float4 SampleBase(ModernVertex i) {
 float4 base=BaseMap.Sample(MaterialSampler,i.uv)*BaseColor*i.color;
 if(LegacyAlpha.x==0)base.a*=LegacyAlpha.y;
 if(LegacyAlpha.x==2)base.a=LegacyAlpha.y;
 if(LegacyAlpha.x==3)base.a*=LegacyAlpha.z;
 if(LegacyAlpha.x==4)base.a=LegacyAlpha.z;
 if(LegacyAlpha.w!=0) {
  float mask=CameraAlphaTexture.Sample(CameraAlphaSampler,mul(float4(i.world,1),CameraMaskMatrix).xy).a;
  base.a=LegacyAlpha.w==2?base.a*mask:mask;
 }
 // Preserve the native 8-bit masked-material decision in both render passes.
 float tested=floor(saturate(base.a)*255+0.5);
 if(Alpha.y==1&&tested<Alpha.x)discard;
 if(Alpha.y==2&&tested<=Alpha.x)discard;
 return base;
}
#if GDX_SHADOW
void ModernPS(ModernVertex i) {SampleBase(i);}
#else
struct ModernOutput {float4 direct:SV_TARGET0;float4 indirect:SV_TARGET1;float4 emission:SV_TARGET2;float4 normal:SV_TARGET3;};
#if GDX_FORWARD
float4 ModernPS(ModernVertex i,bool front:SV_IsFrontFace):SV_TARGET {
#else
ModernOutput ModernPS(ModernVertex i,bool front:SV_IsFrontFace) {
#endif
 float4 base=SampleBase(i);
 // Color texture is legacy UNORM; authored glTF factors are already linear.
 float3 color=FastSRGBToLinear(BaseMap.Sample(MaterialSampler,i.uv).rgb)*BaseColor.rgb*i.color.rgb;
 bool face=Alpha.z!=0?front:true;
 PerturbNormalInfo normalInfo=GetPerturbNormalInfo(i.world,i.normal,face,CameraPosition.w);
 float3 N=normalInfo.Normal;
 if((Maps.x&2)!=0) {
  float3 normalTex=NormalMap.Sample(MaterialSampler,i.uv).xyz*2-1;normalTex.xy*=Surface.z;
  if(dot(i.tangent.xyz,i.tangent.xyz)>1e-8&&abs(i.tangent.w)>.5) {
   // Authored tangent basis, transformed with the same skin/world matrix.
   float3 T=normalize(i.tangent.xyz-N*dot(N,i.tangent.xyz))*normalInfo.Face;
   float3 B=cross(N,T)*i.tangent.w*normalInfo.Face;
   N=normalize(mul(normalTex,float3x3(T,B,N)));
  } else N=PerturbNormal(normalInfo,ddx(i.uv),ddy(i.uv),normalTex,true);
 }
 float roughness=Surface.x,metallic=Surface.y,ao=1;
 if((Maps.x&4)!=0)roughness*=RoughnessMap.Sample(MaterialSampler,i.uv)[Maps.y];
 if((Maps.x&8)!=0)metallic*=MetallicMap.Sample(MaterialSampler,i.uv)[Maps.z];
 if((Maps.x&16)!=0)ao=lerp(1,AOMap.Sample(MaterialSampler,i.uv)[Maps.w],Surface.w);
 SurfaceReflectanceInfo reflectance=GetSurfaceReflectanceMR(color,saturate(metallic),clamp(roughness,.045,1));
 float3 V=normalize(CameraPosition.xyz-i.world);
 ModernOutput o;
 o.direct=float4(ApplyDirectionalLightGGX(SunDirection.xyz,SunColor.rgb,reflectance,N,V),base.a);
 IBLSamplingInfo ibl=GetIBLSamplingInfo(reflectance,PreintegratedBRDF,IBLSampler,N,V);
 o.indirect=float4((GetLambertianIBL(reflectance,ibl,IrradianceMap,IBLSampler)*AmbientColor.rgb+
                   GetSpecularIBL_GGX(reflectance,ibl,EnvironmentColor.rgb)+
                   EnvironmentColor.w*(GetLambertianIBL(reflectance,ibl,SkyIrradiance,IBLSampler)+
                   GetSpecularIBL_GGX(reflectance,ibl,SkyEnvironment,IBLSampler,5.0)))*ao,base.a);
 float3 emission=Emissive.rgb;
 if((Maps.x&32)!=0)emission*=FastSRGBToLinear(EmissiveMap.Sample(MaterialSampler,i.uv).rgb);
 // Game hit/selection tints remain game inputs. Add is unshadowed display
 // contribution; Modulate colors all lighting contributions in linear space.
 float3 tint=FastSRGBToLinear(saturate(LegacyTint.rgb));
 if(LegacyTint.w==1)emission+=tint;
 if(LegacyTint.w==2){o.direct.rgb*=tint;o.indirect.rgb*=tint;emission*=tint;}
 o.emission=float4(emission,base.a);o.normal=float4(N,base.a);
#if GDX_FORWARD
 float shadow=1;
 if(Options.x!=0) {
  float3 light=mul(float4(i.world,1),Shadows.mWorldToLightView).xyz;
  shadow=FilterShadowMap(Shadows,ShadowMap,ShadowSampler,light,ddx(light),ddy(light),abs(mul(float4(i.world,1),Camera.mView).z)).fLightAmount;
 }
 float screenAO=Options.y!=0?ScreenAO.Load(int3(int2(i.position.xy),0)).r:1;
 float3 result=max(o.direct.rgb*shadow+o.indirect.rgb*screenAO+emission,0);
 if(FogParameters.w!=0) {
  float distance=length(i.world-CameraPosition.xyz);
  float progress=max(0,(distance-FogParameters.x)/max(1,FogParameters.y-FogParameters.x));
  float visibility=FogParameters.w==2?exp(-distance*FogParameters.z):exp(-3*progress*progress);
  if(Options.w==0)visibility=saturate(1-progress);
  float3 ray=normalize(i.world-CameraPosition.xyz);
  float3 sky=Sky.SampleLevel(SkySampler,float2(atan2(ray.y,ray.x)/(2*3.14159265359)+.5,1-saturate(ray.z)),0).rgb;
  sky=lerp(sky,FogColor.rgb,.12*pow(1-saturate(ray.z),4));
  result=lerp(sky,result,saturate(visibility));
 }
 return float4(result,base.a);
#else
 return o;
#endif
}
#endif
)";
}
