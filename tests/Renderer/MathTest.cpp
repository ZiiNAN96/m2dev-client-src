// ZiiNAN: Removed final D3D9 compile-time dependency. Analytic CPU contracts.
#include "Math/Math.h"
#include "Renderer/DrawStateTypes.h"
#include <iostream>
#include <stdexcept>
#include <random>
using namespace Math;
static unsigned checks=0;
static void Check(bool ok,const char* what) { ++checks; if(!ok) throw std::runtime_error(what); }
static bool Near(float a,float b,float epsilon=0.0001f) { return std::abs(a-b)<=epsilon*std::max({1.f,std::abs(a),std::abs(b)}); }
static void MatrixNear(const Matrix& a,const Matrix& b) { for(unsigned i=0;i<16;++i) Check(Near((&a._11)[i],(&b._11)[i]),"matrix equality"); }
int main() {
 try {
    static_assert(sizeof(Vector2)==8 && sizeof(Vector3)==12 && sizeof(Vector4)==16);
    static_assert(sizeof(Quaternion)==16 && sizeof(Plane)==16 && sizeof(Color)==16 && sizeof(Matrix)==64);
    static_assert(alignof(Matrix)==4 && sizeof(Renderer::MaterialValues)==68 && sizeof(Renderer::LightValues)==104);
    Check(Renderer::VertexStride(Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1)==32,"actor vertex stride");
    Check(Renderer::VertexStride(Renderer::VertexPosition|Renderer::VertexColor|Renderer::VertexTex1)==24,"UI vertex stride");
    Check((Renderer::WriteRed|Renderer::WriteGreen|Renderer::WriteBlue)==7,"RGB write channels");
    Check(Renderer::PackColor(1,0.5f,0,1)==0xffff7f00u,"asset color truncates");
    Check(uint32_t(Color(1,0.5f,0,1))==0xffff8000u,"float material color rounds");
    Check(uint32_t(Color(0x12345678u))==0x12345678u,"ARGB unpack roundtrip");
    for(unsigned c=0;c<256;++c) {
        const uint32_t value=(c<<24)|((255-c)<<16)|(c<<8)|(255-c);
        Check(uint32_t(Color(value))==value,"all byte colors roundtrip");
    }
    Matrix identity; MatrixIdentity(&identity);
    Matrix scale,translation,combined; MatrixScaling(&scale,2,3,4); MatrixTranslation(&translation,5,6,7);
    MatrixMultiply(&combined,&scale,&translation);
    Vector3 point{1,2,3},actual; Vec3TransformCoord(&actual,&point,&combined);
    Check(actual==Vector3(7,12,19),"row vectors: scale before translate");
    Vec3TransformNormal(&actual,&point,&translation); Check(actual==point,"normal excludes translation");
    Matrix alias=scale; MatrixMultiply(&alias,&alias,&translation); MatrixNear(alias,combined);
    Matrix inverse; float determinant=0; Check(MatrixInverse(&inverse,&determinant,&combined)!=nullptr && Near(determinant,24),"inverse determinant");
    MatrixNear(combined*inverse,identity);
    Matrix singular; MatrixScaling(&singular,0,1,1); inverse=translation;
    Check(!MatrixInverse(&inverse,&determinant,&singular) && determinant==0,"singular inverse rejected");
    MatrixNear(inverse,translation);
    Matrix rotation; MatrixRotationZ(&rotation,Pi/2);
    point={1,0,0}; Vec3TransformCoord(&actual,&point,&rotation);
    Check(Near(actual.x,0) && Near(actual.y,1),"positive rotation convention");
    Quaternion x,y,product; Vector3 axisX{1,0,0},axisY{0,1,0};
    QuaternionRotationAxis(&x,&axisX,0.4f); QuaternionRotationAxis(&y,&axisY,0.7f);
    QuaternionMultiply(&product,&x,&y);
    Matrix mx,my,mq; MatrixRotationQuaternion(&mx,&x); MatrixRotationQuaternion(&my,&y); MatrixRotationQuaternion(&mq,&product);
    MatrixNear(mq,mx*my);
    Quaternion aliasQ=x; QuaternionMultiply(&aliasQ,&aliasQ,&y);
    Check(Near(aliasQ.x,product.x) && Near(aliasQ.w,product.w),"quaternion aliasing");
    Quaternion conjugate; QuaternionConjugate(&conjugate,&x); QuaternionMultiply(&product,&x,&conjugate);
    Check(Near(product.x,0) && Near(product.w,1),"quaternion conjugate");
    Plane plane{0,0,2,-6},normalized; PlaneNormalize(&normalized,&plane);
    point={0,0,3}; Check(Near(PlaneDotCoord(&normalized,&point),0) && Near(normalized.c,1),"plane normalization");
    Vector3 zero{0,0,0}; Vec3Normalize(&actual,&zero); Check(actual==zero,"zero normalization");
    Vector3 cross; Vec3Cross(&cross,&axisX,&axisY); Check(cross==Vector3(0,0,1),"cross product handedness");
    MatrixStack stack; stack.Translate(5,6,7); stack.Push(); stack.Scale(2,3,4);
    MatrixNear(*stack.GetTop(),translation*scale); stack.Pop(); MatrixNear(*stack.GetTop(),translation);
    stack.MultMatrixLocal(&scale); MatrixNear(*stack.GetTop(),scale*translation); stack.Clear(); MatrixNear(*stack.GetTop(),identity);
    Vector3 eye{0,0,10},at{0,0,0},up{0,1,0}; Matrix view,projection;
    MatrixLookAtRH(&view,&eye,&at,&up); MatrixPerspectiveFovRH(&projection,Pi/2,4.f/3.f,1,100);
    Viewport viewport{17,29,800,600,0.2f,0.9f};
    point={0,0,9}; Vec3Project(&actual,&point,&viewport,&projection,&view,&identity);
    Check(Near(actual.x,417) && Near(actual.y,329) && Near(actual.z,0.2f),"RH near plane and viewport origin");
    point={0,0,-90}; Vec3Project(&actual,&point,&viewport,&projection,&view,&identity);
    Check(Near(actual.z,0.9f),"far plane depth");
    std::mt19937 random(1303); std::uniform_real_distribution<float> coordinate(-3,3);
    for(unsigned i=0;i<250;++i) {
        point={coordinate(random),coordinate(random),coordinate(random)};
        Vector3 screen,restored; Vec3Project(&screen,&point,&viewport,&projection,&view,&combined);
        Vec3Unproject(&restored,&screen,&viewport,&projection,&view,&combined);
        Check(Near(restored.x,point.x,0.002f) && Near(restored.y,point.y,0.002f) && Near(restored.z,point.z,0.002f),"project/unproject roundtrip");
    }
    MatrixOrthoOffCenterRH(&projection,-2,6,-3,5,1,101); point={-2,5,-1};
    Vec3Project(&actual,&point,&viewport,&projection,&identity,&identity);
    Check(Near(actual.x,17) && Near(actual.y,29) && Near(actual.z,0.2f),"orthographic top left");
    std::cout<<"Math layout, row-vector/RH projection, inverse, quaternion, color, stack: "<<checks<<" PASS\n";
    return 0;
 } catch(const std::exception& error) { std::cerr<<error.what()<<" after "<<checks<<" checks\n"; return 1; }
}
