#include "VegetationRuntime.h"
#include <algorithm>
#include <cmath>

namespace Vegetation {
Quality ResolveQuality(unsigned level) {
    constexpr Quality presets[]={{.65f,5000,.18f,.35f,0},{.85f,7500,.35f,.65f,.5f},
        {1,10000,.65f,1,1},{1.25f,13000,1,1,1}};
    return presets[std::min(level,3u)];
}
LodBlend SelectModernLOD(const Metadata& m,float distance) {
    if(m.version!=2)return {SelectLOD(m,distance),{},0};
    if(!std::isfinite(distance)||distance<0||distance>m.cullDistance||m.lods.size()!=4)return {};
    for(unsigned i=0;i<3;++i) {
        const float boundary=m.lodDistances[i],width=boundary*m.transitionFraction;
        if(distance<boundary-width*.5f)return {m.lods[3-i],{},0};
        if(width>0&&distance<boundary+width*.5f)
            return {m.lods[3-i],m.lods[2-i],(distance-boundary+width*.5f)/width};
    }
    return {m.lods.front(),{},0};
}
std::array<std::array<float,4>,6> FrustumPlanes(const Matrix& v,const Matrix& p) {
    Matrix m{};for(unsigned r=0;r<4;++r)for(unsigned c=0;c<4;++c)
        for(unsigned k=0;k<4;++k)m[r*4+c]+=v[r*4+k]*p[k*4+c];
    std::array<std::array<float,4>,6> planes{};
    for(unsigned r=0;r<4;++r) {
        planes[0][r]=m[r*4+3]+m[r*4];planes[1][r]=m[r*4+3]-m[r*4];
        planes[2][r]=m[r*4+3]+m[r*4+1];planes[3][r]=m[r*4+3]-m[r*4+1];
        planes[4][r]=m[r*4+2];planes[5][r]=m[r*4+3]-m[r*4+2];
    }
    return planes; // Diligent D3D depth range [0,1], row-vector convention.
}
namespace {
std::uint64_t Mix(std::uint64_t x) {x+=0x9e3779b97f4a7c15ull;x=(x^(x>>30))*0xbf58476d1ce4e5b9ull;x=(x^(x>>27))*0x94d049bb133111ebull;return x^(x>>31);}
float Unit(std::uint64_t x) {return float(Mix(x)>>40)*(1.f/16777216.f);}
}
std::vector<GrassCandidate> GrassCandidates(std::uint64_t seed,int x,int y,float size,unsigned count) {
    std::vector<GrassCandidate> out;if(!std::isfinite(size)||size<=0||count>1024)return out;
    seed=Mix(seed^Mix(std::uint32_t(x))^Mix(std::uint64_t(std::uint32_t(y))<<32));out.reserve(count);
    for(unsigned i=0;i<count;++i) {const auto id=Mix(seed+i);out.push_back({(x+Unit(id))*size,(y+Unit(id+1))*size,
        Unit(id+2)*6.28318530718f,.75f+Unit(id+3)*.5f,Unit(id+4),id});}
    return out;
}
Matrix GrassPlacement::Transform() const {
    auto m=Identity;m[0]=m[5]=cosScale;m[1]=sinScale;m[4]=-sinScale;
    m[10]=scale;m[12]=x;m[13]=-y;m[14]=z;return m;
}
GrassPlacement PlaceGrass(const GrassCandidate& c,float height,float density) {
    GrassPlacement p{c.x,c.y,height,std::cos(c.rotation)*c.scale,std::sin(c.rotation)*c.scale,c.scale,0,c.rank/density};
    p.phase=WindPhase(p.Transform(),c.id);return p;
}
Vec3 BranchDisplacement(float fraction,float height,float time,float phase,const WindProfile& w,const Vec3& direction) {
    if(!std::isfinite(fraction)||!std::isfinite(height)||!std::isfinite(time)||!std::isfinite(phase))return {};
    const float length=std::hypot(direction[0],direction[1]);if(length<1e-6f)return {};
    const float f=std::clamp(fraction,0.f,1.f);
    const float amount=std::sin(time*w.frequency*.65f+phase)*w.strength*w.branchAmplitude*height*f*f;
    return {direction[0]/length*amount,direction[1]/length*amount,0};
}
}
