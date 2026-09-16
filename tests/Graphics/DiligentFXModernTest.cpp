#include "Renderer/DiligentD3D11Backend.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentTerrainRenderer.h"
#include "Renderer/ModernFrame.h"
#include "Renderer/FirstUseAudit.h"
#include "Platform/PlatformWindow.h"
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <numeric>
#include <fstream>

void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main()
{
    try {
        Platform::PlatformWindow window;Platform::WindowCreateInfo info;info.title="G-DX FX adapter smoke";
        Check(window.Create(info),"window");window.SetSize(128,128);
        Renderer::DiligentD3D11Backend backend;Check(backend.Initialize({window.GetNativeHandle().value,128,128}),"D3D11");
        {
            Renderer::DiligentStaticObjectRenderer meshRenderer(backend);Check(meshRenderer.Initialize(false),"mesh frontend");
            Renderer::DiligentTerrainRenderer terrainRenderer(backend);Check(terrainRenderer.Initialize(),"terrain frontend");
            Graphics::GraphicsRuntimeConfig config;config.revision=1;config.style=Graphics::GraphicsStyle::Modern;
            Renderer::ApplyGraphicsRuntimeConfig(config);
            Check(backend.BeginFrame(),"begin");backend.Clear({true,Renderer::ClearColor{.08f,.16f,.28f,1}});
            Graphics::SceneLighting light;light.sunDirection={0,0,1};light.sunIntensity=1;light.ambient={.2f,.2f,.2f};
            Check(Renderer::modernFrame!=nullptr,"modern frame owner");
            Renderer::StaticObjectSource source;source.vertices={{{-.8f,-.8f,-1.f,0,0,1,0,0}},{{.8f,-.8f,-1.f,0,0,1,1,0}},{{0,.8f,-1.f,0,0,1,.5f,1}}};source.indices={0,1,2};
            source.tangents={{{1,0,0,1}},{{1,0,0,1}},{{1,0,0,1}}};
            auto geometry=meshRenderer.UploadGeometry(source);Check(bool(geometry),"geometry");
            const std::array<unsigned char,4> white{255,255,255,255};
            Renderer::TerrainTextureData image;image.width=image.height=1;image.format=Renderer::TerrainTextureFormat::RGBA8;image.mips.push_back({white.data(),4,4});
            auto texture=meshRenderer.UploadTexture(image);Check(bool(texture),"texture");
            Renderer::StaticObjectDraw draw;
            const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
            draw.matrices.world=draw.matrices.view=draw.matrices.projection=identity;draw.normalTransform=identity;
            // Same RH perspective convention as the production game camera.
            draw.matrices.projection={1,0,0,0,0,1,0,0,0,0,-100.f/99.9f,-1,0,0,-10.f/99.9f,0};
            Renderer::loadingPrewarm=true;
            light.sunDirection={0,0,-1};Renderer::modernFrame->Begin(light);
            Check(Renderer::modernFrame->Stats().meshShaderVariants==24,"18 existing and six vegetation mesh shader variants compiled during loading");
            Check(Renderer::modernFrame->Stats().terrainShaderVariants==4,"all terrain shader variants compiled during loading");
            draw.indexCount=draw.vertexCount=3;draw.cull=Renderer::StaticObjectCull::None;
            meshRenderer.Draw(geometry,texture,draw);Check(!meshRenderer.Failed(),"FX mesh shader/draw");
            Renderer::modernFrame->End();
            Renderer::loadingPrewarm=false;
            std::vector<uint8_t> pixels;uint32_t width{},height{};Check(backend.CaptureRGB(pixels,width,height),"readback");
            const auto center=(height/2*width+width/2)*3;
            std::cout<<"center="<<unsigned(pixels[center])<<','<<unsigned(pixels[center+1])<<','<<unsigned(pixels[center+2])<<" background="<<unsigned(pixels[0])<<'\n';
            Check(pixels[center]>150,"FX PBR direct sun and ambient produce lit center pixel");
            Check(pixels[0]<pixels[center],"new atmosphere remains below the lit material");
            backend.EndFrame();backend.Present();meshRenderer.ReleaseBindings();
            std::array<std::array<float,6>,289> terrainVertices{};
            for(unsigned y=0;y<17;++y)for(unsigned x=0;x<17;++x)terrainVertices[y*17+x]={float(x)/4-2,float(y)/4-2,-2,0,0,1};
            const uint16_t terrainIndices[]{0,16,288,0,288,272};
            auto terrainVB=terrainRenderer.UploadVertices(terrainVertices.data(),289,24);
            auto terrainIB=terrainRenderer.UploadIndices(terrainIndices,6);
            auto terrainTexture=terrainRenderer.UploadTexture(image);
            image.format=Renderer::TerrainTextureFormat::Alpha8;image.mips[0]={white.data(),1,1};
            auto mask=terrainRenderer.UploadTexture(image);auto splat=terrainRenderer.CreateSplatMaterial(terrainTexture,mask);
            Check(terrainVB&&terrainIB&&splat,"terrain fixture resources");
            for(auto quality:{Graphics::AmbientOcclusionQuality::SSAO,Graphics::AmbientOcclusionQuality::GTAO}) {
                config.ambientOcclusion=quality;config.shadows=Graphics::ShadowQuality::High;++config.revision;
                Renderer::ApplyGraphicsRuntimeConfig(config);Check(backend.BeginFrame(),"effects begin");
                backend.Clear({true,Renderer::ClearColor{.08f,.16f,.28f,1}});Renderer::modernFrame->Begin(light);
                terrainRenderer.BeginTerrain(draw.matrices,true);
                Renderer::TerrainSplatParameters splatParameters;splatParameters.blend=false;
                terrainRenderer.DrawSplat(terrainVB,terrainIB,6,false,splat,splatParameters);
                Check(!terrainRenderer.Failed(),"FX terrain draw");
                for(unsigned actor=0;actor<20;++actor)meshRenderer.Draw(geometry,texture,draw);
                Renderer::modernFrame->End();Check(backend.CaptureRGB(pixels,width,height),"effects readback");
                Check(pixels[center]>130,"shadow and AO scene remains lit");
                backend.EndFrame();backend.Present();meshRenderer.ReleaseBindings();
            }
            const auto stats=Renderer::modernFrame->Stats();
            Check(stats.lightBufferCreations==1&&stats.lightUploads==3,"one shared light buffer and one upload per rendered scene");
            Check(stats.meshDraws==41&&stats.terrainDraws==2&&stats.shadowDraws==126,"all geometry uses shared color and cascade passes");
            // Upstream SSAO fades in over one second after pipeline creation.
            // Wait once, then compare actual contact shading rather than the
            // initial neutral image or merely successful resource allocation.
            std::this_thread::sleep_for(std::chrono::milliseconds(1100));
            auto contact=[&](Graphics::AmbientOcclusionQuality quality,const Graphics::SceneLighting& illumination,Graphics::ShadowQuality shadows=Graphics::ShadowQuality::Off) {
                config.ambientOcclusion=quality;config.shadows=shadows;++config.revision;
                Renderer::ApplyGraphicsRuntimeConfig(config);Check(backend.BeginFrame(),"AO semantic frame");
                backend.Clear({true,Renderer::ClearColor{.08f,.16f,.28f,1}});Renderer::modernFrame->Begin(illumination);
                terrainRenderer.BeginTerrain(draw.matrices,true);Renderer::TerrainSplatParameters parameters;parameters.blend=false;
                terrainRenderer.DrawSplat(terrainVB,terrainIB,6,false,splat,parameters);meshRenderer.Draw(geometry,texture,draw);
                Renderer::modernFrame->End();std::vector<uint8_t> result;Check(backend.CaptureRGB(result,width,height),"AO semantic readback");
                backend.EndFrame();backend.Present();meshRenderer.ReleaseBindings();return result;
            };
            auto indirect=light;indirect.sunIntensity=0;indirect.ambient={.6f,.6f,.6f};
            const auto noAO=contact(Graphics::AmbientOcclusionQuality::Off,indirect);
            for(auto quality:{Graphics::AmbientOcclusionQuality::SSAO,Graphics::AmbientOcclusionQuality::GTAO}) {
                const auto withAO=contact(quality,indirect);long long reduction=0;
                for(size_t i=0;i<noAO.size();++i)reduction+=int(noAO[i])-int(withAO[i]);
                Check(reduction>100,"real SSAO darkens indirect contact shading");
            }
            auto direct=light;direct.ambient={0,0,0};
            Check(contact(Graphics::AmbientOcclusionQuality::Off,direct)==contact(Graphics::AmbientOcclusionQuality::GTAO,direct),"screen AO leaves direct sunlight byte-identical");
            auto emissive=std::make_shared<Renderer::MaterialRuntimeData>();emissive->emissive={.2f,.5f,.1f};draw.material=emissive;
            direct.sunIntensity=0;
            Check(contact(Graphics::AmbientOcclusionQuality::Off,direct)==contact(Graphics::AmbientOcclusionQuality::GTAO,direct),"screen AO leaves emission byte-identical");
            draw.material.reset();
            draw.actorStage=Renderer::ActorMaterialStage::Add;draw.textureFactor={.7f,0,0,1};
            const auto hit=contact(Graphics::AmbientOcclusionQuality::Off,direct);
            Check(hit[center]>150&&hit[center+1]==0,"game additive hit tint remains visible without scene light");
            draw.actorStage=Renderer::ActorMaterialStage::Modulate;draw.textureFactor={0,1,0,1};
            const auto tint=contact(Graphics::AmbientOcclusionQuality::Off,light);
            Check(tint[center]==0&&tint[center+1]>150&&tint[center+2]==0,"game modulate tint colors the lit actor");
            draw.actorStage=Renderer::ActorMaterialStage::None;draw.textureFactor={1,1,1,1};
            std::array<double,4> shadowCenter{};
            const std::array<std::array<float,3>,4> sunDirections{{{-.7f,0,-1},{.7f,0,-1},{0,-.2f,-1},{0,-1,-.25f}}};
            for(unsigned direction=0;direction<sunDirections.size();++direction) {
                auto sun=light;sun.sunDirection=sunDirections[direction];sun.ambient={0,0,0};
                const auto unshadowed=contact(Graphics::AmbientOcclusionQuality::Off,sun);
                const auto shadowed=contact(Graphics::AmbientOcclusionQuality::Off,sun,Graphics::ShadowQuality::High);
                double darkening=0,weightedX=0;
                for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x) {
                    const auto index=(y*width+x)*3;
                    const int difference=std::max(0,int(unshadowed[index])-int(shadowed[index]));
                    darkening+=difference;weightedX+=x*difference;
                }
                std::cout<<"sun="<<direction<<" receiverDarkening="<<darkening<<'\n';
                Check(darkening>100,"each left/right/high/low sun casts a measurable receiver shadow");
                shadowCenter[direction]=weightedX/darkening;
            }
            Check(shadowCenter[1]>shadowCenter[0]+3,"shadow moves with the shared sun from left to right");
            // The caster is entirely outside the RH camera: x >= 1.3 at z=-1,
            // while the camera spans only [-1,1] there. Its projected shadow
            // reaches the visible receiver at z=-2.
            auto offCamera=[&](bool includeCaster) {
                config.ambientOcclusion=Graphics::AmbientOcclusionQuality::Off;
                config.shadows=Graphics::ShadowQuality::High;++config.revision;
                Renderer::ApplyGraphicsRuntimeConfig(config);Check(backend.BeginFrame(),"off-camera frame");
                backend.Clear({true,Renderer::ClearColor{.08f,.16f,.28f,1}});
                auto sun=light;sun.sunDirection={-1,0,-1};sun.ambient={0,0,0};
                Renderer::modernFrame->Begin(sun);
                terrainRenderer.BeginTerrain(draw.matrices,true);Renderer::TerrainSplatParameters parameters;parameters.blend=false;
                terrainRenderer.DrawSplat(terrainVB,terrainIB,6,false,splat,parameters);
                Check(Renderer::modernFrame->BeginShadowCollection(),"light caster collection");
                Check(Renderer::modernFrame->ShadowCasterVisible({1.5f,0,-1},.3f),"off-camera caster intersects light cascade");
                Check(!Renderer::modernFrame->ShadowCasterVisible({1000000,0,-1},.3f),"distant caster culled by light cascades");
                if(includeCaster) {
                    auto caster=draw;caster.matrices.world={.25f,0,0,0,0,.25f,0,0,0,0,.25f,0,1.5f,0,-.75f,1};
                    meshRenderer.Draw(geometry,texture,caster);
                }
                Renderer::modernFrame->EndShadowCollection();Renderer::modernFrame->End();
                std::vector<uint8_t> result;Check(backend.CaptureRGB(result,width,height),"off-camera readback");
                backend.EndFrame();backend.Present();meshRenderer.ReleaseBindings();return result;
            };
            const auto withoutCaster=offCamera(false),withCaster=offCamera(true);
            long long outsideDarkening=0;
            for(size_t i=0;i<withCaster.size();++i)outsideDarkening+=std::max(0,int(withoutCaster[i])-int(withCaster[i]));
            Check(outsideDarkening>1000,"off-camera caster produces a visible receiver shadow");
            Check(!Renderer::shadowCasterCollection,"camera visibility restored after collection");
            Check(Renderer::modernFrame->Stats().meshShaderVariants==24,"no new mesh shader compilation in subsequent world frames");
            Check(Renderer::modernFrame->Stats().terrainShaderVariants==4,"no new terrain shader compilation in subsequent world frames");
            std::cout<<"offCameraReceiverDarkening="<<outsideDarkening<<'\n';
            for(bool present:{false,true}) {
                const auto& result=present?withCaster:withoutCaster;
                std::ofstream file(present?"gdxc-off-camera-shadow.ppm":"gdxc-off-camera-reference.ppm",std::ios::binary);
                file<<"P6\n"<<width<<' '<<height<<"\n255\n";file.write(reinterpret_cast<const char*>(result.data()),result.size());
            }
            window.Minimize();Check(backend.Resize(0,0)&&!backend.BeginFrame(),"suspend Modern frame");
            window.Restore();Check(backend.Resize(192,144),"resize Modern resources");
            Check(backend.BeginFrame(),"Modern after restore");backend.Clear({true,Renderer::ClearColor{.08f,.16f,.28f,1}});
            Renderer::modernFrame->Begin(light);draw.blend=true;draw.depthWrite=false;
            auto transparent=std::make_shared<Renderer::MaterialRuntimeData>();transparent->baseColor[3]=.5f;draw.material=transparent;
            meshRenderer.Draw(geometry,texture,draw);Renderer::modernFrame->End();
            Check(backend.CaptureRGB(pixels,width,height),"transparent after resize");
            Check(pixels[(height/2*width+width/2)*3]>65,"forward transparent material visible against background");
            backend.EndFrame();backend.Present();meshRenderer.ReleaseBindings();
            config.style=Graphics::GraphicsStyle::Classic;++config.revision;Renderer::ApplyGraphicsRuntimeConfig(config);
            Check(backend.BeginFrame()&&Renderer::modernFrame==nullptr&&Renderer::liveModernRenderers==0,"Classic toggle releases FX owner");
            backend.EndFrame();backend.Present();
            std::cout<<"PASS FX shader, shared material path and direct/indirect composition\n";
        }
        backend.Shutdown();window.Destroy();return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
