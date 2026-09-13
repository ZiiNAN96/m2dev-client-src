#pragma once
namespace Renderer
{
// ZiiNAN: Diligent GPU skinning prototype
inline constexpr char gpuSkinningShader[] = R"(
cbuffer SkinningPalette { row_major float4x4 Bones[256]; };
void SkinVertex(float3 position, float3 normal, uint4 weights, uint4 indices,
                out float3 skinnedPosition, out float3 skinnedNormal) {
 precise float3 p=0; precise float3 n=0;
 [unroll] for(uint k=0;k<4;++k) {
   if(weights[k]!=0) {
     precise float weight=float(weights[k])*(1.0/255.0);
     p+=weight*mul(float4(position,1),Bones[indices[k]]).xyz;
     n+=weight*mul(float4(normal,0),Bones[indices[k]]).xyz;
   }
 }
 skinnedPosition=p; skinnedNormal=n;
}
)";
}
