#pragma once
namespace Renderer {
inline constexpr char shadowReceiverShader[]=R"(
cbuffer ShadowSceneConstants {
 row_major float4x4 ShadowWorldToClip[3];
 row_major float4x4 ShadowCameraView;
 float4 ShadowSplits; float4 ShadowSettings; float4 ShadowBias; float4 ShadowTexels;
};
Texture2DArray<float> SunShadowDepth;
SamplerComparisonState SunShadowSampler;
float SampleSunCascade(float3 world,float3 normal,uint cascade) {
 float3 p=mul(float4(world+normal*ShadowBias.z,1),ShadowWorldToClip[cascade]).xyz;
 if(any(abs(p.xy)>1)||p.z<=0||p.z>=1)return 1;
 float2 uv=p.xy*float2(.5,-.5)+.5;
 float slope=1-saturate(dot(normal,SceneSunDirection.xyz));
 float depth=p.z-ShadowBias.x*(1+slope*ShadowBias.y);
 float visibility=0;int r=int(ShadowSettings.z);
 [loop]for(int y=-r;y<=r;++y)[loop]for(int x=-r;x<=r;++x)
   visibility+=SunShadowDepth.SampleCmpLevelZero(SunShadowSampler,float3(uv+float2(x,y)*ShadowSettings.y,cascade),depth);
 return visibility/((2*r+1)*(2*r+1));
}
float SunVisibility(float3 world,float3 normal) {
 if(ShadowSettings.x<.5)return 1;
 float distance=abs(mul(float4(world,1),ShadowCameraView).z);
 uint count=uint(ShadowSettings.x),c=distance>ShadowSplits.x?1:0;
 if(count>2&&distance>ShadowSplits.y)c=2;c=min(c,count-1);
 float visibility=SampleSunCascade(world,normal,c);
 float start=c==0?0:ShadowSplits[c-1],end=ShadowSplits[c];
 if(c+1<count)visibility=lerp(visibility,SampleSunCascade(world,normal,c+1),saturate((distance-lerp(start,end,.9))/max((end-start)*.1,1)));
 float farDistance=ShadowSplits[count-1];return lerp(visibility,1,saturate((distance-farDistance*.9)/max(farDistance*.1,1)));
}
float3 CascadeColor(float3 world) {
 float d=abs(mul(float4(world,1),ShadowCameraView).z);
 return d<ShadowSplits.x?float3(1,.2,.2):d<ShadowSplits.y?float3(.2,1,.2):float3(.2,.2,1);
}
)";
inline constexpr char ambientDepthShader[]=R"(
cbuffer AmbientConstants {
 row_major float4x4 InvProjection;
 float4 AOSize; float4 AOParameters; float4 AOFade; uint4 AOModes;
};
Texture2D<float> SceneDepth; Texture2D<float4> SceneColor; Texture2D<float4> AmbientDelta;
Texture2D<float> AmbientRaw;
struct FullOutput {float4 position:SV_POSITION;float2 uv:TEXCOORD0;};
FullOutput FullVS(uint id:SV_VertexID) {FullOutput o;o.uv=float2((id<<1)&2,id&2);o.position=float4(o.uv*float2(2,-2)+float2(-1,1),0,1);return o;}
int2 SafePixel(int2 p){return clamp(p,int2(0,0),int2(AOSize.xy)-1);}
float3 Position(int2 pixel) {
 pixel=SafePixel(pixel);float depth=SceneDepth.Load(int3(pixel,0));
 float2 uv=(float2(pixel)+.5)/AOSize.xy;
 float4 p=mul(float4(uv*float2(2,-2)+float2(-1,1),depth,1),InvProjection);return p.xyz/p.w;
}
float3 Surface(int2 p) {
 float3 c=Position(p),l=Position(p-int2(1,0)),r=Position(p+int2(1,0)),u=Position(p-int2(0,1)),d=Position(p+int2(0,1));
 float3 dx=abs(l.z-c.z)<abs(r.z-c.z)?c-l:r-c,dy=abs(u.z-c.z)<abs(d.z-c.z)?c-u:d-c;
 float3 n=cross(dx,dy);n=dot(n,n)>1e-10?normalize(n):normalize(-c);return dot(n,-c)<0?-n:n;
}
float AmbientPS(FullOutput input):SV_TARGET {
 int2 pixel=SafePixel(int2(input.uv*AOSize.xy));float depth=SceneDepth.Load(int3(pixel,0));if(depth>=.999999)return 1;
 float3 p=Position(pixel),n=Surface(pixel);float distance=abs(p.z);
 float fade=1-saturate((distance-AOFade.x)/max(1,AOFade.y-AOFade.x));if(fade<=0)return 1;
 float radius=AOParameters.x;float pixelRadius=clamp(radius*AOParameters.z/max(distance,1),1,96);
 float occlusion=0;
 // Fixed angular slices and quadratic world-radius steps: stable without temporal AA.
 [loop]for(uint slice=0;slice<AOModes.x;++slice) {
   float angle=3.14159265*(slice+.5)/AOModes.x;float2 direction=float2(cos(angle),sin(angle));
   float horizon[2]={0,0};
   [unroll]for(uint side=0;side<2;++side)[loop]for(uint stepIndex=1;stepIndex<=AOModes.y;++stepIndex) {
     float t=float(stepIndex)/AOModes.y;int2 offset=int2(round(direction*(side?1:-1)*max(1,pixelRadius*t*t)));
     int2 q=pixel+offset;if(any(q<0)||any(q>=int2(AOSize.xy)))continue;
     if(SceneDepth.Load(int3(q,0))>=.999999)continue;
     float3 delta=Position(q)-p;float length2=dot(delta,delta);if(length2<.25||length2>radius*radius)continue;
     float elevation=saturate(dot(n,delta)*rsqrt(length2)-.08);
     horizon[side]=max(horizon[side],elevation*(1-length2/(radius*radius)));
   }
   occlusion+=(horizon[0]+horizon[1])*.5;
 }
 return 1-min(.35,occlusion/max(1,AOModes.x)*AOParameters.y*fade);
}
float FilterAO(int2 pixel) {
 float3 p=Position(pixel),n=Surface(pixel);float sum=0,weights=0;
 int2 low=int2(float2(pixel)*AOSize.zw/AOSize.xy);
 [unroll]for(int y=-1;y<=1;++y)[unroll]for(int x=-1;x<=1;++x) {
   int2 q=clamp(low+int2(x,y),int2(0,0),int2(AOSize.zw)-1);
   int2 full=SafePixel(int2((float2(q)+.5)*AOSize.xy/AOSize.zw));
   float3 other=Position(full);float weight=exp(-abs(other.z-p.z)/max(1,AOParameters.x*.08))*pow(saturate(dot(n,Surface(full))),8);
   weight*=1/(1+x*x+y*y);sum+=AmbientRaw.Load(int3(q,0))*weight;weights+=weight;
 }
 return weights>1e-6?sum/weights:1;
}
float3 Decode(float3 c){return lerp(c/12.92,pow(max((c+.055)/1.055,0),2.4),step(.04045,c));}
float3 Encode(float3 c){c=saturate(c);return lerp(12.92*c,1.055*pow(c,1/2.4)-.055,step(.0031308,c));}
float4 CompositePS(FullOutput input):SV_TARGET {
 int2 pixel=SafePixel(int2(input.position.xy));float ao=FilterAO(pixel);
 if(AOModes.z!=0)return float4(ao.xxx,1);
 float4 color=SceneColor.Load(int3(pixel,0));float3 ambient=AmbientDelta.Load(int3(pixel,0)).rgb;
 return float4(Encode(max(0,Decode(color.rgb)-ambient*(1-ao))),color.a);
}
)";
}
