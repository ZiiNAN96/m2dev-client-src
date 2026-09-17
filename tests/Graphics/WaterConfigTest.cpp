#include "Graphics/WaterConfig.h"
#include "Graphics/SceneLighting.h"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace Graphics;
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try {
    for(int q=0;q<4;++q){
        auto settings=PresetSettings(static_cast<GraphicsPreset>(q),GraphicsStyle::Modern);
        auto runtime=Resolve(settings);auto water=ResolveWater(runtime);
        Check(water.enabled&&int(runtime.water)==q,"four real water presets");
        Check(water.ssr==(q>=2)&&water.refraction==(q>=1)&&water.secondNormal==(q>=1),"SSR and refraction quality mapping");
        Check(water.halfResolution==(q==2)&&water.maxTraversal==(q==3?128:64),"High half resolution vs Ultra full resolution");
        Check(LoadGraphicsSettings(SaveGraphicsSettings(settings)).settings==settings,"all water quality values persist");
        settings.style=GraphicsStyle::Classic;auto classic=Resolve(settings);
        Check(!ResolveWater(classic).enabled&&classic.waterFrameMilliseconds==70&&classic.water==WaterQuality::High,"Classic remains unchanged");
    }
    Check(std::abs(WaterFresnel(1)-.0203732f)<1e-6f&&WaterFresnel(0)==1,"water IOR Fresnel endpoints");
    Check(WaterFresnel(.1f)>WaterFresnel(.5f)&&WaterFresnel(.5f)>WaterFresnel(.9f),"grazing reflection increases");
    double t60=0,t120=0;for(unsigned i=0;i<600;++i)t60+=1./60;for(unsigned i=0;i<1200;++i)t120+=1./120;
    const auto a=WaterUVOffset(t60),b=WaterUVOffset(t120);
    for(unsigned i=0;i<4;++i)Check(std::abs(a[i]-b[i])<1e-6f,"same movement at 60 and 120 FPS");
    WaterMaterial bad;bad.roughness=std::numeric_limits<float>::quiet_NaN();bad.absorption={-1,100,bad.roughness};
    auto valid=ValidateWater(bad);Check(valid.roughness==WaterMaterial{}.roughness&&valid.absorption[0]==0&&valid.absorption[1]==.01f&&std::isfinite(valid.absorption[2]),"invalid material safe");
    SceneLighting light;light.sunDirection={bad.roughness,0,0};light.sunIntensity=2;
    Check(ValidateSceneLighting(light).sunIntensity==0,"same validated sun disables invalid direct lighting");
    Check(WaterUVOffset(std::numeric_limits<double>::infinity())==std::array<float,4>{},"invalid time safe");
    std::cout<<"PASS Water config, four qualities, persistence, IOR Fresnel, 60/120 FPS, invalid data and shared sun\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
