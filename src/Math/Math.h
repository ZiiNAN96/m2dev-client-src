#pragma once
// ZiiNAN: Removed final D3D9 compile-time dependency.
// Packed CPU values, row vectors, translation in row 4. SIMD stays internal.
#include <DirectXMath.h>
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <type_traits>
#include <vector>

namespace Math {
inline constexpr float Pi=3.14159265358979323846f;
inline constexpr float ToRadian(float degrees) { return degrees*(Pi/180.0f); }
inline constexpr float ToDegree(float radians) { return radians*(180.0f/Pi); }
struct Vector2 {
    float x,y;
    Vector2()=default;
    constexpr Vector2(float v0,float v1) : x(v0),y(v1) {}
    explicit Vector2(const float* p) : x(p[0]),y(p[1]) {}
    operator float*() { return &x; }
    operator const float*() const { return &x; }
    Vector2& operator+=(const Vector2& v) { x+=v.x; y+=v.y; return *this; }
    Vector2 operator+(const Vector2& v) const { return {x+v.x,y+v.y}; }
    Vector2& operator-=(const Vector2& v) { x-=v.x; y-=v.y; return *this; }
    Vector2 operator-(const Vector2& v) const { return {x-v.x,y-v.y}; }
    Vector2& operator*=(float s) { x*=s; y*=s; return *this; }
    Vector2& operator/=(float s) { return *this *= (1.0f/s); }
    Vector2 operator*(float s) const { auto v=*this; return v*=s; }
    Vector2 operator/(float s) const { auto v=*this; return v/=s; }
    Vector2 operator+() const { return *this; }
    Vector2 operator-() const { return {-x,-y}; }
    bool operator==(const Vector2& v) const { return x==v.x && y==v.y; }
    bool operator!=(const Vector2& v) const { return !(*this==v); }
    friend Vector2 operator*(float s,const Vector2& v) { return v*s; }
};
static_assert(sizeof(Vector2)==2*sizeof(float) && std::is_standard_layout_v<Vector2>);
struct Vector3 {
    float x,y,z;
    Vector3()=default;
    constexpr Vector3(float v0,float v1,float v2) : x(v0),y(v1),z(v2) {}
    explicit Vector3(const float* p) : x(p[0]),y(p[1]),z(p[2]) {}
    operator float*() { return &x; }
    operator const float*() const { return &x; }
    Vector3& operator+=(const Vector3& v) { x+=v.x; y+=v.y; z+=v.z; return *this; }
    Vector3 operator+(const Vector3& v) const { return {x+v.x,y+v.y,z+v.z}; }
    Vector3& operator-=(const Vector3& v) { x-=v.x; y-=v.y; z-=v.z; return *this; }
    Vector3 operator-(const Vector3& v) const { return {x-v.x,y-v.y,z-v.z}; }
    Vector3& operator*=(float s) { x*=s; y*=s; z*=s; return *this; }
    Vector3& operator/=(float s) { return *this *= (1.0f/s); }
    Vector3 operator*(float s) const { auto v=*this; return v*=s; }
    Vector3 operator/(float s) const { auto v=*this; return v/=s; }
    Vector3 operator+() const { return *this; }
    Vector3 operator-() const { return {-x,-y,-z}; }
    bool operator==(const Vector3& v) const { return x==v.x && y==v.y && z==v.z; }
    bool operator!=(const Vector3& v) const { return !(*this==v); }
    friend Vector3 operator*(float s,const Vector3& v) { return v*s; }
};
static_assert(sizeof(Vector3)==3*sizeof(float) && std::is_standard_layout_v<Vector3>);
struct Vector4 {
    float x,y,z,w;
    Vector4()=default;
    constexpr Vector4(float v0,float v1,float v2,float v3) : x(v0),y(v1),z(v2),w(v3) {}
    explicit Vector4(const float* p) : x(p[0]),y(p[1]),z(p[2]),w(p[3]) {}
    operator float*() { return &x; }
    operator const float*() const { return &x; }
    Vector4& operator+=(const Vector4& v) { x+=v.x; y+=v.y; z+=v.z; w+=v.w; return *this; }
    Vector4 operator+(const Vector4& v) const { return {x+v.x,y+v.y,z+v.z,w+v.w}; }
    Vector4& operator-=(const Vector4& v) { x-=v.x; y-=v.y; z-=v.z; w-=v.w; return *this; }
    Vector4 operator-(const Vector4& v) const { return {x-v.x,y-v.y,z-v.z,w-v.w}; }
    Vector4& operator*=(float s) { x*=s; y*=s; z*=s; w*=s; return *this; }
    Vector4& operator/=(float s) { return *this *= (1.0f/s); }
    Vector4 operator*(float s) const { auto v=*this; return v*=s; }
    Vector4 operator/(float s) const { auto v=*this; return v/=s; }
    Vector4 operator+() const { return *this; }
    Vector4 operator-() const { return {-x,-y,-z,-w}; }
    bool operator==(const Vector4& v) const { return x==v.x && y==v.y && z==v.z && w==v.w; }
    bool operator!=(const Vector4& v) const { return !(*this==v); }
    friend Vector4 operator*(float s,const Vector4& v) { return v*s; }
};
static_assert(sizeof(Vector4)==4*sizeof(float) && std::is_standard_layout_v<Vector4>);
struct Quaternion {
    float x,y,z,w;
    Quaternion()=default;
    constexpr Quaternion(float v0,float v1,float v2,float v3) : x(v0),y(v1),z(v2),w(v3) {}
    explicit Quaternion(const float* p) : x(p[0]),y(p[1]),z(p[2]),w(p[3]) {}
    operator float*() { return &x; }
    operator const float*() const { return &x; }
    Quaternion& operator+=(const Quaternion& v) { x+=v.x; y+=v.y; z+=v.z; w+=v.w; return *this; }
    Quaternion operator+(const Quaternion& v) const { return {x+v.x,y+v.y,z+v.z,w+v.w}; }
    Quaternion& operator-=(const Quaternion& v) { x-=v.x; y-=v.y; z-=v.z; w-=v.w; return *this; }
    Quaternion operator-(const Quaternion& v) const { return {x-v.x,y-v.y,z-v.z,w-v.w}; }
    Quaternion& operator*=(float s) { x*=s; y*=s; z*=s; w*=s; return *this; }
    Quaternion& operator/=(float s) { return *this *= (1.0f/s); }
    Quaternion operator*(float s) const { auto v=*this; return v*=s; }
    Quaternion operator/(float s) const { auto v=*this; return v/=s; }
    Quaternion operator+() const { return *this; }
    Quaternion operator-() const { return {-x,-y,-z,-w}; }
    bool operator==(const Quaternion& v) const { return x==v.x && y==v.y && z==v.z && w==v.w; }
    bool operator!=(const Quaternion& v) const { return !(*this==v); }
    friend Quaternion operator*(float s,const Quaternion& v) { return v*s; }
    Quaternion operator*(const Quaternion& q) const;
    Quaternion& operator*=(const Quaternion& q) { return *this=*this*q; }
};
static_assert(sizeof(Quaternion)==4*sizeof(float) && std::is_standard_layout_v<Quaternion>);
struct Plane {
    float a,b,c,d;
    Plane()=default;
    constexpr Plane(float v0,float v1,float v2,float v3) : a(v0),b(v1),c(v2),d(v3) {}
    explicit Plane(const float* p) : a(p[0]),b(p[1]),c(p[2]),d(p[3]) {}
    operator float*() { return &a; }
    operator const float*() const { return &a; }
    Plane& operator+=(const Plane& v) { a+=v.a; b+=v.b; c+=v.c; d+=v.d; return *this; }
    Plane operator+(const Plane& v) const { return {a+v.a,b+v.b,c+v.c,d+v.d}; }
    Plane& operator-=(const Plane& v) { a-=v.a; b-=v.b; c-=v.c; d-=v.d; return *this; }
    Plane operator-(const Plane& v) const { return {a-v.a,b-v.b,c-v.c,d-v.d}; }
    Plane& operator*=(float s) { a*=s; b*=s; c*=s; d*=s; return *this; }
    Plane& operator/=(float s) { return *this *= (1.0f/s); }
    Plane operator*(float s) const { auto v=*this; return v*=s; }
    Plane operator/(float s) const { auto v=*this; return v/=s; }
    Plane operator+() const { return *this; }
    Plane operator-() const { return {-a,-b,-c,-d}; }
    bool operator==(const Plane& v) const { return a==v.a && b==v.b && c==v.c && d==v.d; }
    bool operator!=(const Plane& v) const { return !(*this==v); }
    friend Plane operator*(float s,const Plane& v) { return v*s; }
};
static_assert(sizeof(Plane)==4*sizeof(float) && std::is_standard_layout_v<Plane>);
struct Color {
    float r,g,b,a;
    Color()=default;
    constexpr Color(float v0,float v1,float v2,float v3) : r(v0),g(v1),b(v2),a(v3) {}
    explicit Color(const float* p) : r(p[0]),g(p[1]),b(p[2]),a(p[3]) {}
    operator float*() { return &r; }
    operator const float*() const { return &r; }
    Color& operator+=(const Color& v) { r+=v.r; g+=v.g; b+=v.b; a+=v.a; return *this; }
    Color operator+(const Color& v) const { return {r+v.r,g+v.g,b+v.b,a+v.a}; }
    Color& operator-=(const Color& v) { r-=v.r; g-=v.g; b-=v.b; a-=v.a; return *this; }
    Color operator-(const Color& v) const { return {r-v.r,g-v.g,b-v.b,a-v.a}; }
    Color& operator*=(float s) { r*=s; g*=s; b*=s; a*=s; return *this; }
    Color& operator/=(float s) { return *this *= (1.0f/s); }
    Color operator*(float s) const { auto v=*this; return v*=s; }
    Color operator/(float s) const { auto v=*this; return v/=s; }
    Color operator+() const { return *this; }
    Color operator-() const { return {-r,-g,-b,-a}; }
    bool operator==(const Color& v) const { return r==v.r && g==v.g && b==v.b && a==v.a; }
    bool operator!=(const Color& v) const { return !(*this==v); }
    friend Color operator*(float s,const Color& v) { return v*s; }
    Color(uint32_t argb) : r(float((argb>>16)&255)/255),g(float((argb>>8)&255)/255),b(float(argb&255)/255),a(float(argb>>24)/255) {}
    operator uint32_t() const { const auto byte=[](float v){ return uint32_t(v>=1?255:v<=0?0:v*255+0.5f); }; return (byte(a)<<24)|(byte(r)<<16)|(byte(g)<<8)|byte(b); }
};
static_assert(sizeof(Color)==4*sizeof(float) && std::is_standard_layout_v<Color>);

struct Matrix : DirectX::XMFLOAT4X4 {
    Matrix()=default;
    using DirectX::XMFLOAT4X4::XMFLOAT4X4;
    explicit Matrix(const float* p) { std::memcpy(m,p,64); }
    operator float*() { return &_11; }
    operator const float*() const { return &_11; }
    Matrix operator*(const Matrix& other) const {
        Matrix result; DirectX::XMStoreFloat4x4(&result,DirectX::XMMatrixMultiply(DirectX::XMLoadFloat4x4(this),DirectX::XMLoadFloat4x4(&other))); return result;
    }
    Matrix& operator*=(const Matrix& other) { return *this=*this*other; }
    Matrix& operator*=(float s) { for(auto& row:m) for(float& v:row) v*=s; return *this; }
    Matrix& operator/=(float s) { return *this *= (1.f/s); }
    Matrix operator*(float s) const { Matrix r=*this; return r*=s; }
    Matrix operator/(float s) const { Matrix r=*this; return r/=s; }
    Matrix operator+() const { return *this; }
    Matrix operator-() const { return *this * -1.f; }
    Matrix& operator+=(const Matrix& v) { for(int y=0;y<4;++y) for(int x=0;x<4;++x) m[y][x]+=v.m[y][x]; return *this; }
    Matrix& operator-=(const Matrix& v) { for(int y=0;y<4;++y) for(int x=0;x<4;++x) m[y][x]-=v.m[y][x]; return *this; }
    Matrix operator+(const Matrix& v) const { auto r=*this; return r+=v; }
    Matrix operator-(const Matrix& v) const { auto r=*this; return r-=v; }
    bool operator==(const Matrix& v) const { for(int y=0;y<4;++y) for(int x=0;x<4;++x) if(m[y][x]!=v.m[y][x]) return false; return true; }
    bool operator!=(const Matrix& v) const { return !(*this==v); }
    friend Matrix operator*(float s,const Matrix& v) { return v*s; }
};
static_assert(sizeof(Matrix)==64 && alignof(Matrix)==alignof(float) && std::is_standard_layout_v<Matrix>);
struct Viewport { uint32_t X=0,Y=0,Width=0,Height=0; float MinZ=0,MaxZ=1; };
namespace detail {
inline DirectX::XMVECTOR Load(const Vector2& v) { return DirectX::XMVectorSet(v.x,v.y,0,0); }
inline DirectX::XMVECTOR Load(const Vector3& v) { return DirectX::XMVectorSet(v.x,v.y,v.z,0); }
inline DirectX::XMVECTOR Load(const Vector4& v) { return DirectX::XMVectorSet(v.x,v.y,v.z,v.w); }
inline DirectX::XMVECTOR Load(const Quaternion& v) { return DirectX::XMVectorSet(v.x,v.y,v.z,v.w); }
inline DirectX::XMVECTOR Load(const Plane& v) { return DirectX::XMVectorSet(v.a,v.b,v.c,v.d); }
template<class T> inline T* Store(T* out,DirectX::FXMVECTOR v) { DirectX::XMFLOAT4 f; DirectX::XMStoreFloat4(&f,v); std::memcpy(out,&f,sizeof(T)); return out; }
inline Matrix* Store(Matrix* out,DirectX::FXMMATRIX m) { DirectX::XMStoreFloat4x4(out,m); return out; }
inline DirectX::XMMATRIX Load(const Matrix& m) { return DirectX::XMLoadFloat4x4(&m); }
}
inline Vector2* Vec2Normalize(Vector2* o,const Vector2* v) { return detail::Store(o,DirectX::XMVector2Normalize(detail::Load(*v))); }
inline float Vec2Dot(const Vector2* a,const Vector2* b) { return a->x*b->x+a->y*b->y; }
inline float Vec2CCW(const Vector2* a,const Vector2* b) { return a->x*b->y-a->y*b->x; }
inline float Vec2Length(const Vector2* v) { return std::sqrt(Vec2Dot(v,v)); }
inline float Vec3Dot(const Vector3* a,const Vector3* b) { return a->x*b->x+a->y*b->y+a->z*b->z; }
inline float Vec3LengthSq(const Vector3* v) { return Vec3Dot(v,v); }
inline float Vec3Length(const Vector3* v) { return std::sqrt(Vec3LengthSq(v)); }
inline Vector3* Vec3Normalize(Vector3* o,const Vector3* v) { return detail::Store(o,DirectX::XMVector3Normalize(detail::Load(*v))); }
inline Vector3* Vec3Cross(Vector3* o,const Vector3* a,const Vector3* b) { return detail::Store(o,DirectX::XMVector3Cross(detail::Load(*a),detail::Load(*b))); }
inline Vector3* Vec3Add(Vector3* o,const Vector3* a,const Vector3* b) { *o=*a+*b; return o; }
inline Vector3* Vec3Scale(Vector3* o,const Vector3* v,float s) { *o=*v*s; return o; }
inline Vector3* Vec3Lerp(Vector3* o,const Vector3* a,const Vector3* b,float t) { *o=*a+(*b-*a)*t; return o; }
inline Vector3* Vec3TransformCoord(Vector3* o,const Vector3* v,const Matrix* m) { return detail::Store(o,DirectX::XMVector3TransformCoord(detail::Load(*v),detail::Load(*m))); }
inline Vector3* Vec3TransformNormal(Vector3* o,const Vector3* v,const Matrix* m) { return detail::Store(o,DirectX::XMVector3TransformNormal(detail::Load(*v),detail::Load(*m))); }
inline Vector4* Vec3Transform(Vector4* o,const Vector3* v,const Matrix* m) { return detail::Store(o,DirectX::XMVector3Transform(detail::Load(*v),detail::Load(*m))); }
inline Vector4* Vec4Transform(Vector4* o,const Vector4* v,const Matrix* m) { return detail::Store(o,DirectX::XMVector4Transform(detail::Load(*v),detail::Load(*m))); }
inline Matrix* MatrixIdentity(Matrix* o) { return detail::Store(o,DirectX::XMMatrixIdentity()); }
inline Matrix* MatrixMultiply(Matrix* o,const Matrix* a,const Matrix* b) { *o=*a * *b; return o; }
inline Matrix* MatrixTranspose(Matrix* o,const Matrix* m) { return detail::Store(o,DirectX::XMMatrixTranspose(detail::Load(*m))); }
inline float MatrixDeterminant(const Matrix* m) { return DirectX::XMVectorGetX(DirectX::XMMatrixDeterminant(detail::Load(*m))); }
inline Matrix* MatrixInverse(Matrix* o,float* determinant,const Matrix* m) { DirectX::XMVECTOR d; auto r=DirectX::XMMatrixInverse(&d,detail::Load(*m)); const float f=DirectX::XMVectorGetX(d); if(determinant)*determinant=f; if(f==0)return nullptr; return detail::Store(o,r); }
inline Matrix* MatrixScaling(Matrix* o,float x,float y,float z) { return detail::Store(o,DirectX::XMMatrixScaling(x,y,z)); }
inline Matrix* MatrixTranslation(Matrix* o,float x,float y,float z) { return detail::Store(o,DirectX::XMMatrixTranslation(x,y,z)); }
inline Matrix* MatrixRotationX(Matrix* o,float r) { return detail::Store(o,DirectX::XMMatrixRotationX(r)); }
inline Matrix* MatrixRotationY(Matrix* o,float r) { return detail::Store(o,DirectX::XMMatrixRotationY(r)); }
inline Matrix* MatrixRotationZ(Matrix* o,float r) { return detail::Store(o,DirectX::XMMatrixRotationZ(r)); }
inline Matrix* MatrixRotationAxis(Matrix* o,const Vector3* axis,float r) { return detail::Store(o,DirectX::XMMatrixRotationAxis(detail::Load(*axis),r)); }
inline Matrix* MatrixRotationYawPitchRoll(Matrix* o,float yaw,float pitch,float roll) { return detail::Store(o,DirectX::XMMatrixRotationRollPitchYaw(pitch,yaw,roll)); }
inline Matrix* MatrixRotationQuaternion(Matrix* o,const Quaternion* q) { return detail::Store(o,DirectX::XMMatrixRotationQuaternion(detail::Load(*q))); }
inline Matrix* MatrixLookAtRH(Matrix* o,const Vector3* eye,const Vector3* at,const Vector3* up) { return detail::Store(o,DirectX::XMMatrixLookAtRH(detail::Load(*eye),detail::Load(*at),detail::Load(*up))); }
inline Matrix* MatrixPerspectiveFovRH(Matrix* o,float fov,float aspect,float nearZ,float farZ) { return detail::Store(o,DirectX::XMMatrixPerspectiveFovRH(fov,aspect,nearZ,farZ)); }
inline Matrix* MatrixOrthoRH(Matrix* o,float w,float h,float nearZ,float farZ) { return detail::Store(o,DirectX::XMMatrixOrthographicRH(w,h,nearZ,farZ)); }
inline Matrix* MatrixOrthoOffCenterRH(Matrix* o,float l,float r,float b,float t,float nearZ,float farZ) { return detail::Store(o,DirectX::XMMatrixOrthographicOffCenterRH(l,r,b,t,nearZ,farZ)); }
inline Quaternion* QuaternionMultiply(Quaternion* o,const Quaternion* a,const Quaternion* b) { return detail::Store(o,DirectX::XMQuaternionMultiply(detail::Load(*a),detail::Load(*b))); }
inline Quaternion Quaternion::operator*(const Quaternion& q) const { Quaternion r; return *QuaternionMultiply(&r,this,&q); }
inline Quaternion* QuaternionConjugate(Quaternion* o,const Quaternion* q) { return detail::Store(o,DirectX::XMQuaternionConjugate(detail::Load(*q))); }
inline Quaternion* QuaternionRotationAxis(Quaternion* o,const Vector3* axis,float r) { return detail::Store(o,DirectX::XMQuaternionRotationAxis(detail::Load(*axis),r)); }
inline Quaternion* QuaternionRotationYawPitchRoll(Quaternion* o,float yaw,float pitch,float roll) { return detail::Store(o,DirectX::XMQuaternionRotationRollPitchYaw(pitch,yaw,roll)); }
inline float PlaneDotCoord(const Plane* p,const Vector3* v) { return p->a*v->x+p->b*v->y+p->c*v->z+p->d; }
inline Plane* PlaneNormalize(Plane* o,const Plane* p) { return detail::Store(o,DirectX::XMPlaneNormalize(detail::Load(*p))); }
inline Color* ColorModulate(Color* o,const Color* a,const Color* b) { *o={a->r*b->r,a->g*b->g,a->b*b->b,a->a*b->a}; return o; }
inline Vector3* Vec3Project(Vector3* o,const Vector3* v,const Viewport* viewport,const Matrix* projection,const Matrix* view,const Matrix* world) { return detail::Store(o,DirectX::XMVector3Project(detail::Load(*v),float(viewport->X),float(viewport->Y),float(viewport->Width),float(viewport->Height),viewport->MinZ,viewport->MaxZ,detail::Load(*projection),detail::Load(*view),detail::Load(*world))); }
inline Vector3* Vec3Unproject(Vector3* o,const Vector3* v,const Viewport* viewport,const Matrix* projection,const Matrix* view,const Matrix* world) { return detail::Store(o,DirectX::XMVector3Unproject(detail::Load(*v),float(viewport->X),float(viewport->Y),float(viewport->Width),float(viewport->Height),viewport->MinZ,viewport->MaxZ,detail::Load(*projection),detail::Load(*view),detail::Load(*world))); }

class MatrixStack {
    std::vector<Matrix> values;
public:
    MatrixStack() { LoadIdentity(); }
    void Clear() { values.clear(); LoadIdentity(); }
    Matrix* GetTop() { return &values.back(); }
    void LoadIdentity() { if(values.empty())values.emplace_back(); MatrixIdentity(GetTop()); }
    void LoadMatrix(const Matrix* m) { *GetTop()=*m; }
    void Push() { values.push_back(values.back()); }
    void Pop() { assert(values.size()>1); if(values.size()>1)values.pop_back(); }
    void MultMatrix(const Matrix* m) { *GetTop()=*GetTop() * *m; }
    void MultMatrixLocal(const Matrix* m) { *GetTop()=*m * *GetTop(); }
    void Translate(float x,float y,float z) { Matrix m; MatrixTranslation(&m,x,y,z); MultMatrix(&m); }
    void Scale(float x,float y,float z) { Matrix m; MatrixScaling(&m,x,y,z); MultMatrix(&m); }
    void RotateAxis(const Vector3* axis,float r) { Matrix m; MatrixRotationAxis(&m,axis,r); MultMatrix(&m); }
    void RotateAxisLocal(const Vector3* axis,float r) { Matrix m; MatrixRotationAxis(&m,axis,r); MultMatrixLocal(&m); }
    void RotateYawPitchRollLocal(float yaw,float pitch,float roll) { Matrix m; MatrixRotationYawPitchRoll(&m,yaw,pitch,roll); MultMatrixLocal(&m); }
};
}
