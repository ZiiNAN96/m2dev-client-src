#include "ShadowAmbient.h"
#include <algorithm>
#include <cmath>

namespace Graphics {
namespace {
float Finite(float v,float fallback,float lo,float hi){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;}
Vector3 Add(Vector3 a,Vector3 b){for(unsigned i=0;i<3;++i)a[i]+=b[i];return a;}
Vector3 Scale(Vector3 v,float s){for(auto& x:v)x*=s;return v;}
float Dot(Vector3 a,Vector3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
Vector3 Cross(Vector3 a,Vector3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
bool Normalize(Vector3& v){const float n=std::hypot(v[0],v[1],v[2]);if(!std::isfinite(n)||n<1e-8f)return false;v=Scale(v,1/n);return true;}
}
ShadowQualityConfig ValidateShadowConfig(ShadowQualityConfig s){
    if(s.cascades>3)s.cascades=0;
    if(s.resolution!=1024&&s.resolution!=2048)s.resolution=1024;
    s.filterRadius=std::clamp(s.filterRadius,1u,2u);
    s.distance=Finite(s.distance,4000,500,12000);s.splitLambda=Finite(s.splitLambda,.65f,0,1);
    s.casterExtension=Finite(s.casterExtension,2500,100,4000);
    s.depthBias=Finite(s.depthBias,.00015f,0,.001f);s.slopeBias=Finite(s.slopeBias,1.25f,0,3);
    s.normalOffset=Finite(s.normalOffset,.6f,0,2);return s;
}
AmbientDepthConfig ValidateAmbientConfig(AmbientDepthConfig a){
    if(a.quality>2)a.quality=0;a.divisor=a.quality==1?2:1;
    a.directions=a.quality==2?6:4;a.steps=a.quality==2?6:4;
    a.radius=Finite(a.radius,60,5,150);a.intensity=Finite(a.intensity,.45f,0,.7f);
    a.fadeStart=Finite(a.fadeStart,2000,100,6000);a.fadeEnd=Finite(a.fadeEnd,5000,a.fadeStart+100,8000);return a;
}
ShadowQualityConfig ResolveShadowQuality(unsigned level,float viewDistance){
    ShadowQualityConfig s;
    // Six persisted G0 values: LegacySolo maps to the low Modern tier.
    constexpr unsigned counts[]{0,1,1,2,3,3};
    constexpr float distances[]{4000,3000,3000,5000,7500,9000};
    if(level>5)level=0;s.cascades=counts[level];s.distance=std::min(distances[level],Finite(viewDistance,25600,500,38400));
    s.resolution=level>=3?2048:1024;s.filterRadius=level==5?2:1;return s;
}
AmbientDepthConfig AmbientQuality(unsigned level){AmbientDepthConfig a;a.quality=level;return ValidateAmbientConfig(a);}
Matrix4 Multiply(const Matrix4& a,const Matrix4& b){Matrix4 r{};for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j)for(unsigned k=0;k<4;++k)r[i*4+j]+=a[i*4+k]*b[k*4+j];return r;}
bool Inverse(const Matrix4& m,Matrix4& out){
    double a[4][8]{};for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){if(!std::isfinite(m[i*4+j]))return false;a[i][j]=m[i*4+j];a[i][j+4]=i==j;}
    for(unsigned c=0;c<4;++c){unsigned p=c;for(unsigned r=c+1;r<4;++r)if(std::abs(a[r][c])>std::abs(a[p][c]))p=r;
        if(std::abs(a[p][c])<1e-12)return false;for(unsigned j=0;j<8;++j)std::swap(a[c][j],a[p][j]);
        const double d=a[c][c];for(auto& x:a[c])x/=d;
        for(unsigned r=0;r<4;++r)if(r!=c){const double f=a[r][c];for(unsigned j=0;j<8;++j)a[r][j]-=f*a[c][j];}}
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j){out[i*4+j]=float(a[i][j+4]);if(!std::isfinite(out[i*4+j]))return false;}return true;
}
Vector3 TransformPoint(const Vector3& p,const Matrix4& m){
    std::array<float,4> r{};for(unsigned j=0;j<4;++j)r[j]=p[0]*m[j]+p[1]*m[4+j]+p[2]*m[8+j]+m[12+j];
    const float w=std::abs(r[3])>1e-12f?r[3]:1;return {r[0]/w,r[1]/w,r[2]/w};
}
std::array<float,3> CascadeSplits(float nearD,float farD,unsigned count,float lambda){
    std::array<float,3> result{};if(!count||count>3||!std::isfinite(nearD)||!std::isfinite(farD)||nearD<=0||farD<=nearD)return result;
    lambda=Finite(lambda,.65f,0,1);for(unsigned i=0;i<count;++i){float t=float(i+1)/count;result[i]=(1-lambda)*(nearD+(farD-nearD)*t)+lambda*nearD*std::pow(farD/nearD,t);}result[count-1]=farD;return result;
}
ShadowCascades BuildCascades(const Matrix4& view,const Matrix4& projection,const Vector3& sun,ShadowQualityConfig config){
    ShadowCascades result;config=ValidateShadowConfig(config);Vector3 backward=sun;
    Matrix4 inverseView,inverseProjection;if(!config.cascades||!Normalize(backward)||!Inverse(view,inverseView)||!Inverse(projection,inverseProjection))return result;
    const auto nearPoint=TransformPoint({0,0,0},inverseProjection),farPoint=TransformPoint({0,0,1},inverseProjection);
    const float nearD=std::abs(nearPoint[2]),cameraFar=std::abs(farPoint[2]),farD=std::min(config.distance,cameraFar);
    if(!std::isfinite(farD)||nearD<.001f||farD<=nearD)return result;
    const auto splits=CascadeSplits(nearD,farD,config.cascades,config.splitLambda);
    // Match the main right-handed camera: visible geometry has negative view Z.
    // This preserves the existing material front-face policy in both passes.
    Vector3 right=Cross(std::abs(backward[2])>.95f?Vector3{0,1,0}:Vector3{0,0,1},backward);Normalize(right);const auto up=Cross(backward,right);
    float previous=nearD;
    for(unsigned c=0;c<config.cascades;++c){auto& cascade=result.cascade[c];std::array<Vector3,8> points;unsigned k=0;
        // Small overlap permits a receiver blend across each split.
        const float start=c?std::max(nearD,previous-(previous-(c>1?splits[c-2]:nearD))*.1f):nearD;
        Vector3 center{};
        for(float d:{start,splits[c]})for(float y:{-1.f,1.f})for(float x:{-1.f,1.f}){
            const auto n=TransformPoint({x,y,0},inverseProjection),f=TransformPoint({x,y,1},inverseProjection);
            const float t=(d-nearD)/(cameraFar-nearD);const auto p=TransformPoint(Add(Scale(n,1-t),Scale(f,t)),inverseView);points[k++]=p;center=Add(center,Scale(p,1.f/8));}
        float radius=0;for(const auto& p:points){const auto delta=Add(p,Scale(center,-1));radius=std::max(radius,std::sqrt(Dot(delta,delta)));}
        radius=std::ceil(radius/16)*16;radius*=float(config.resolution)/float(config.resolution-4);
        const float texel=2*radius/config.resolution;
        center=Add(center,Scale(right,std::round(Dot(center,right)/texel)*texel-Dot(center,right)));
        center=Add(center,Scale(up,std::round(Dot(center,up)/texel)*texel-Dot(center,up)));
        const auto origin=Add(center,Scale(backward,radius+config.casterExtension));
        cascade.view={right[0],up[0],backward[0],0,right[1],up[1],backward[1],0,right[2],up[2],backward[2],0,-Dot(origin,right),-Dot(origin,up),-Dot(origin,backward),1};
        cascade.projection={1/radius,0,0,0,0,1/radius,0,0,0,0,-1/(2*radius+config.casterExtension),0,0,0,0,1};
        cascade.worldToClip=Multiply(cascade.view,cascade.projection);cascade.nearDistance=previous;cascade.farDistance=splits[c];cascade.radius=radius;cascade.texelSize=texel;previous=splits[c];
    }result.count=config.cascades;return result;
}
bool IntersectsCascade(const ShadowCascade& c,const Vector3& center,float radius){
    if(!std::isfinite(radius)||radius<0)return false;const auto p=TransformPoint(center,c.worldToClip);
    const float xy=radius/c.radius,z=radius*std::abs(c.projection[10]);return p[0]>=-1-xy&&p[0]<=1+xy&&p[1]>=-1-xy&&p[1]<=1+xy&&p[2]>=-z&&p[2]<=1+z;
}
}
