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
cbuffer Composite {CameraAttribs Camera;ShadowMapAttribs Shadows;float4 Options;float4 HorizonColor;float4 DiskDirection;float4 DiskRadiance;};
Texture2DArray<float> ShadowMap;SamplerComparisonState ShadowSampler;Texture2D ScreenAO;
#endif
cbuffer ModernObject {
 row_major float4x4 World;
 row_major float4x4 ViewProjection;
 float4 BaseColor;
 float4 Surface; // roughness, metallic, normal scale, AO strength
 float4 Emissive; // xyz emission, w explicit PBR material
 uint4 Maps; // bitmask, roughness channel, metallic channel, AO channel
 float4 Alpha; // threshold, comparison, sidedness, determinant sign
 float4 CardRight; float4 CardForward; float4 CardUp; float4 Wind; float4 CardPitch;
 uint4 Card;
 row_major float4x4 CameraMaskMatrix;float4 LegacyAlpha;float4 LegacyTint;
 row_major float4x4 LegacyView;
 float4 WorldWind;float4 BranchWind;float4 Foliage;
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
struct ModernVertex {float4 position:SV_POSITION;float3 world:TEXCOORD0;float3 normal:TEXCOORD1;float2 uv:TEXCOORD2;float4 color:COLOR0;float4 tangent:TEXCOORD3;float2 shimmerUV:TEXCOORD4;nointerpolation float3 lod:TEXCOORD5;};
ModernVertex ModernVS(float3 position:ATTRIB0,float3 normal:ATTRIB1,float2 uv:ATTRIB2
#if GDX_SKIN
,uint4 weights:ATTRIB3,uint4 indices:ATTRIB4
#elif GDX_AUX
,float4 color:ATTRIB3,float2 uv1:ATTRIB4,float3 pivot:ATTRIB5,float flexibility:ATTRIB6,float3 pitchCos:ATTRIB7,float3 pitchSin:ATTRIB8
#endif
#if GDX_TANGENT
,float4 tangent:ATTRIB9
#endif
#if H2_INSTANCED
,float4 instance0:ATTRIB10,float4 instance1:ATTRIB11,float4 instance2:ATTRIB12,float4 instance3:ATTRIB13,float4 instanceParameters:ATTRIB14
#endif
) {
 row_major float4x4 transform=World;
 float phase=0;float3 lod=float3(0,Alpha.x,1);
#if H2_INSTANCED
 transform=float4x4(instance0,instance1,instance2,instance3);phase=instanceParameters.x;lod=instanceParameters.yzw;
#endif
 float rootFraction=saturate((position.z-BranchWind.z)/max(WorldWind.w,1));
#if GDX_SKIN
 row_major float4x4 skin=0;
 [unroll]for(uint k=0;k<4;++k)if(weights[k]!=0)skin+=Bones[indices[k]]*(float(weights[k])*(1.0/255.0));
 transform=mul(skin,World);
#elif GDX_AUX
 float3 offset=position-pivot;
 float sway=sin((Wind.x+phase)*Wind.z+pivot.x*.013+pivot.y*.017)*Wind.y*flexibility;
 if(Card.y!=0)sway=sin(Wind.x*Wind.z*5+phase+pivot.x*.013+pivot.y*.017)*Wind.y*flexibility;
 if(Card.x!=0) {
  float3 right=CardRight.xyz,forward=CardForward.xyz,up=CardUp.xyz;
  if(Card.y!=0) {
   right=normalize(mul(right,transpose((float3x3)transform)));
   forward=normalize(mul(forward,transpose((float3x3)transform)));
   up=normalize(mul(up,transpose((float3x3)transform)));
  }
  float2 rocked=float2(offset.x*cos(sway)-offset.z*sin(sway),offset.x*sin(sway)+offset.z*cos(sway));
  offset.x=rocked.x;offset.z=rocked.y;
  if(Card.x==1)offset+=offset.z*(CardPitch.x*pitchCos+CardPitch.y*pitchSin);
  position=pivot+offset.x*right+offset.y*forward+offset.z*up;
  normal=normal.x*right+normal.y*forward+normal.z*up;
 } else if(Card.y==0)position.xy+=sway*position.z*CardRight.xy;
 else position.xy+=WorldWind.xy*sway*WorldWind.w*.04*rootFraction;
#endif
 ModernVertex o;
 o.lod=lod;
 o.color=1;
#if GDX_AUX
 o.color=color;
#endif
 o.world=mul(float4(position,1),transform).xyz;
 if(Card.y!=0) {
  float scale=length(transform[2].xyz);
  float bend=sin(Wind.x*Wind.z*.65+phase)*BranchWind.x*WorldWind.w*scale*rootFraction*rootFraction;
  o.world.xy+=WorldWind.xy*bend;
 }
 float3x3 normalMatrix=(float3x3)transform;
 float determinant=dot(cross(normalMatrix[0],normalMatrix[1]),normalMatrix[2]);
 o.normal=abs(determinant)>1e-9?mul(normal,InverseTranspose3x3(normalMatrix)):float3(0,0,0);
 o.tangent=0;
 o.shimmerUV=0;
 if(LegacyTint.w==3) {
  // Original item sphere-map coordinates and animated texture transform.
  // This authored game effect is independent of the material's PBR model.
  float3 eye=mul(float4(o.world,1),LegacyView).xyz;
  float3 eyeNormal=normalize(mul(o.normal,(float3x3)LegacyView));
  o.shimmerUV=mul(float4(reflect(normalize(eye),eyeNormal),1),CameraMaskMatrix).xy;
 }
#if GDX_TANGENT
 o.tangent=float4(mul(tangent.xyz,(float3x3)transform),tangent.w*(determinant<0?-1:1));
#endif
 o.position=mul(float4(o.world,1),ViewProjection);o.uv=uv;
 if(Card.x==2&&WorldWind.z>1) {
  float angle=atan2(CardRight.y,CardRight.x)-atan2(transform[0].y,transform[0].x);
  float view=fmod(floor(angle*(WorldWind.z/6.28318530718)+.5)+WorldWind.z*2,WorldWind.z);
  o.uv.x=(clamp(uv.x,.002,.998)+view)/WorldWind.z;
 }
 return o;
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
 if(Alpha.y==1&&tested<i.lod.y)discard;
 if(Alpha.y==2&&tested<=i.lod.y)discard;
#if H2_INSTANCED
 // Complementary ordered coverage for the two LODs, shared by color/shadows.
 const uint bayer[16]={0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
 uint2 pixel=uint2(i.position.xy)&3;float coverage=(bayer[pixel.y*4+pixel.x]+.5)/16.0;
 if(coverage>=i.lod.z)discard;
 if(i.lod.x>=0){if(coverage<i.lod.x)discard;}else if(coverage>=-i.lod.x)discard;
#endif
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
 bool pbr=Emissive.w!=0;
 if(pbr&&(Maps.x&2)!=0) {
  float3 normalTex=NormalMap.Sample(MaterialSampler,i.uv).xyz*2-1;normalTex.xy*=Surface.z;
  if(dot(i.tangent.xyz,i.tangent.xyz)>1e-8&&abs(i.tangent.w)>.5) {
   // Authored tangent basis, transformed with the same skin/world matrix.
   float3 T=normalize(i.tangent.xyz-N*dot(N,i.tangent.xyz))*normalInfo.Face;
   float3 B=cross(N,T)*i.tangent.w*normalInfo.Face;
   N=normalize(mul(normalTex,float3x3(T,B,N)));
  } else N=PerturbNormal(normalInfo,ddx(i.uv),ddy(i.uv),normalTex,true);
 }
 ModernOutput o;
 if(pbr) {
 float roughness=Surface.x,metallic=Surface.y,ao=1;
 if((Maps.x&4)!=0)roughness*=RoughnessMap.Sample(MaterialSampler,i.uv)[Maps.y];
 if((Maps.x&8)!=0)metallic*=MetallicMap.Sample(MaterialSampler,i.uv)[Maps.z];
 if((Maps.x&16)!=0)ao=lerp(1,AOMap.Sample(MaterialSampler,i.uv)[Maps.w],Surface.w);
 SurfaceReflectanceInfo reflectance=GetSurfaceReflectanceMR(color,saturate(metallic),clamp(roughness,.045,1));
 float3 V=normalize(CameraPosition.xyz-i.world);
 o.direct=float4(ApplyDirectionalLightGGX(SunDirection.xyz,SunColor.rgb,reflectance,N,V),base.a);
 IBLSamplingInfo ibl=GetIBLSamplingInfo(reflectance,PreintegratedBRDF,IBLSampler,N,V);
 o.indirect=float4((GetLambertianIBL(reflectance,ibl,IrradianceMap,IBLSampler)*AmbientColor.rgb+
                   GetSpecularIBL_GGX(reflectance,ibl,EnvironmentColor.rgb)+
                   EnvironmentColor.w*(GetLambertianIBL(reflectance,ibl,SkyIrradiance,IBLSampler)+
                   GetSpecularIBL_GGX(reflectance,ibl,SkyEnvironment,IBLSampler,5.0)))*ao,base.a);
 } else {
  // Legacy diffuse has no authored BRDF. Keep its chroma and use the shared
  // irradiance (map sunlight was converted by pi), without invented specular,
  // metalness or reflected sky. Shadows/AO still affect their own light terms.
  o.direct=float4(color*SunColor.rgb*(saturate(dot(N,-SunDirection.xyz))/3.14159265359),base.a);
 o.indirect=float4(color*AmbientColor.rgb,base.a);
 }
 // Thin foliage uses the same shared sun radiance and shadow term. Tint is
 // multiplied by authored albedo; transmission is never emission or bloom.
 if(Foliage.w>0) {
  float back=saturate(dot(-N,-SunDirection.xyz));
  o.direct.rgb+=color*Foliage.rgb*SunColor.rgb*(back*Foliage.w/3.14159265359);
 }
 float3 emission=Emissive.rgb;
 if((Maps.x&32)!=0)emission*=FastSRGBToLinear(EmissiveMap.Sample(MaterialSampler,i.uv).rgb);
 // Game hit/selection tints remain game inputs. Add is unshadowed display
 // contribution; Modulate colors all lighting contributions in linear space.
 float3 tint=FastSRGBToLinear(saturate(LegacyTint.rgb));
 if(LegacyTint.w==1)emission+=tint;
 if(LegacyTint.w==2){o.direct.rgb*=tint;o.indirect.rgb*=tint;emission*=tint;}
 // Keep the existing item texture/alpha/specular-power mask. No synthetic
 // metallic or white BRDF term is introduced for ordinary legacy materials.
 if(LegacyTint.w==3)emission+=base.a*FastSRGBToLinear(CameraAlphaTexture.Sample(CameraAlphaSampler,i.shimmerUV).rgb);
 o.emission=float4(emission,base.a);o.normal=float4(N,base.a);
#if GDX_FORWARD
 float shadow=1;
 if(Options.x!=0) {
  float3 light=mul(float4(i.world,1),Shadows.mWorldToLightView).xyz;
  shadow=FilterShadowMap(Shadows,ShadowMap,ShadowSampler,light,ddx(light),ddy(light),abs(mul(float4(i.world,1),Camera.mView).z)).fLightAmount;
 }
 float screenAO=Options.y!=0?ScreenAO.Load(int3(int2(i.position.xy),0)).r:1;
 float3 result=max(o.direct.rgb*shadow+o.indirect.rgb*screenAO+emission,0);
 return float4(result,base.a);
#else
 return o;
#endif
}
#endif
)";
}
