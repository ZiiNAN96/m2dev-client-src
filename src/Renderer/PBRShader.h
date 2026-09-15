#pragma once
namespace Renderer
{
// Single metallic/roughness PS for rigid and GPU-skinned geometry. No IBL/HDR passes.
// glTF 2.0 channel/color-space contract; Cook-Torrance GGX + Smith + Schlick.
inline constexpr char pbrShader[] = R"(
cbuffer PBRConstants {
 row_major float4x4 PWorld; row_major float4x4 PView; row_major float4x4 PProjection;
 row_major float4x4 PNormal;
 float4 PAmbient; float4 PDirectional; float4 PLightDirection;
 float4 PFogColor; float4 PFogParameters;
 float4 PBaseColor; float4 PEmissive; float4 PFactors;
 float4 PUvRows[10];
 uint4 PFlags; uint4 PAlpha;
 float4 PFade;
 row_major float4x4 PCameraTransform;
};
Texture2D PBaseTexture; Texture2D PNormalTexture; Texture2D PMetalRoughTexture;
Texture2D POcclusionTexture; Texture2D PEmissiveTexture; Texture2D PCameraTexture;
SamplerState PMaterialSampler; SamplerState PCameraSampler;
struct POutput {float4 position:SV_POSITION;float3 eye:TEXCOORD0;float3 normal:TEXCOORD1;
 float4 tangent:TEXCOORD2;float2 uv:TEXCOORD3;float fog:TEXCOORD4;float2 cameraUV:TEXCOORD5;};
float3 SafeNormal(float3 v,float3 fallback) {float n=dot(v,v);return n>1e-16?v*rsqrt(n):fallback;}
float2 MaterialUV(uint slot,float2 uv) {return float2(dot(PUvRows[slot*2].xyz,float3(uv,1)),dot(PUvRows[slot*2+1].xyz,float3(uv,1)));}
float3 ToLinear(float3 c) {return lerp(c/12.92,pow(max((c+.055)/1.055,0),2.4),step(.04045,c));}
float3 ToSRGB(float3 c) {c=saturate(c);return lerp(12.92*c,1.055*pow(c,1/2.4)-.055,step(.0031308,c));}
POutput PBRVS(float3 position:ATTRIB0,float3 normal:ATTRIB1,float2 oldUV:ATTRIB2,float4 tangent:ATTRIB5,float2 uv:ATTRIB6) {
 POutput o;float4 eye=mul(mul(float4(position,1),PWorld),PView);o.eye=eye.xyz;
 o.position=mul(eye,PProjection);o.normal=mul(float4(normal,0),PNormal).xyz;
 o.tangent=float4(mul(mul(float4(tangent.xyz,0),PWorld),PView).xyz,tangent.w*(determinant((float3x3)PWorld)<0?-1:1));o.uv=uv;
 float d=(PFlags.z&256)!=0?length(eye.xyz):abs(eye.z);uint fogMode=PFlags.z&255;
 o.fog=1;if(fogMode==1)o.fog=exp(-PFogParameters.z*d);
 if(fogMode==2)o.fog=exp(-pow(PFogParameters.z*d,2));
 if(fogMode==3)o.fog=(PFogParameters.y-d)/max(1e-8,PFogParameters.y-PFogParameters.x);
 o.fog=saturate(o.fog);o.cameraUV=mul(eye,PCameraTransform).xy;return o;
}
float3 SurfaceNormal(POutput i,bool front) {
 float3 n=SafeNormal(i.normal,float3(0,0,1));
 if(!front)n=-n;
 [branch] if((PFlags.x&2)==0)return n;
 float2 uv=MaterialUV(1,i.uv);
 float3 mapped=PNormalTexture.Sample(PMaterialSampler,uv).xyz*2-1;
 // Quantized zero vectors (128,128,128) are invalid normal data.
 if(dot(mapped,mapped)<1e-4)mapped=float3(0,0,1);
 // Divide by a common positive scale before normalization to avoid overflow,
 // while preserving glTF's finite negative and greater-than-one normal scales.
 float divisor=max(abs(PFactors.z),1);
 mapped=float3(mapped.xy*(PFactors.z/divisor),mapped.z/divisor);
 mapped=SafeNormal(mapped,float3(0,0,1));
 float3 t,b;
 bool identity=all(PUvRows[2].xyz==float3(1,0,0))&&all(PUvRows[3].xyz==float3(0,1,0));
 if(abs(i.tangent.w)>.5&&identity) {
   t=SafeNormal(i.tangent.xyz-n*dot(n,i.tangent.xyz),float3(1,0,0));b=cross(n,t)*i.tangent.w;
 } else {
   // Bounded fallback for legacy overrides / missing offline tangents. Mirrored UVs keep their sign.
   float3 dx=ddx(i.eye),dy=ddy(i.eye);float2 tx=ddx(uv),ty=ddy(uv);
   float det=tx.x*ty.y-tx.y*ty.x;
   if(abs(det)<1e-10)return n;
   float3 rawT=(dx*ty.y-dy*tx.y)/det,rawB=(dy*tx.x-dx*ty.x)/det;
   t=SafeNormal(rawT-n*dot(n,rawT),float3(1,0,0));b=cross(n,t)*(dot(cross(n,t),rawB)<0?-1:1);
 }
 return SafeNormal(t*mapped.x+b*mapped.y+n*mapped.z,n);
}
float4 PBRPS(POutput i,bool front:SV_IsFrontFace):SV_TARGET {
 float4 base=PBaseTexture.Sample(PMaterialSampler,MaterialUV(0,i.uv));
 if((PFlags.y&1)==0)base.rgb=ToLinear(base.rgb);
 base*=PBaseColor;
 float alpha=base.a;
 if(PAlpha.x==0)alpha*=PFade.x;
 if(PAlpha.x==2)alpha=PFade.x;
 if(PAlpha.x==3)alpha*=PFade.y;
 if(PAlpha.x==4)alpha=PFade.y;
 [branch] if(PAlpha.w!=0)alpha*=PCameraTexture.Sample(PCameraSampler,i.cameraUV).a;
 float tested=floor(saturate(alpha)*255+.5);
 if(PAlpha.y==1&&tested<float(PAlpha.z))discard;
 if(PAlpha.y==2&&tested<=float(PAlpha.z))discard;
 float roughness=PFactors.x,metallic=PFactors.y,ao=1;
 [branch] if((PFlags.x&4)!=0){float4 mr=PMetalRoughTexture.Sample(PMaterialSampler,MaterialUV(2,i.uv));roughness*=mr.g;metallic*=mr.b;}
 roughness=clamp(roughness,.045,1);metallic=saturate(metallic);
 [branch] if((PFlags.x&8)!=0)ao=lerp(1,POcclusionTexture.Sample(PMaterialSampler,MaterialUV(3,i.uv)).r,PFactors.w);
 float3 emissive=PEmissive.rgb;
 [branch] if((PFlags.x&16)!=0){float3 e=PEmissiveTexture.Sample(PMaterialSampler,MaterialUV(4,i.uv)).rgb;if((PFlags.y&16)==0)e=ToLinear(e);emissive*=e;}
 float3 n=SurfaceNormal(i,front),v=SafeNormal(-i.eye,n),l=SafeNormal(PLightDirection.xyz,n),h=SafeNormal(v+l,n);
 float nv=max(dot(n,v),1e-5),nl=saturate(dot(n,l)),nh=saturate(dot(n,h)),vh=saturate(dot(v,h));
 float3 f0=lerp(float3(.04,.04,.04),base.rgb,metallic),f=f0+(1-f0)*pow(1-vh,5);
 float a=roughness*roughness,a2=a*a,d=nh*nh*(a2-1)+1;
 float distribution=a2/max(3.14159265*d*d,1e-8);
 float gv=2*nv/(nv+sqrt(a2+(1-a2)*nv*nv));
 float gl=2*nl/max(nl+sqrt(a2+(1-a2)*nl*nl),1e-5);
 float3 diffuse=(1-f)*(1-metallic)*base.rgb/3.14159265;
 float3 specular=distribution*gv*gl*f/max(4*nv*nl,1e-5);
 float3 direct=(diffuse+specular)*PDirectional.rgb*nl;
 // Existing scene ambient is a small approximation, not an environment/IBL system.
 float3 indirect=PAmbient.rgb*((1-f0)*(1-metallic)*base.rgb+f0)*ao;
 float3 color=direct+indirect+emissive;
 if(PFlags.w==1)color=base.rgb;
 if(PFlags.w==2)return float4(n*.5+.5,alpha);
 if(PFlags.w==3)return float4(roughness.xxx,alpha);
 if(PFlags.w==4)return float4(metallic.xxx,alpha);
 if(PFlags.w==5)return float4(ao.xxx,alpha);
 if(PFlags.w==6)color=emissive;
 color=ToSRGB(color);if(PFlags.w==0)color=lerp(PFogColor.rgb,color,i.fog);
 return float4(color,alpha);
}
)";
}
