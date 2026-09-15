#pragma once
#include "Graphics/ShadowAmbient.h"
#include <cmath>
namespace ShadowFixture {
using namespace Graphics;
inline Vector3 Cross(Vector3 a,Vector3 b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
inline Vector3 Normal(Vector3 v){float n=std::hypot(v[0],v[1],v[2]);for(auto& c:v)c/=n;return v;}
inline float Dot(Vector3 a,Vector3 b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
inline Matrix4 View(Vector3 eye={500,-700,500},Vector3 target={0,0,30}){
    Vector3 z{eye[0]-target[0],eye[1]-target[1],eye[2]-target[2]};z=Normal(z);auto x=Normal(Cross({0,0,1},z)),y=Cross(z,x);
    return {x[0],y[0],z[0],0,x[1],y[1],z[1],0,x[2],y[2],z[2],0,-Dot(x,eye),-Dot(y,eye),-Dot(z,eye),1};
}
inline Matrix4 Projection(float aspect=4.f/3,float nearD=10,float farD=3000){float f=1/std::tan(.5f);return {f/aspect,0,0,0,0,f,0,0,0,0,farD/(nearD-farD),-1,0,0,nearD*farD/(nearD-farD),0};}
}
