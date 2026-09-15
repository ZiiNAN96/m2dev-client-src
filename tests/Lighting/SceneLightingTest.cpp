#include "Graphics/SceneLighting.h"
#include "Graphics/GraphicsSettings.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace Graphics;
static void Check(bool value,const char* why){if(!value)throw std::runtime_error(why);}
static bool Near(float a,float b){return std::abs(a-b)<1e-6f;}
static void Validation()
{
    SceneLighting v;const SceneLighting defaults;
    for(const auto d:{LightVector{0,0,0},LightVector{NAN,1,0},LightVector{INFINITY,0,1}}){v.sun.direction=d;auto r=ValidateLighting(v);Check(r.sun.direction==defaults.sun.direction,"invalid direction fallback");}
    v.sun.direction={std::numeric_limits<float>::max(),std::numeric_limits<float>::max(),0};
    v.sun.color={NAN,-1,INFINITY};v.sun.intensity=-1;
    v.ambientSkyColor={-1,2,NAN};v.ambientGroundColor={INFINITY,-2,3};v.ambientIntensity=NAN;
    auto r=ValidateLighting(v);Check(Near(r.sun.direction[0],std::sqrt(.5f))&&Near(r.sun.direction[1],std::sqrt(.5f)),"large finite direction normalizes safely");
    Check(r.sun.color==LightVector{1,0,1}&&r.sun.intensity==0,"sun color/intensity clamp");
    Check(r.ambientSkyColor[0]==0&&r.ambientSkyColor[1]==1&&r.ambientSkyColor[2]==defaults.ambientSkyColor[2],"ambient channel validation");
    Check(r.ambientGroundColor[0]==defaults.ambientGroundColor[0]&&r.ambientGroundColor[1]==0&&r.ambientGroundColor[2]==1&&r.ambientIntensity==1,"ground fallback");
    v.sun.intensity=100;v.ambientIntensity=100;r=ValidateLighting(v);Check(r.sun.intensity==4&&r.ambientIntensity==2,"documented LDR bounds");
}
static void MapConversion()
{
    auto d=ConvertMapLighting({});Check(d.sun.direction[2]>0&&Near(d.ambientSkyColor[0],.21404114f),"missing map fields retain defaults and decode once");
    MapLighting map;map.direction={0,0,-2};map.diffuse={.5f,1,0};map.ambient={.25f,.5f,1};
    auto r=ConvertMapLighting(map);Check(r.sun.direction==LightVector{0,0,1}&&Near(r.sun.color[0],.21404114f)&&r.sun.color[1]==1&&r.sun.color[2]==0,"travel direction negated; colors sRGB to linear");
    Check(Near(r.ambientGroundColor[1],r.ambientSkyColor[1]*.4f)&&r.ambientSkyColor[2]==1,"hemisphere preserves map hue");
    map.sunEnabled=false;Check(ConvertMapLighting(map).sun.intensity==0,"explicit disabled sun keeps ambient");
    map.ambient={0,0,0};map.materialEmissive={.321569f,.321569f,.411765f};
    r=ConvertMapLighting(map);Check(r.ambientSkyColor[0]>.08f&&r.ambientSkyColor[2]>r.ambientSkyColor[0],"actual A1 scene fill survives zero background ambient");
    map.materialEmissive={0,0,0};r=ConvertMapLighting(map);Check(r.ambientSkyColor[0]==.03f&&r.ambientGroundColor[0]==.012f,"missing scene fill gets small hemisphere fallback");
}
static void Revision()
{
    SceneLightingState scene;MapLighting a,b;b.diffuse={.25f,.7f,1};b.direction={1,0,0};
    scene.SetMap(a);const auto first=scene.Revision();const auto old=scene.Get();
    for(int frame=0;frame<20;++frame)Check(!scene.SetMap(a)&&scene.Revision()==first,"warm frame does not rebuild lighting");
    Check(scene.SetMap(b)&&scene.Revision()==first+1,"map B changes revision");
    Check(scene.SetMap(a)&&scene.Get()==old,"return to A fully restores all values");
    Check(!scene.Set(scene.Get()),"identical explicit scene does not upload");
}
static void Settings()
{
    for(auto preset:{GraphicsPreset::Low,GraphicsPreset::Medium,GraphicsPreset::High,GraphicsPreset::Ultra})
        for(auto style:{GraphicsStyle::Classic,GraphicsStyle::Modern}){
            auto settings=PresetSettings(preset,style);auto round=LoadGraphicsSettings(SaveGraphicsSettings(settings));
            Check(round.settings==settings,"existing config persists style and preset");
            Check(Resolve(settings).usePBR==(style==GraphicsStyle::Modern),"all presets use real central style, no fake lighting levels");
        }
}
int main(int argc,char** argv){try{Check(argc==2,"test mode");std::string mode=argv[1];
    if(mode=="Validation")Validation();else if(mode=="MapConversion")MapConversion();else if(mode=="Revision")Revision();else if(mode=="Settings")Settings();else Check(false,"unknown mode");
    std::cout<<"PASS "<<mode<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
