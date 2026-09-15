#include "Renderer/DiligentD3D11Backend.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/ModernFrame.h"
#include "Platform/PlatformWindow.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace Renderer;
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
constexpr std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
StaticObjectSource Sphere()
{
    StaticObjectSource source;
    constexpr unsigned rings=20,sectors=32;
    for(unsigned row=0;row<=rings;++row)for(unsigned col=0;col<=sectors;++col) {
        float theta=float(row)*3.14159265f/rings,phi=float(col)*6.2831853f/sectors;
        float x=std::sin(theta)*std::cos(phi),y=std::cos(theta),z=std::sin(theta)*std::sin(phi);
        source.vertices.push_back({x,y,z,x,y,z,float(col)/sectors,float(row)/rings});
        source.tangents.push_back({-std::sin(phi),0,std::cos(phi),1});
    }
    for(unsigned row=0;row<rings;++row)for(unsigned col=0;col<sectors;++col) {
        auto a=uint16_t(row*(sectors+1)+col),b=uint16_t(a+sectors+1);
        source.indices.insert(source.indices.end(),{a,uint16_t(a+1),b,uint16_t(a+1),uint16_t(b+1),b});
    }
    return source;
}
int main()
{
    try {
        Platform::PlatformWindow window;Platform::WindowCreateInfo info;info.title="G-DX material fixtures";
        Check(window.Create(info),"window");window.SetSize(1000,650);
        DiligentD3D11Backend backend;Check(backend.Initialize({window.GetNativeHandle().value,1000,650}),"backend");
        {
            DiligentStaticObjectRenderer renderer(backend);Check(renderer.Initialize(false),"mesh renderer");
            const auto source=Sphere();auto sphere=renderer.UploadGeometry(source);Check(bool(sphere),"sphere geometry");
            auto texture=[&](std::array<uint8_t,4> color,bool checker=false) {
                std::array<uint8_t,8*8*4> pixels;
                for(unsigned i=0;i<64;++i)for(unsigned c=0;c<4;++c)pixels[i*4+c]=checker&&c==3?((i/8+i%8)%2?255:0):color[c];
                TerrainTextureData image;image.width=image.height=8;image.format=TerrainTextureFormat::RGBA8;image.mips.push_back({pixels.data(),pixels.size(),32});
                auto result=renderer.UploadTexture(image);Check(bool(result),"fixture texture");return result;
            };
            auto white=texture({255,255,255,255}),normal=texture({205,100,222,255}),occlusion=texture({0,0,0,255}),mask=texture({255,255,255,255},true);
            Graphics::GraphicsRuntimeConfig config;config.revision=1;config.style=Graphics::GraphicsStyle::Modern;
            config.shadows=Graphics::ShadowQuality::High;config.ambientOcclusion=Graphics::AmbientOcclusionQuality::GTAO;ApplyGraphicsRuntimeConfig(config);
            Check(backend.BeginFrame(),"frame");backend.Clear({true,ClearColor{.04f,.05f,.07f,1}});
            Graphics::SceneLighting light;light.sunDirection={-.6f,-.8f,-1};light.sunIntensity=2.5f;light.ambient={.35f,.35f,.35f};light.environmentColor={.6f,.6f,.6f};
            modernFrame->Begin(light);
            StaticObjectDraw draw;draw.matrices.world=draw.matrices.view=identity;draw.matrices.view[14]=-11;
            draw.matrices.projection={1.78f,0,0,0,0,2.74f,0,0,0,0,-50.f/49.9f,-1,0,0,-5.f/49.9f,0};
            draw.normalTransform=identity;draw.cull=StaticObjectCull::None;draw.indexCount=unsigned(source.indices.size());draw.vertexCount=unsigned(source.vertices.size());
            for(unsigned i=0;i<12;++i) {
                draw.matrices.world=identity;draw.matrices.world[0]=draw.matrices.world[5]=draw.matrices.world[10]=.72f;
                draw.matrices.world[12]=(float(i%4)-1.5f)*2.6f;draw.matrices.world[13]=(1-float(i/4))*2.4f;
                auto material=std::make_shared<MaterialRuntimeData>();material->baseColor={.55f,.19f,.07f,1};
                material->roughness=i==1||i==3?.08f:.85f;material->metallic=i==2||i==3?1.f:0.f;
                if(i==4||i==11){material->textures[1]=normal;material->normalScale=i==11?0.f:1.f;}
                if(i==5)material->textures[4]=occlusion;
                if(i==6)material->emissive={.02f,.55f,.06f};
                if(i==9)draw.matrices.world[0]=-.72f;
                if(i==10){draw.matrices.world[0]=1.f;draw.matrices.world[5]=.45f;draw.matrices.world[1]=.25f;}
                draw.alphaTest=i==7?StaticObjectAlphaTest::GreaterEqual:StaticObjectAlphaTest::Disabled;draw.alphaReference=128;
                draw.material=material;renderer.Draw(sphere,i==7?mask:white,draw);
            }
            modernFrame->End();Check(!renderer.Failed(),"all material draws");
            std::vector<uint8_t> pixels;uint32_t width{},height{};Check(backend.CaptureRGB(pixels,width,height),"material readback");
            std::ofstream out("gdx-material-fixtures.ppm",std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());Check(bool(out),"material image");
            const auto sample=[&](unsigned col,unsigned row){const unsigned x=unsigned((float(col)-1.5f)*2.6f*1.78f/11.f*500.f+500.f),y=unsigned(325.f-(1-float(row))*2.4f*2.74f/11.f*325.f);return pixels[(y*width+x)*3+1];};
            Check(sample(2,1)>sample(1,1),"emissive material stays bright with AO");
            Check(modernFrame->Stats().lightBufferCreations==1&&modernFrame->Stats().meshDraws==12,"material fixtures share lighting and renderer");
            backend.EndFrame();backend.Present();renderer.ReleaseBindings();
            std::cout<<"PASS 12 native material fixtures; gdx-material-fixtures.ppm\n";
        }
        backend.Shutdown();Check(liveModernRenderers==0,"Modern owner released");window.Destroy();return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
