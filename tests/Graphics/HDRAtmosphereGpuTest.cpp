#include "Renderer/DiligentD3D11Backend.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentEffectRenderer.h"
#include "Renderer/ModernFrame.h"
#include "Renderer/FirstUseAudit.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Platform/PlatformWindow.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <iostream>
#include <fstream>
#include <stdexcept>

using namespace Renderer;
namespace {
std::atomic<unsigned> errors{};
void DILIGENT_CALL_TYPE Message(Diligent::DEBUG_MESSAGE_SEVERITY severity,const char* message,const char*,const char*,int) {
    if(severity>=Diligent::DEBUG_MESSAGE_SEVERITY_ERROR){++errors;std::cerr<<message<<'\n';}
}
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
constexpr std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
void Save(const char* name,const std::vector<uint8_t>& image,unsigned width,unsigned height) {
    std::ofstream file(name,std::ios::binary);file<<"P6\n"<<width<<' '<<height<<"\n255\n";
    file.write(reinterpret_cast<const char*>(image.data()),image.size());Check(bool(file),"save G56 proof");
}
}
int main() {
    try {
        Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        Platform::PlatformWindow window;Platform::WindowCreateInfo info;info.title="G56 HDR atmosphere proof";
        Check(window.Create(info),"window");window.SetSize(512,384);
        DiligentD3D11Backend backend;Check(backend.Initialize({window.GetNativeHandle().value,512,384}),"D3D11");
        {
            DiligentStaticObjectRenderer meshes(backend);Check(meshes.Initialize(false),"mesh frontend");
            DiligentEffectRenderer effects(backend);Check(effects.Initialize(),"world/UI effect frontend");
            StaticObjectSource source;
            source.vertices={{{-.3f,3,-.3f,0,-1,0,0,0}},{{.3f,3,-.3f,0,-1,0,1,0}},{{.3f,3,.3f,0,-1,0,1,1}},{{-.3f,3,.3f,0,-1,0,0,1}}};
            source.indices={0,1,2,0,2,3};auto geometry=meshes.UploadGeometry(source);
            const std::array<unsigned char,4> white{255,255,255,255};
            TerrainTextureData image;image.width=image.height=1;image.format=TerrainTextureFormat::RGBA8;image.mips.push_back({white.data(),4,4});
            auto texture=meshes.UploadTexture(image);Check(geometry&&texture,"fixture assets");
            StaticObjectDraw draw;draw.matrices.world=identity;draw.normalTransform=identity;
            draw.matrices.view={1,0,0,0,0,0,-1,0,0,1,0,0,0,0,0,1};
            draw.matrices.projection={1.299038f,0,0,0,0,1.732051f,0,0,0,0,-100.f/99.9f,-1,0,0,-10.f/99.9f,0};
            draw.indexCount=6;draw.vertexCount=4;draw.cull=StaticObjectCull::None;
            auto material=std::make_shared<MaterialRuntimeData>();material->baseColor={0,0,0,1};draw.material=material;
            auto settings=Graphics::PresetSettings(Graphics::GraphicsPreset::High,Graphics::GraphicsStyle::Modern);
            settings.shadows=Graphics::ShadowQuality::Off;settings.ambientOcclusion=Graphics::AmbientOcclusionQuality::Off;
            Graphics::SceneLighting light;light.sunIntensity=0;light.ambient={0,0,0};
            unsigned revision=0,width=512,height=384;
            const EffectVertex overlay[]{{{-.95f,-.9f,.1f},0xffd06020,{}},{{-.55f,-.9f,.1f},0xffd06020,{}},{{-.95f,-.65f,.1f},0xffd06020,{}},{{-.55f,-.65f,.1f},0xffd06020,{}}};
            auto render=[&](float emission,bool bloom,float exposure=0,bool geometryVisible=true,bool withUI=false) {
                settings.bloom=bloom;ApplyGraphicsRuntimeConfig(Graphics::Resolve(settings,++revision));
                Check(backend.BeginFrame(),"frame begin");backend.Clear({true,ClearColor{0,0,0,1}});
                light.exposureBias=exposure;material->emissive={emission,emission,emission};
                modernFrame->Begin(light,true);modernFrame->SetCamera(draw.matrices);
                if(geometryVisible)meshes.Draw(geometry,texture,draw);
                modernFrame->End();
                if(withUI) {
                    // Same authored colour before tone mapping: world particles.
                    EffectDraw effect;effect.matrices.world=effect.matrices.view=effect.matrices.projection=identity;
                    effect.textured=false;effect.depthTest=false;effect.blend=false;effect.colorOp=effect.alphaOp=1;
                    effects.Draw(overlay,4,{},effect,EffectPart::Particle);
                }
                modernFrame->FinishWorld();
                if(withUI) {
                    auto ui=std::array<EffectVertex,4>{overlay[0],overlay[1],overlay[2],overlay[3]};
                    for(auto& vertex:ui)vertex.position[0]+=1.3f;
                    EffectDraw effect;effect.matrices.world=effect.matrices.view=effect.matrices.projection=identity;
                    effect.textured=false;effect.depthTest=false;effect.blend=false;effect.colorOp=effect.alphaOp=1;
                    effect.ui=true;effect.viewport={0,0,width,height};effects.Draw(ui.data(),4,{},effect,EffectPart::Particle);
                }
                std::vector<uint8_t> pixels;Check(backend.CaptureRGB(pixels,width,height),"readback");
                Check(!meshes.Failed()&&!effects.Failed(),"world and UI draws");
                backend.EndFrame();backend.Present();meshes.ReleaseBindings();effects.ReleaseBindings();return pixels;
            };
            Renderer::loadingPrewarm=true;render(.5f,true);Renderer::loadingPrewarm=false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1100)); // pinned FX Bloom initial fade
            const auto two=render(2,false),eight=render(8,false);
            const unsigned center=(height/2*width+width/2)*3;
            Check(eight[center]>two[center]+12&&eight[center]<255,"HDR values above one remain distinct after highlight roll-off");
            const auto whiteOff=render(.5f,false),whiteOn=render(.5f,true);
            Check(whiteOff==whiteOn,"ordinary white below threshold produces exactly zero bloom");
            const auto glow=render(8,true);
            long long halo=0;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)
                if(x<220||x>292||y<156||y>228)halo+=std::max(0,int(glow[(y*width+x)*3])-int(eight[(y*width+x)*3]));
            Check(halo>100,"HDR emissive produces measurable surrounding bloom");
            Save("g56-emissive-off.ppm",eight,width,height);Save("g56-emissive-on.ppm",glow,width,height);
            const auto uiDark=render(.5f,false,-1,true,true),uiBright=render(.5f,true,1,true,true);
            const unsigned uiPixel=(unsigned(height*.9f)*width+unsigned(width*.8f))*3;
            const unsigned worldPixel=(unsigned(height*.9f)*width+unsigned(width*.15f))*3;
            Check(uiDark[uiPixel]==208&&uiDark[uiPixel+1]==96&&uiDark[uiPixel+2]==32,"UI retains exact authored colour");
            for(unsigned c=0;c<3;++c)Check(uiDark[uiPixel+c]==uiBright[uiPixel+c],"UI bypasses exposure and bloom");
            Check(uiBright[worldPixel]>uiDark[worldPixel]+15,"world effects receive exposure before UI");
            Save("g56-ui-dark.ppm",uiDark,width,height);Save("g56-ui-bright.ppm",uiBright,width,height);
            std::array<double,3> diskX{},diskY{};
            const std::array<std::array<float,3>,3> rays{{{-.3f,-1,-.2f},{.3f,-1,-.2f},{0,-1,-.48f}}};
            for(unsigned state=0;state<3;++state) {
                light.sunDirection=rays[state];light.sunIntensity=3.14159265f;
                const auto sky=render(0,false,0,false);
                unsigned count=0;double sumX=0,sumY=0;
                for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                    const auto p=(y*width+x)*3;
                    if(sky[p]>245&&sky[p+1]>245&&sky[p+2]>245){++count;sumX+=x;sumY+=y;}
                }
                Check(count>=2&&count<150,"one compact HDR sun disk");diskX[state]=sumX/count;diskY[state]=sumY/count;
                const double expectedX=width*.5*(1-draw.matrices.projection[0]*rays[state][0]);
                const double expectedY=height*.5*(1+draw.matrices.projection[5]*rays[state][2]);
                Check(std::abs(diskX[state]-expectedX)<3&&std::abs(diskY[state]-expectedY)<3,"disk projects opposite the shared sunlight rays");
                const char* file=state==0?"g56-sun-right.ppm":state==1?"g56-sun-left.ppm":"g56-sun-high.ppm";
                Save(file,sky,width,height);
            }
            Check(diskX[0]>diskX[1]+100&&diskY[2]<diskY[0]-60,"left/right/elevation follow SceneLighting");
            // Put an opaque world quad in front of the high sun direction.
            draw.matrices.world[14]=1.44f;
            const auto occluded=render(0,false);
            Check(occluded[(unsigned(diskY[2])*width+unsigned(diskX[2]))*3]<200,"world depth occludes sun disk");
            draw.matrices.world=identity;
            material->baseColor={.7f,.4f,.2f,1};material->roughness=.5f;
            const auto noSkyIBL=render(0,false);
            light.skyIBLIntensity=.15f;const auto skyIBL=render(0,false);
            Check(skyIBL[center]>noSkyIBL[center],"FX sky convolution supplies directional PBR indirect light");
            Save("g56-sky-ibl.ppm",skyIBL,width,height);
            light.skyIBLIntensity=0;material->baseColor={0,0,0,1};
            const auto stats=modernFrame->Stats();Check(stats.hdrTargetBytes==512ull*384*8,"one FP16 HDR scene target");
            window.Minimize();Check(backend.Resize(0,0)&&!backend.BeginFrame(),"minimize suspends HDR");
            window.Restore();window.SetSize(320,240);Check(backend.Resize(320,240),"resize HDR");
            width=320;height=240;settings.modernSky=false;render(8,true);
            Check(modernFrame->Stats().hdrTargetBytes==320ull*240*8,"restored HDR attachments use new size");
            settings.style=Graphics::GraphicsStyle::Classic;ApplyGraphicsRuntimeConfig(Graphics::Resolve(settings,++revision));
            Check(backend.BeginFrame()&&!modernFrame&&liveModernRenderers==0,"Classic releases the HDR atmosphere owner");backend.EndFrame();backend.Present();
            settings.style=Graphics::GraphicsStyle::Modern;render(.5f,false);
            Check(modernFrame!=nullptr,"Modern can be reapplied live");
        }
        backend.Shutdown();Check(liveModernRenderers==0,"zero Modern resource owners");window.Destroy();
        Check(errors==0,"zero Diligent ERROR/FATAL diagnostics");
        std::cout<<"PASS HDR >1, tone mapping, bloom threshold, world/UI, sun projection/occlusion, quality, resize, style, lifetime; DiligentErrors=0\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
