#include "Renderer/DiligentD3D11Backend.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/AssetMaterialRenderData.h"
#include "Renderer/DiligentEffectRenderer.h"
#include "Renderer/ModernFrame.h"
#include "Renderer/FirstUseAudit.h"
#include "Graphics/AtmosphereConfig.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Platform/PlatformWindow.h"
#include <atomic>
#include <cmath>
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
            auto material=std::make_shared<MaterialRuntimeData>();material->model=AssetRuntime::MaterialModel::PBRMetallicRoughness;material->baseColor={0,0,0,1};draw.material=material;
            auto settings=Graphics::PresetSettings(Graphics::GraphicsPreset::High,Graphics::GraphicsStyle::Modern);
            settings.shadows=Graphics::ShadowQuality::Off;settings.ambientOcclusion=Graphics::AmbientOcclusionQuality::Off;
            Graphics::SceneLighting light;light.sunIntensity=0;light.ambient={0,0,0};
            unsigned revision=0,width=512,height=384;
            bool effectFog=false;
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
                    effect.fog=effectFog?1:0;effect.fogColor={.5f,.5f,.5f,1};effect.fogParameters={0,1,100,0};
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
            {
                // Additive authored aura intensity must match HDR emission, with
                // the original alpha fade and colour blend factor applied once.
                constexpr uint32_t auraColour=0x80102060;
                auto aura=std::array<EffectVertex,4>{overlay[0],overlay[1],overlay[2],overlay[3]};
                for(auto& vertex:aura) {
                    vertex.position[0]=(vertex.position[0]+.75f)*.5f;
                    vertex.position[1]=(vertex.position[1]+.775f)*.5f;
                }
                auto captureAura=[&](uint32_t alpha,uint32_t sourceBlend,bool reference) {
                    settings.bloom=false;ApplyGraphicsRuntimeConfig(Graphics::Resolve(settings,++revision));
                    Check(backend.BeginFrame(),"aura frame");backend.Clear({true,ClearColor{0,0,0,1}});
                    light.exposureBias=0;material->emissive={0,0,0};
                    if(reference)for(unsigned c=0;c<3;++c) {
                        const float intensity=float((auraColour>>(16-c*8))&255)/255.f;
                        material->emissive[c]=intensity*(sourceBlend==3?intensity:alpha/255.f);
                    }
                    modernFrame->Begin(light,true);modernFrame->SetCamera(draw.matrices);
                    meshes.Draw(geometry,texture,draw);modernFrame->End();
                    if(!reference) {
                        for(auto& vertex:aura)vertex.color=(auraColour&0xffffff)|(alpha<<24);
                        EffectDraw effect;effect.matrices.world=effect.matrices.view=effect.matrices.projection=identity;
                        effect.textured=false;effect.depthTest=false;effect.colorOp=effect.alphaOp=1;
                        effect.src=sourceBlend;effect.dst=2;
                        effects.Draw(aura.data(),4,{},effect,EffectPart::Particle);
                    }
                    modernFrame->FinishWorld();std::vector<uint8_t> pixels;
                    Check(backend.CaptureRGB(pixels,width,height),"aura readback");
                    backend.EndFrame();backend.Present();meshes.ReleaseBindings();effects.ReleaseBindings();return pixels;
                };
                for(uint32_t src:{5u,3u})for(uint32_t alpha:{0u,128u,255u}) {
                    const auto actual=captureAura(alpha,src,false),expected=captureAura(alpha,src,true);
                    for(unsigned c=0;c<3;++c)Check(std::abs(int(actual[center+c])-int(expected[center+c]))<=2,
                        "additive aura retains authored emission strength, alpha fade and source factor");
                    if(src==5&&alpha==128)Save("g56-authored-aura.ppm",actual,width,height);
                }
                std::cout<<"PASS additive blue aura emission, alpha fade and colour blend factor\n";
            }
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
            {
                const auto savedLight=light;
                light.cloudCoverage=.65f;Graphics::developmentSkySeconds=10;
                const auto clouds=render(0,false,0,false);
                Check(render(0,false,0,false)==clouds,"fixed sky time is repeatable");
                Graphics::developmentSkySeconds=130;
                Check(render(0,false,0,false)!=clouds,"clouds advance with time");
                Graphics::developmentSkySeconds=10;
                Check(render(0,false,0,false)==clouds,"cloud motion does not accumulate per frame");
                const auto withBloom=render(0,true,0,false);
                Save("g8-sky-clouds.ppm",clouds,width,height);
                Save("g8-sky-clouds-bloom.ppm",withBloom,width,height);
                light.fogColor={.15f,.08f,.05f};
                Check(render(0,false,0,false)!=clouds,"map horizon change invalidates cached sky without changing the sun");
                light=savedLight;Graphics::developmentSkySeconds=-1;
            }
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
            {
                const auto originalDraw=draw;
                AssetRuntime::MaterialAsset authoredPBR;authoredPBR.explicitRenderState=true;authoredPBR.alphaTest=false;
                authoredPBR.culling=AssetRuntime::Culling::None;authoredPBR.baseColorFactor=material->baseColor;
                ApplyAssetMaterial(authoredPBR,draw);
                Check(render(0,false)==skyIBL,"GLB base colour is applied once through the production material bridge");
                draw=originalDraw;
            }
            // Authored legacy texture colours under neutral illumination: no
            // additive dielectric/sky reflection and no distance fog veil.
            light={};light.ambient={.6f,.6f,.6f};light.sunDirection={0,1,0};light.sunIntensity=1.25663706f;
            material->model=AssetRuntime::MaterialModel::Legacy;material->baseColor={1,1,1,1};
            const auto originalTexture=texture;
            const std::array<std::array<uint8_t,4>,3> authored{{{55,135,35,255},{140,85,40,255},{45,80,160,255}}};
            auto linear=[](uint8_t c){const double x=c/255.0;return x<=.04045?x/12.92:std::pow((x+.055)/1.055,2.4);};
            for(unsigned swatch=0;swatch<authored.size();++swatch) {
                image.mips[0]={authored[swatch].data(),4,4};texture=meshes.UploadTexture(image);Check(bool(texture),"legacy colour texture");
                material->metallic=0;material->roughness=.85f;light.environmentColor={0,0,0};light.skyIBLIntensity=0;
                const auto diffuse=render(0,false);
                double originalSum=0,renderedSum=0;
                for(unsigned c=0;c<3;++c){originalSum+=linear(authored[swatch][c]);renderedSum+=linear(diffuse[center+c]);}
                Check(renderedSum>.05,"legacy shadow/midtone remains readable");
                for(unsigned c=0;c<3;++c)Check(std::abs(linear(authored[swatch][c])/originalSum-linear(diffuse[center+c])/renderedSum)<.02,
                    "green/brown/armour chromaticity survives lighting and tone mapping");
                material->metallic=1;material->roughness=.045f;light.environmentColor={1,1,1};light.skyIBLIntensity=1;
                Check(render(0,false)==diffuse,"legacy ignores unrequested metallic/gloss and reflected environment");
                light.fogEnabled=true;light.densityFog=true;light.fogNear=0;light.fogFar=1;light.fogDensity=100;
                Check(render(0,false)==diffuse,"opaque world does not receive environment distance fog");
                material->baseColor[3]=.5f;draw.blend=true;draw.depthWrite=false;
                const auto transparentFog=render(0,false);light.fogEnabled=false;
                Check(render(0,false)==transparentFog,"forward world does not receive environment distance fog");
                draw.blend=false;draw.depthWrite=true;material->baseColor[3]=1;
                material->model=AssetRuntime::MaterialModel::PBRMetallicRoughness;
                const auto metal=render(0,false);material->metallic=0;
                const auto dielectric=render(0,false);
                unsigned difference=0;for(unsigned c=0;c<3;++c)difference+=std::abs(int(metal[center+c])-int(dielectric[center+c]));
                Check(difference>8,"explicit PBR retains the metallic/roughness BRDF");
                material->model=AssetRuntime::MaterialModel::Legacy;
                const char* file=swatch==0?"g56-legacy-green.ppm":swatch==1?"g56-legacy-brown.ppm":"g56-legacy-armour.ppm";
                Save(file,diffuse,width,height);
            }
            texture=originalTexture;
            effectFog=false;const auto effectsClear=render(0,false,0,true,true);effectFog=true;
            Check(render(0,false,0,true,true)==effectsClear,"HDR particles ignore legacy environment fog while UI remains unchanged");
            effectFog=false;
            const auto materialStats=modernFrame->Stats();
            Check(materialStats.legacyMaterialDraws>0&&materialStats.pbrMaterialDraws>0,"both explicit material routes exercised");
            std::cout<<"PASS legacy green/brown/armour chroma, explicit PBR, opaque/forward/effect world without fog\n";
            {
                const auto savedDraw=draw;const auto savedLight=light;const auto savedMaterial=*material;
                light={};light.sunIntensity=0;light.ambient={.45f,.45f,.45f};light.skyIBLIntensity=0;
                material->model=AssetRuntime::MaterialModel::Legacy;material->baseColor={.2f,.12f,.06f,1};
                const auto plain=render(0,false);
                // Deterministic authored sphere map: a coloured shimmer band and
                // a black band. The original animated matrix selects either one.
                const uint8_t band[]{240,80,170,255,0,0,0,255};
                TerrainTextureData shimmer;shimmer.width=2;shimmer.height=1;shimmer.format=TerrainTextureFormat::RGBA8;
                shimmer.mips.push_back({band,8,8});draw.sphereMap=meshes.UploadTexture(shimmer);
                Check(bool(draw.sphereMap),"authored item shimmer texture");
                draw.actorStage=ActorMaterialStage::Specular;draw.factorAlpha=true;
                draw.cameraAlphaSampling={};draw.cameraAlphaSampling.linearMin=false;draw.cameraAlphaSampling.linearMag=false;
                draw.cameraAlphaSampling.wrapU=true;draw.cameraAlphaSampling.wrapV=true;
                draw.cameraAlphaTransform=identity;draw.cameraAlphaTransform[12]=.25f;
                draw.textureFactor={1,1,1,0};
                Check(render(0,false)==plain,"zero item specular power preserves original diffuse");
                draw.textureFactor[3]=.45f;const auto shimmerOn=render(0,false);
                Check(shimmerOn[center]>plain[center]+20&&shimmerOn[center+2]>plain[center+2]+10,"authored armour/weapon shimmer remains visible in Modern");
                draw.textureFactor[3]=.9f;const auto stronger=render(0,false);
                Check(stronger[center]>shimmerOn[center]+10,"item specular power controls authored shimmer strength");
                draw.cameraAlphaTransform[12]=.75f;const auto shifted=render(0,false);
                Check(shifted==plain,"original animated sphere transform moves the shimmer band");
                Save("g56-authored-shimmer-off.ppm",plain,width,height);
                Save("g56-authored-shimmer-on.ppm",shimmerOn,width,height);
                Save("g56-authored-shimmer-shift.ppm",shifted,width,height);
                Check(modernFrame->Stats().authoredShimmerDraws>=4,"authored shimmer draw route exercised");
                draw=savedDraw;
                Check(render(0,false)==plain,"item shimmer does not leak into ordinary legacy draws");
                light=savedLight;*material=savedMaterial;
                std::cout<<"PASS authored item shimmer, strength, animated sphere transform, no effect on ordinary legacy diffuse\n";
            }
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
