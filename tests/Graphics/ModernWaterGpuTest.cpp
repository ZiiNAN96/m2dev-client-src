#include "Renderer/DiligentD3D11Backend.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentWorldRenderer.h"
#include "Renderer/WaterDiagnostics.h"
#include "Renderer/ModernFrame.h"
#include "Renderer/Diagnostics.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Platform/PlatformWindow.h"
#include <iostream>
#include <fstream>
#include <thread>
#include <chrono>
#include <stdexcept>
using namespace Renderer;
namespace {
unsigned errors{};
void DILIGENT_CALL_TYPE Message(Diligent::DEBUG_MESSAGE_SEVERITY severity,const char* message,const char*,const char*,int){if(severity>=Diligent::DEBUG_MESSAGE_SEVERITY_ERROR){++errors;std::cerr<<message<<'\n';}}
void Check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
constexpr std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
void Save(const char* name,const std::vector<uint8_t>& pixels,unsigned w,unsigned h){std::ofstream file(name,std::ios::binary);file<<"P6\n"<<w<<' '<<h<<"\n255\n";file.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());}
long long Difference(const std::vector<uint8_t>& a,const std::vector<uint8_t>& b,unsigned begin=0){long long result=0;for(size_t i=begin;i<a.size();++i)result+=std::abs(int(a[i])-int(b[i]));return result;}
bool SameQuantizedImage(const std::vector<uint8_t>& a,const std::vector<uint8_t>& b){
    // FP16 -> tone mapping can cross an 8-bit rounding boundary. Permit one
    // code value in <=0.1% of channels, never an object-shaped history trail.
    if(a.size()!=b.size())return false;unsigned changed=0;
    for(size_t i=0;i<a.size();++i){const auto d=std::abs(int(a[i])-int(b[i]));if(d>1)return false;changed+=d!=0;}
    return changed<=a.size()/1000;
}
}
int main(){try {
    Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
    Platform::PlatformWindow window;Platform::WindowCreateInfo info;info.title="G7 water proof";Check(window.Create(info),"window");window.SetSize(640,480);
    DiligentD3D11Backend backend;Check(backend.Initialize({window.GetNativeHandle().value,640,480}),"D3D11");
    {
        DiligentStaticObjectRenderer meshes(backend);Check(meshes.Initialize(false),"mesh frontend");
        DiligentWorldRenderer water(backend);Check(water.Initialize(),"water frontend");
        auto quad=[&](float x0,float x1,float y0,float y1,float z0,float z1,bool upright){
            StaticObjectSource source;
            if(upright)source.vertices={{{x0,y0,z0,0,-1,0,0,0}},{{x1,y0,z0,0,-1,0,1,0}},{{x1,y0,z1,0,-1,0,1,1}},{{x0,y0,z1,0,-1,0,0,1}}};
            else source.vertices={{{x0,y0,z0,0,0,1,0,0}},{{x1,y0,z0,0,0,1,1,0}},{{x1,y1,z1,0,0,1,1,1}},{{x0,y1,z1,0,0,1,0,1}}};
            source.indices={0,1,2,0,2,3};return meshes.UploadGeometry(source);
        };
        auto floor=quad(-2000,2000,-1000,4000,-15,-700,false);
        auto building=quad(-650,-150,1500,0,0,650,true),tree=quad(250,520,1500,0,0,850,true),actor=quad(-80,60,900,0,-100,260,true);
        const unsigned char texel[]{190,170,115,255};TerrainTextureData image;image.width=image.height=1;image.format=TerrainTextureFormat::RGBA8;image.mips.push_back({texel,4,4});auto texture=meshes.UploadTexture(image);
        StaticObjectDraw draw;draw.matrices.world=identity;draw.normalTransform=identity;
        const float angle=.35f,c=std::cos(angle),s=std::sin(angle),cy=-1000,cz=480;
        draw.matrices.view={1,0,0,0,0,s,-c,0,0,c,s,0,0,-(cy*s+cz*c),cy*c-cz*s,1};
        draw.matrices.projection={1.299038f,0,0,0,0,1.732051f,0,0,0,0,-10000.f/9990.f,-1,0,0,-100000.f/9990.f,0};
        draw.indexCount=6;draw.vertexCount=4;draw.cull=StaticObjectCull::None;
        auto material=std::make_shared<MaterialRuntimeData>();draw.material=material;
        const EffectVertex surface[]{{{-1600,-600,0},0xffffffff,{}},{{1600,-600,0},0xffffffff,{}},{{-1600,3200,0},0xffffffff,{}},{{1600,3200,0},0xffffffff,{}}};
        EffectDraw waterDraw;waterDraw.matrices=draw.matrices;waterDraw.textured=false;
        auto settings=Graphics::PresetSettings(Graphics::GraphicsPreset::High,Graphics::GraphicsStyle::Modern);settings.shadows=Graphics::ShadowQuality::Off;settings.ambientOcclusion=Graphics::AmbientOcclusionQuality::Off;settings.bloom=false;
        Graphics::SceneLighting light;light.sunDirection={0,-.7f,-.7f};light.sunIntensity=3.14159265f;light.ambient={.25f,.25f,.25f};
        unsigned revision=0,width=640,height=480;
        auto render=[&](unsigned quality,double time,bool objects=true,float actorX=0,bool waterVisible=true){
            waterTestSeconds=time;
            settings.water=static_cast<Graphics::WaterQuality>(quality);ApplyGraphicsRuntimeConfig(Graphics::Resolve(settings,++revision));
            Check(backend.BeginFrame(),"begin");backend.Clear({true,ClearColor{0,0,0,1}});modernFrame->Begin(light,true);modernFrame->SetCamera(draw.matrices);
            draw.matrices.world=identity;material->emissive={0,0,0};meshes.Draw(floor,texture,draw);
            if(objects){material->emissive={.7f,.04f,.02f};meshes.Draw(building,texture,draw);material->emissive={.02f,.55f,.04f};meshes.Draw(tree,texture,draw);
                material->emissive={.03f,.04f,.8f};draw.matrices.world[12]=actorX;meshes.Draw(actor,texture,draw);draw.matrices.world=identity;}
            modernFrame->End();waterTestSeconds=time;
            if(waterVisible)water.Draw(surface,4,{},waterDraw,WorldPart::Water);
            modernFrame->FinishWater();modernFrame->FinishWorld();
            std::vector<uint8_t> pixels;Check(backend.CaptureRGB(pixels,width,height),"readback");backend.EndFrame();backend.Present();meshes.ReleaseBindings();water.ReleaseBindings();Check(!meshes.Failed()&&!water.Failed(),"draw success");return pixels;
        };
        render(2,10);std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        std::array<std::vector<uint8_t>,4> qualities;
        for(unsigned q=0;q<4;++q){render(q,10);std::this_thread::sleep_for(std::chrono::milliseconds(q>=2?1100:1));qualities[q]=render(q,10);const auto name="g7-quality-"+std::to_string(q)+".ppm";Save(name.c_str(),qualities[q],width,height);}
        waterTestView=1;Save("g7-ssr-radiance.ppm",render(3,10),width,height);waterTestView=2;Save("g7-ssr-confidence.ppm",render(3,10),width,height);
        waterTestView=3;Save("g7-ssr-intersection.ppm",render(3,10),width,height);waterTestView=4;Save("g7-ssr-roughness.ppm",render(3,10),width,height);waterTestView=0;
        std::cout<<"SSR frames="<<modernFrame->Stats().ssrFrames<<" fallback="<<modernFrame->Stats().ssrFallbacks<<" difference="<<Difference(qualities[1],qualities[2])<<'\n';
        Check(Difference(qualities[0],qualities[1])>10000,"Medium refraction/depth is visible");
        Check(Difference(qualities[1],qualities[2])>1000,"Diligent SSR contributes visible reflection");
        Check(Difference(qualities[2],qualities[3])>100,"SSR quality has a real image effect");
        auto moving=render(1,12);Save("g7-moving-normals.ppm",moving,width,height);Check(Difference(moving,qualities[1])>1000,"time changes normal reflections");
        auto repeat=render(1,10);Save("g7-repeat.ppm",repeat,width,height);
        std::cout<<"fixedTimeDifference="<<Difference(repeat,qualities[1])<<'\n';
        waterTestReadback=true;render(1,10);waterTestReadback=false;
        std::cout<<"opaqueHDRPeak="<<waterTestInputPeak<<" waterHDRPeak="<<waterTestOutputPeak<<" finite="<<waterTestFinite<<'\n';
        Check(waterTestFinite&&waterTestOutputPeak>2&&waterTestOutputPeak>waterTestInputPeak*2,"actual FP16 water sun glint exceeds HDR one");
        Check(SameQuantizedImage(repeat,qualities[1]),"fixed time water matches within isolated 8-bit rounding");
        auto bare=render(1,10,true,0,false);Save("g7-opaque-reference.ppm",bare,width,height);
        // All pixels outside the water stay byte-identical (sky and dry object tops).
        Check(Difference(bare,repeat)>10000,"water visibly changes the scene");
        for(unsigned i=0;i<width*80*3;++i)Check(bare[i]==repeat[i],"water does not tint sky or dry world");
        light.sunDirection={-.5f,-.8f,-.2f};auto morning=render(1,10);Save("g7-low-sun.ppm",morning,width,height);
        light.sunDirection={.5f,-.8f,-.2f};auto evening=render(1,10);Save("g7-evening-sun.ppm",evening,width,height);Check(Difference(morning,evening)>10000,"shared sun moves water highlight");
        light.sunDirection={0,0,-1};auto noon=render(1,10);Save("g7-high-sun.ppm",noon,width,height);
        light.sunDirection={0,-.7f,-.7f};settings.bloom=true;auto bloom=render(1,10);Save("g7-bloom-on.ppm",bloom,width,height);settings.bloom=false;
        render(2,10);std::this_thread::sleep_for(std::chrono::milliseconds(1100));
        auto actorMoved=render(2,10,true,350);Save("g7-actor-moved.ppm",actorMoved,width,height);
        render(2,10,false);auto cleared=render(2,10,false);Save("g7-reflections-cleared.ppm",cleared,width,height);
        auto clearedAgain=render(2,10,false);Save("g7-cleared-repeat.ppm",clearedAgain,width,height);
        std::cout<<"clearedDifference="<<Difference(cleared,clearedAgain)<<'\n';
        Check(SameQuantizedImage(cleared,clearedAgain),"no reflection history after moving/removing objects");
        const auto resources=modernFrame->Stats().waterResourceCreations;
        for(unsigned i=0;i<8;++i)render(2,10+i*.016);
        Check(modernFrame->Stats().waterResourceCreations==resources,"no per-frame water resource creation");
        auto stats=modernFrame->Stats();Check(stats.waterFrames>10&&stats.ssrFrames>10&&stats.ssrFallbacks==0,"real water and SSR execution");
        std::cout<<"waterFrames="<<stats.waterFrames<<" ssrFrames="<<stats.ssrFrames<<" waterCpuMs="<<stats.waterSubmitMilliseconds<<" ssrCpuMs="<<stats.ssrSubmitMilliseconds<<" waterBytes="<<stats.waterTargetBytes<<'\n';
        Check(backend.Resize(320,240),"resize");render(3,10);Check(backend.Resize(0,0),"minimize");Check(!backend.BeginFrame(),"suspended");Check(backend.Resize(640,480),"restore");render(2,10);
        for(unsigned i=0;i<2;++i){ApplyGraphicsRuntimeConfig(Graphics::Resolve(Graphics::GraphicsSettings{},++revision));Check(backend.BeginFrame(),"Classic");Check(liveWaterRenderers==0,"style releases water owner");backend.EndFrame();backend.Present();render(1,10);}
        waterTestSeconds=-1;
    }
    backend.Shutdown();Check(liveWaterRenderers==0&&liveModernRenderers==0&&errors==0,"clean shutdown and Diligent diagnostics");
    std::cout<<"PASS G7 water quality, normals, shared sun, SSR, depth/refraction, no world tint, no per-frame resources, resize/restore/style; errors=0 resources=0\n";return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}}
