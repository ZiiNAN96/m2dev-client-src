#pragma once
namespace Renderer
{
inline constexpr char sceneLightingShader[] = R"(
cbuffer SceneLightingConstants {
 float4 SceneSunDirection;
 float4 SceneSunColor;
 float4 SceneAmbientSky;
 float4 SceneAmbientGround;
};
float3 LightingNormal(float3 v,float3 fallback) {
 float scale=max(max(abs(v.x),abs(v.y)),abs(v.z));
 if(scale<1e-20)return fallback;
 v/=scale;return normalize(v);
}
float3 LightingToLinear(float3 c) {return lerp(c/12.92,pow(max((c+.055)/1.055,0),2.4),step(.04045,c));}
float3 LightingToSRGB(float3 c) {c=saturate(c);return lerp(12.92*c,1.055*pow(c,1/2.4)-.055,step(.0031308,c));}
// Row vectors: inverse-transpose uses cofactor rows; the determinant sign matters
// for mirrored transforms. A singular matrix uses a finite geometric fallback.
float3 LightingTransformNormal(float3 n,float3x3 m) {
 float scale=max(max(length(m[0]),length(m[1])),length(m[2]));
 if(scale<1e-12)return LightingNormal(n,float3(0,0,1));
 m/=scale;
 float3x3 cof=float3x3(cross(m[1],m[2]),cross(m[2],m[0]),cross(m[0],m[1]));
 float det=dot(m[0],cof[0]);
 if(abs(det)<1e-12)return LightingNormal(n,float3(0,0,1));
 return LightingNormal(mul(n,cof)*(det<0?-1:1),float3(0,0,1));
}
float3 SceneAmbient(float3 n) {return lerp(SceneAmbientGround.rgb,SceneAmbientSky.rgb,saturate(n.z*.5+.5));}
// Diffuse-only foundation for existing terrain/vegetation; same normalized sun,
// linear colors and hemisphere as the material BRDF, no new material model.
float3 SceneDiffuse(float3 base,float3 n) {
 return base*(SceneAmbient(n)+SceneSunColor.rgb*saturate(dot(n,SceneSunDirection.xyz))/3.14159265);
}
)";
}
