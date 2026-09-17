#include "Graphics/AtmosphereConfig.h"
#include <limits>
#include <iostream>
#include <stdexcept>
void Check(bool result,const char* message){if(!result)throw std::runtime_error(message);}
int main() {
    try {
        using namespace Graphics;
        for(auto preset:{GraphicsPreset::Low,GraphicsPreset::Medium,GraphicsPreset::High,GraphicsPreset::Ultra}) {
            auto settings=PresetSettings(preset,GraphicsStyle::Modern);
            auto config=Resolve(settings);
            Check(config.hdr,"all Modern presets include HDR");
            auto atmosphere=ResolveAtmosphere({},config);
            Check(atmosphere.exposure==2.f,"fixed exposure is independent of quality");
            Check(config.bloom==(preset==GraphicsPreset::High||preset==GraphicsPreset::Ultra),"highlight-only bloom default");
            Check(atmosphere.skyWidth==(preset==GraphicsPreset::Low?128u:512u),"two real sky resolutions");
            settings.style=GraphicsStyle::Classic;config=Resolve(settings);
            Check(!config.hdr&&!config.bloom&&!GraphicsFeatures{config}.UseModernSky(),"Classic bypasses all new effects");
        }
        GraphicsRuntimeConfig config;SceneLighting light;
        light.exposureBias=1;Check(ResolveAtmosphere(light,config).exposure==4,"one stop doubles exposure");
        light.exposureBias=-1;Check(ResolveAtmosphere(light,config).exposure==1,"negative bias halves exposure");
        light.exposureBias=100;Check(ResolveAtmosphere(light,config).exposure==16,"bounded map exposure");
        light.exposureBias=std::numeric_limits<float>::quiet_NaN();Check(ResolveAtmosphere(light,config).exposure==2,"invalid exposure has safe default");
        light.sunDirection={std::numeric_limits<float>::infinity(),0,1};light.sunIntensity=2;
        Check(ValidateSceneLighting(light).sunIntensity==0,"invalid sun cannot produce NaN sky");
        light={};light.sunIntensity=3;
        const auto morning=WithDevelopmentSun(light,0),noon=WithDevelopmentSun(light,1),evening=WithDevelopmentSun(light,2);
        Check(morning.sunDirection[0]<0&&evening.sunDirection[0]>0,"morning/evening use opposite sun directions");
        Check(-noon.sunDirection[2]>-morning.sunDirection[2]&&noon.sunIntensity==3,"noon elevation changes the shared source");
        light.fogNear=100;light.fogFar=0;light.fogEnabled=true;
        Check(!ValidateSceneLighting(light).fogEnabled,"invalid fog interval rejected");
        light.skyIBLIntensity=std::numeric_limits<float>::quiet_NaN();
        Check(ValidateSceneLighting(light).skyIBLIntensity==0,"invalid sky IBL rejected");
        light.skyIBLIntensity=10;Check(ValidateSceneLighting(light).skyIBLIntensity==1,"bounded sky IBL policy");
        light.cloudCoverage=std::numeric_limits<float>::quiet_NaN();
        Check(ValidateSceneLighting(light).cloudCoverage==0,"invalid clouds cannot poison the atlas");
        light.cloudCoverage=4;Check(ValidateSceneLighting(light).cloudCoverage==1,"bounded cloud coverage");
        double sixty=0,oneTwenty=0;
        for(unsigned i=0;i<600;++i)sixty+=1.0/60;
        for(unsigned i=0;i<1200;++i)oneTwenty+=1.0/120;
        Check(CloudOffset(sixty)==CloudOffset(oneTwenty),"60/120 FPS produce identical ten-second cloud offsets");
        Check(CloudOffset(4810)==CloudOffset(10),"cloud noise wraps after the common period");
        Check(CloudOffset(std::numeric_limits<double>::infinity())==std::array<float,2>{},"nonfinite time is safe");
        std::cout<<"PASS G56 exposure, presets, sun and environment validation\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
