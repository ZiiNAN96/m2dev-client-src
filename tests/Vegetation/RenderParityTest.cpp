// SDK-free production renderer against compact, independently captured Gate A2 goldens.
#include <cstddef>
#include <rapidjson/document.h>
#include "EterGrnLib/StdAfx.h"
#include "Vegetation/VegetationRenderer.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include "EterLib/Camera.h"
#include "EterLib/GrpImage.h"
#include "EterLib/ResourceManager.h"
#include "Renderer/ResourceData.h"
#include "PackLib/PackManager.h"
#include "Math/Math.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>
#include <algorithm>

float CCamera::CAMERA_MAX_DISTANCE=2500;
namespace fs=std::filesystem;
void Check(bool ok,const std::string&why){if(!ok)throw std::runtime_error(why);}
std::vector<std::byte> Read(const fs::path&p){std::ifstream f(p,std::ios::binary|std::ios::ate);Check(bool(f),"file missing: "+p.string());const auto size=f.tellg();Check(size>=0&&size<128*1024*1024,"invalid read size");std::vector<std::byte>b(static_cast<std::size_t>(size));f.seekg(0);Check(bool(f.read(reinterpret_cast<char*>(b.data()),size)),"file read");return b;}
void Save(const fs::path&p,const std::vector<std::uint8_t>&rgb,unsigned width,unsigned height){
    const unsigned pitch=(width*3+3)&~3u;BITMAPFILEHEADER h{0x4d42,DWORD(54+pitch*height),0,0,54};BITMAPINFOHEADER i{};i.biSize=40;i.biWidth=width;i.biHeight=-LONG(height);i.biPlanes=1;i.biBitCount=24;
    std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<char*>(&h),sizeof(h));f.write(reinterpret_cast<char*>(&i),sizeof(i));std::vector<std::uint8_t>row(pitch);
    for(unsigned y=0;y<height;++y){for(unsigned x=0;x<width;++x){row[x*3]=rgb[(y*width+x)*3+2];row[x*3+1]=rgb[(y*width+x)*3+1];row[x*3+2]=rgb[(y*width+x)*3];}f.write(reinterpret_cast<char*>(row.data()),row.size());}Check(bool(f),"capture write");
}
int main(int argc,char**argv){HWND window=nullptr;try{
    Check(argc==5,"usage: VegetationRenderParity client-directory compiled-directory evidence-directory golden-file");const fs::path client=argv[1],compiled=argv[2],evidence=argv[3];fs::create_directories(evidence);
    CPackManager packs;Check(packs.AddPack((client/"pack/tree.pck").string())&&packs.AddPack((client/"pack/zone.pck").string()),"original tree/zone texture packs");
    CResourceManager resources;resources.RegisterResourceNewFunctionPointer("dds",[](const char* path)->CResource*{return new CGraphicImage(path);});
    window=CreateWindowW(L"STATIC",L"H-X hidden vegetation parity",WS_POPUP,0,0,384,384,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Renderer::DiligentD3D11Backend backend;Check(window&&backend.Initialize({window,384,384}),"hidden D3D11 backend");
    std::ofstream textureReport(evidence/"texture-parity.tsv");textureReport<<"path\tformat\twidth\theight\tmips\tbytesIdentical\n";std::set<std::string> auditedTextures;
    const auto goldenBytes=Read(argv[4]);rapidjson::Document goldens;goldens.Parse(reinterpret_cast<const char*>(goldenBytes.data()),goldenBytes.size());Check(!goldens.HasParseError(),"render golden JSON");
    std::ofstream report(evidence/"render-goldens.tsv");report<<"asset\tview\tlod\tmeanTileRGBError\tmaxTileRGBError\tdraws\n";unsigned compared=0,frames=0;
    {
        Renderer::DiligentStaticObjectRenderer renderer(backend);Check(renderer.Initialize(),"production renderer");
        Vegetation::Runtime runtime;const auto registry=Read(compiled/"vegetation/registry.json");Check(bool(runtime.registry.Parse({reinterpret_cast<const char*>(registry.data()),registry.size()})),"compiled registry");
        std::set<std::string> names;for(const auto&g:goldens["cases"].GetArray())names.insert(g["asset"].GetString());
        for(const auto&name:names){const std::string key="d:/ymir work/tree/"+std::string(name)+".spt";auto asset=runtime.Load(key,[&](std::string_view p,std::vector<std::byte>&b){b=Read(compiled/fs::path(p));return true;});Check(bool(asset),asset.error);std::string error;
            std::vector<CGraphicImage::TRef> retainedImages;
            auto render=Vegetation::Prepare(asset.asset,renderer,[&](std::string_view p){
                CGraphicImage::TRef image=static_cast<CGraphicImage*>(resources.GetResourcePointer(std::string(p).c_str()));
                Check(!image->IsEmpty(),"production resource texture load");
                if(auditedTextures.insert(std::string(p)).second){
                    struct Capture final:Renderer::ITextureUploader{std::shared_ptr<Renderer::TextureResource> image;Renderer::TerrainTexturePtr UploadTexture(const Renderer::TerrainTextureData& d)override{image=Renderer::TextureResource::Copy(d);return image;}} expected;
                    LoadStaticObjectTextureFile(std::string(p).c_str(),expected);
                    const auto actual=image->GetTexturePointer()->GetSource();Check(actual&&expected.image,"texture source audit");
                    Check(actual->desc.format==expected.image->desc.format&&actual->desc.width==expected.image->desc.width&&actual->desc.height==expected.image->desc.height&&actual->mips.size()==expected.image->mips.size(),"texture format/dimensions/mips parity");
                    for(unsigned mip=0;mip<actual->mips.size();++mip)Check(actual->mips[mip].pixels==expected.image->mips[mip].pixels&&actual->mips[mip].rowStride==expected.image->mips[mip].rowStride,"texture color/alpha bytes parity for every mip");
                    const char* formats[]={"Unknown","RGBA8","BGRA8","BGRX8","BC1","BC2","BC3","Alpha8","B5G5R5A1"};
                    textureReport<<p<<'\t'<<formats[unsigned(actual->desc.format)]<<'\t'<<actual->desc.width<<'\t'<<actual->desc.height<<'\t'<<actual->mips.size()<<"\ttrue\n";
                }
                retainedImages.push_back(image);return image->GetAssetTexture(renderer);
            },error);Check(bool(render),error);
            const float height=asset.asset->metadata.bounds.max[2]-asset.asset->metadata.bounds.min[2];Vegetation::Instance instance(asset.asset,Vegetation::Identity);
            const std::array<unsigned char,4>maskPixel{255,255,255,200};Renderer::TerrainTextureData maskData;maskData.width=maskData.height=1;maskData.format=Renderer::TerrainTextureFormat::RGBA8;maskData.mips.push_back({maskPixel.data(),4,4});auto mask=renderer.UploadTexture(maskData);Check(bool(mask),"camera mask upload");bool windChecked=false;std::map<std::pair<unsigned,int>,std::vector<std::uint8_t>> repeated;

            for(unsigned view=0;view<5;++view)for(float level:{1.f,.5f,0.f,.5f,1.f}){
                Vegetation::RenderContext c;c.windStrength=0;c.state.sampling=c.state.cameraAlphaSampling={true,true,true,true,true,true};
                Math::Vector3 eye(view==1?height*1.6f:0,-height*2,view==2?height*1.5f:height*.5f),target(0,0,height*.5f),up(0,0,1);Math::Matrix v,p;
                instance.transform=Vegetation::Identity;
                if(view>=3){instance.transform[12]=19759;instance.transform[13]=-71610;instance.transform[14]=13516;eye={height*1.4142f,height*1.4142f,height*1.5f};for(unsigned k=0;k<3;++k){eye[k]+=instance.transform[12+k];target[k]+=instance.transform[12+k];}}
                Math::MatrixLookAtRH(&v,&eye,&target,&up);Math::MatrixPerspectiveFovRH(&p,.75f,1,1,height*30);std::memcpy(c.view.data(),&v,64);std::memcpy(c.projection.data(),&p,64);
                if(view==1||view==3){c.state.fog=Renderer::TerrainFog::Linear;c.state.rangeFog=true;c.state.fogParameters={height,height*4,0,0};c.state.fogColor={.1f,.15f,.2f,1};}
                if(view==4){c.state.fog=Renderer::TerrainFog::Exp;c.state.fogParameters={0,0,1/(height*20),0};c.state.fogColor={.7f,.75f,.85f,1};}
                if(view==2){c.state.cameraAlpha=mask;c.state.blend=true;}
                const float distance=asset.asset->metadata.farDistance-level*(asset.asset->metadata.farDistance-asset.asset->metadata.nearDistance);c.camera={instance.transform[12],instance.transform[13]-distance,instance.transform[14]};
                std::vector<std::uint8_t> pixels;unsigned width{},heightPixels{};const auto previousDraws=Vegetation::statistics.submitted;
                Check(backend.BeginFrame(),"begin golden frame");backend.Clear({true,Renderer::ClearColor{0,0,0,1}});renderer.ResetFrame();
                Check(Vegetation::Draw(instance,*render,renderer,c)&&!renderer.Failed(),"production vegetation draw");
                Check(backend.CaptureRGB(pixels,width,heightPixels)&&width==384&&heightPixels==384,"GPU readback");backend.EndFrame();backend.Present();renderer.ReleaseBindings();++frames;
                const auto draws=Vegetation::statistics.submitted-previousDraws;Check(draws>0&&draws<=5,"bounded nonempty draw submission");
                const auto id=std::make_pair(view,int(level*2));if(auto it=repeated.find(id);it!=repeated.end())Check(it->second==pixels,"near/mid/far/mid/near deterministic return");else repeated[id]=pixels;
                for(const auto&g:goldens["cases"].GetArray())if(name==g["asset"].GetString()&&view==g["view"].GetUint()&&level==g["level"].GetFloat()){
                    double sum=0,maximum=0;unsigned index=0;
                    for(unsigned ty=0;ty<16;++ty)for(unsigned tx=0;tx<16;++tx)for(unsigned channel=0;channel<3;++channel){
                        double value=0;for(unsigned y=ty*24;y<(ty+1)*24;++y)for(unsigned x=tx*24;x<(tx+1)*24;++x)value+=pixels[(y*384+x)*3+channel];
                        const double error=std::abs(value/576-g["rgb16"][index++].GetDouble());sum+=error;maximum=std::max(maximum,error);
                    }
                    report<<name<<'\t'<<view<<'\t'<<level<<'\t'<<sum/768<<'\t'<<maximum<<'\t'<<draws<<std::endl;
                    Check(sum/768<.25&&maximum<3,"independent reference image signature differs; see TSV");++compared;
                }
                if(!windChecked&&view==0&&level==1&&instance.lod.meshes[2]>=0){std::array<std::vector<std::uint8_t>,3>windImages;unsigned w{},h{};
                    for(unsigned sample=0;sample<3;++sample){c.windStrength=1;c.time=sample==1?1.25f:0;Check(backend.BeginFrame(),"wind frame");backend.Clear({true,Renderer::ClearColor{0,0,0,1}});renderer.ResetFrame();Check(Vegetation::Draw(instance,*render,renderer,c),"wind draw");Check(backend.CaptureRGB(windImages[sample],w,h),"wind readback");backend.EndFrame();backend.Present();renderer.ReleaseBindings();}
                    Check(windImages[0]==windImages[2]&&windImages[0]!=windImages[1],"wind changes with time and repeats deterministically");windChecked=true;
                }

            }
            Check(backend.Resize(0,0)&&backend.Resize(400,384)&&backend.Resize(384,384),"minimize/restore/resize");
        }
        resources.DestroyDeletingList();resources.Destroy();runtime.Clear();renderer.ReleaseBindings();Check(Vegetation::liveAssets==0&&Vegetation::liveInstances==0&&Vegetation::liveGeometry==0&&Vegetation::liveRenderAssets==0,"vegetation resource lifetime");Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0,"GPU resource lifetime");
    }backend.Shutdown();DestroyWindow(window);window=nullptr;Check(compared==81&&frames==225,"golden/LOD case coverage");std::cout<<"PASS 9 real types; 225 frames; 81 comparisons to 54 independent SDK-free image signatures; fog/alpha/camera mask/wind/LOD/resize; resources=0\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';if(window)DestroyWindow(window);return 1;}}
