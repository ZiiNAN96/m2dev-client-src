#include "ShadowFixture.h"
#include "Graphics/GraphicsSettings.h"
#include "Graphics/SceneLighting.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Graphics;
static void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    Check(argc==2,"mode required");const std::string mode=argv[1];
    if(mode=="Config"){
        for(unsigned level=0;level<6;++level){auto c=ResolveShadowQuality(level,20000);Check(c.cascades<=3&&c.distance<=9000&&c.resolution<=2048,"bounded quality");}
        ShadowQualityConfig s;s.cascades=999;s.distance=-1;s.depthBias=std::numeric_limits<float>::quiet_NaN();s=ValidateShadowConfig(s);Check(s.cascades==0&&s.distance>=500&&std::isfinite(s.depthBias),"safe invalid shadow config");
        AmbientDepthConfig a;a.radius=std::numeric_limits<float>::infinity();a.intensity=std::numeric_limits<float>::quiet_NaN();a.quality=99;a=ValidateAmbientConfig(a);Check(a.quality==0&&a.radius==60&&a.intensity==.45f,"invalid AO fallback");
        for(unsigned i=0;i<4;++i){auto modern=Resolve(PresetSettings(GraphicsPreset(i),GraphicsStyle::Modern)),classic=Resolve(PresetSettings(GraphicsPreset(i)));Check(classic.shadowConfig.cascades==0&&classic.ambientConfig.quality==0,"Classic gates");Check(modern.shadowConfig.cascades==(i==0?0:i==1?2:3)&&modern.ambientConfig.quality==(i==0?0:i==1?1:2),"real presets");}
    }else if(mode=="Splits"){
        for(unsigned count=1;count<=3;++count){auto split=CascadeSplits(10,5000,count,.65f);float last=10;for(unsigned i=0;i<count;++i){Check(split[i]>last,"monotonic splits");last=split[i];}Check(last==5000,"exact distance end");}
        Check(CascadeSplits(0,0,99,0)==std::array<float,3>{},"invalid splits rejected");
    }else if(mode=="Sun"){
        auto config=ResolveShadowQuality(4,20000);const auto view=ShadowFixture::View(),projection=ShadowFixture::Projection();
        for(Vector3 direction: {Vector3{-1,0,1},Vector3{1,0,1},Vector3{0,0,1}}){const auto c=BuildCascades(view,projection,direction,config);Check(c.count==3,"valid sun cascades");
            const Vector3 p{0,0,100},projected{-100*direction[0]/direction[2],0,0};
            const auto a=TransformPoint(p,c.cascade[1].worldToClip),b=TransformPoint(projected,c.cascade[1].worldToClip);Check(std::abs(a[0]-b[0])<1e-5&&std::abs(a[1]-b[1])<1e-5&&a[2]<b[2],"sun surface-to-sun casts opposite and closer depth");}
        Check(BuildCascades(view,projection,{0,0,0},config).count==0,"zero direction disables shadows");
        Check(BuildCascades(view,projection,{std::numeric_limits<float>::quiet_NaN(),0,1},config).count==0,"NaN direction disables shadows");
    }else if(mode=="Stabilization"){
        auto config=ResolveShadowQuality(4,20000);auto a=BuildCascades(ShadowFixture::View(),ShadowFixture::Projection(),{1,0,1},config);
        auto b=BuildCascades(ShadowFixture::View({500.0001f,-700,500},{.0001f,0,30}),ShadowFixture::Projection(),{1,0,1},config);
        Check(a.count==3&&b.count==3,"stabilized cascade count");for(unsigned i=0;i<3;++i){Check(a.cascade[i].radius==b.cascade[i].radius,"stable extent");for(unsigned j:{0u,1u,4u,5u,8u,9u,12u,13u})Check(std::abs(a.cascade[i].worldToClip[j]-b.cascade[i].worldToClip[j])<1e-6f,"subtexel XY stability");}
        Check(!IntersectsCascade(a.cascade[0],{1e7f,1e7f,0},10),"far caster culled");
    }else if(mode=="Reconstruction"){
        for(float aspect:{.5f,4.f/3,2.f}){const auto projection=ShadowFixture::Projection(aspect);Matrix4 inverse;Check(Inverse(projection,inverse),"inverse projection");
            for(float depth:{0.f,.5f,.999f})for(float x:{-1.f,0.f,1.f})for(float y:{-1.f,0.f,1.f}){Vector3 clip{x,y,depth};auto p=TransformPoint(clip,inverse),restored=TransformPoint(p,projection);for(unsigned c=0;c<3;++c)Check(std::abs(restored[c]-clip[c])<1e-4f,"near far corner resize reconstruction");}}
        Matrix4 inverse;Check(!Inverse({},inverse),"singular inverse rejected");
    }else if(mode=="Boundary"){
        Check(sizeof(void*)==8,"64-bit configuration math");Check(sizeof(Matrix4)==64,"portable matrix contract");
        std::cout<<"pointer="<<sizeof(void*)<<" long="<<sizeof(long)<<'\n';
    }else throw std::runtime_error("unknown mode");
    std::cout<<"PASS "<<mode<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
