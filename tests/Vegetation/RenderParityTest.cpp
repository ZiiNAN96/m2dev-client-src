// Offline SDK reference and the normal mesh renderer, in the same hidden D3D11 framebuffer.
#include <cstddef>
#include <SpeedTreeRT.h>
#include "EterGrnLib/StdAfx.h"
#include "Vegetation/VegetationRenderer.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentTreeRenderer.h"
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
void Setup(CSpeedTreeRT&tree,const fs::path&p){
    const float light[]={-.707f,-.300f,.707f,1,1,1,.5f,.5f,.5f,1,1,1,0,1,0,0};CSpeedTreeRT::SetNumWindMatrices(4);CSpeedTreeRT::SetTextureFlip(true);CSpeedTreeRT::SetLightAttributes(0,light);CSpeedTreeRT::SetLightState(0,true);CSpeedTreeRT::SetDropToBillboard(true);CSpeedTreeRT::SetTime(0);
    tree.SetWindStrength(1);tree.SetLocalMatrices(0,4);Check(tree.LoadTree(p.string().c_str()),"reference source load");tree.SetBranchLightingMethod(CSpeedTreeRT::LIGHT_STATIC);tree.SetFrondLightingMethod(CSpeedTreeRT::LIGHT_STATIC);tree.SetLeafLightingMethod(CSpeedTreeRT::LIGHT_STATIC);tree.SetBranchWindMethod(CSpeedTreeRT::WIND_NONE);tree.SetFrondWindMethod(CSpeedTreeRT::WIND_NONE);tree.SetLeafWindMethod(CSpeedTreeRT::WIND_NONE);tree.SetNumLeafRockingGroups(1);Check(tree.Compute(nullptr,1,false),"reference compute");tree.SetLeafRockingState(true);
}
void Reference(CSpeedTreeRT&tree,Renderer::DiligentTreeRenderer&r,const Vegetation::RenderContext&c,float level,const std::array<Renderer::TerrainTexturePtr,4>&textures,const Vegetation::Matrix&world){
    const float dir[]={-c.view[2],-c.view[6],-c.view[10]};CSpeedTreeRT::SetCamera(c.camera.data(),dir);CSpeedTreeRT::SetTime(0);tree.SetLodLevel(level);CSpeedTreeRT::SGeometry g;tree.GetGeometry(g);
    Renderer::TreeDraw d;d.matrices={world,c.view,c.projection};d.alphaTest=true;d.depthWrite=true;d.blend=c.state.blend;d.fog=unsigned(c.state.fog);d.fogColor=c.state.fogColor;d.fogParameters=c.state.fogParameters;d.rangeFog=c.state.rangeFog;d.samplers[0].sampling=d.samplers[1].sampling={true,true,true,true,true,true};
    for(unsigned kind=0;kind<2;++kind){const auto&n=kind?g.m_sFronds:g.m_sBranches;if(n.m_nDiscreteLodLevel<0||!n.m_usVertexCount)continue;Renderer::TreeSource source;
        source.vertices.resize(n.m_usVertexCount);for(unsigned v=0;v<n.m_usVertexCount;++v){auto&out=source.vertices[v];std::copy_n(n.m_pCoords+v*3,3,out.position.begin());out.color=std::uint32_t(n.m_pColors[v]);std::copy_n(n.m_pTexCoords0+v*2,2,out.uv.begin());if(n.m_pTexCoords1)std::copy_n(n.m_pTexCoords1+v*2,2,out.shadowUv.begin());}
        for(unsigned s=0;s<n.m_usNumStrips;++s)source.indices.insert(source.indices.end(),n.m_pStrips[s],n.m_pStrips[s]+n.m_pStripLengths[s]);if(source.indices.empty())continue;
        auto geometry=r.UploadGeometry(source);Check(bool(geometry),"reference indexed upload");d.part=kind?Renderer::TreePart::Frond:Renderer::TreePart::Branch;d.cull=kind?0:1;d.strip=true;d.stage1=c.state.cameraAlpha?2:(textures[2]?1:0);d.cameraCoordinates=bool(c.state.cameraAlpha);d.first=0;d.alphaReference=unsigned(kind?g.m_fFrondAlphaTestValue:g.m_fBranchAlphaTestValue);
        for(unsigned s=0;s<n.m_usNumStrips;++s){d.count=n.m_pStripLengths[s];if(d.count>=3)r.Draw(&tree,geometry,textures[kind?1:0],c.state.cameraAlpha?textures[3]:textures[2],d);d.first+=d.count;}
    }
    d.cull=0;d.strip=false;d.first=0;d.stage1=c.state.cameraAlpha?2:0;d.fog=c.state.fog==Renderer::TerrainFog::None?0:4;const bool densityFog=c.state.fog==Renderer::TerrainFog::Exp;
    const float fogNear=densityFog?0:c.state.fogParameters[0],fogFar=densityFog?2.3f/c.state.fogParameters[2]:c.state.fogParameters[1];
    d.legacyConstants[85]={fogNear,fogFar,fogFar!=fogNear?1/(fogFar-fogNear):0,0};
    Math::Matrix view,projection,compound;std::memcpy(&view,c.view.data(),64);std::memcpy(&projection,c.projection.data(),64);Math::MatrixMultiply(&compound,&view,&projection);
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)d.legacyConstants[row][col]=compound.m[col][row];d.legacyConstants[52]={world[12],world[13],world[14],0};
    unsigned tableCount=0;const float*table=tree.GetLeafBillboardTable(tableCount);Check(tableCount<=84*4,"leaf table fits native renderer");std::copy_n(table,tableCount,d.legacyConstants[4].data());
    for(const auto*leaf:{&g.m_sLeaves0,&g.m_sLeaves1})if(leaf->m_bIsActive&&leaf->m_usLeafCount){Renderer::TreeSource source;const unsigned corners[]={0,1,2,0,2,3};
        for(unsigned i=0;i<leaf->m_usLeafCount;++i)for(unsigned k:corners){Renderer::TreeVertex v;std::copy_n(leaf->m_pCenterCoords+i*3,3,v.position.begin());v.color=std::uint32_t(leaf->m_pColors[i]);std::copy_n(leaf->m_pLeafMapTexCoords[i]+k*2,2,v.uv.begin());v.leaf={float(4+leaf->m_pLeafClusterIndices[i]*4+k),tree.GetLeafLodSizeAdjustments()[leaf->m_nDiscreteLodLevel]};source.vertices.push_back(v);}
        auto geometry=r.UploadGeometry(source);d.part=Renderer::TreePart::Leaf;d.count=unsigned(source.vertices.size());d.alphaReference=unsigned(leaf->m_fAlphaTestValue);r.Draw(&tree,geometry,textures[1],c.state.cameraAlpha?textures[3]:Renderer::TerrainTexturePtr{},d);
    }
    d.fog=unsigned(c.state.fog);
    if(g.m_sBillboard0.m_bIsActive){Renderer::TreeSource source;for(unsigned k:{0u,1u,2u,0u,2u,3u}){Renderer::TreeVertex v;std::copy_n(g.m_sBillboard0.m_pCoords+k*3,3,v.position.begin());std::copy_n(g.m_sBillboard0.m_pTexCoords+k*2,2,v.uv.begin());source.vertices.push_back(v);}auto geometry=r.UploadGeometry(source);d.part=Renderer::TreePart::Billboard;d.count=6;d.alphaReference=unsigned(g.m_sBillboard0.m_fAlphaTestValue);r.Draw(&tree,geometry,textures[1],c.state.cameraAlpha?textures[3]:Renderer::TerrainTexturePtr{},d);}
    Check(!r.Failed(),"reference draw failed");
}
int main(int argc,char**argv){HWND window=nullptr;try{
    Check(argc==4,"usage: VegetationRenderParity client-directory compiled-directory evidence-directory");const fs::path client=argv[1],compiled=argv[2],evidence=argv[3];fs::create_directories(evidence);
    CPackManager packs;Check(packs.AddPack((client/"pack/tree.pck").string())&&packs.AddPack((client/"pack/zone.pck").string()),"original tree/zone texture packs");
    CResourceManager resources;resources.RegisterResourceNewFunctionPointer("dds",[](const char* path)->CResource*{return new CGraphicImage(path);});
    window=CreateWindowW(L"STATIC",L"H-X hidden vegetation parity",WS_POPUP,0,0,384,384,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    Renderer::DiligentD3D11Backend backend;Check(window&&backend.Initialize({window,384,384}),"hidden D3D11 backend");
    std::ofstream textureReport(evidence/"texture-parity.tsv");textureReport<<"path\tformat\twidth\theight\tmips\tbytesIdentical\n";std::set<std::string> auditedTextures;
    std::ofstream report(evidence/"visual-parity.tsv");report<<"asset\tview\tlod\tiou\tmeanRGBError\tchangedFraction\treferenceDraws\tziinanDraws\n";bool passed=true;
    {
        Renderer::DiligentStaticObjectRenderer renderer(backend);Renderer::DiligentTreeRenderer reference(backend);Check(renderer.Initialize()&&reference.Initialize(),"normal and reference renderers");
        Vegetation::Runtime runtime;const auto registry=Read(compiled/"vegetation/registry.json");Check(bool(runtime.registry.Parse({reinterpret_cast<const char*>(registry.data()),registry.size()})),"compiled registry");
        const std::array<const char*,34> names={"b1_baobab_rt","b1_baobab_rt2","b1_beech_rt","b1_beech_rt2","b1_beech_rt3","b1_beech_rt4","b1_montereycypress_rt","b1_montereycypress_rt2","b1_montereycypress_rt3","b1_montereycypress_rt4","b1_montereycypress_rt5","b1_pagodatree_rt","b1_pagodatree_rt2","b1_pagodatree_rt3","b1_sassafras_rt_fall","b1_sassafras_rt_fall2","b3_beech_rt","b3_beech_rt2","b3_beech_rt3","b3_beech_rt_fall","b3_beech_rt_fall2","b3_pagodatree_rt_winter","b3_pagodatree_rt_winter2","b3_shingleoak_rt","b3_shingleoak_rt2","b3_shingleoak_rt3","b3_shingleoak_rt4","b3_shingleoak_rt5","b3_umbrellathorn_rt_flowers","b3_umbrellathorn_rt_flowers2","b3_umbrellathorn_rt_flowers3","b3_umbrellathorn_rt_flowers4","n2_cinnamonfern_rt_01","n2_coconutpalm_rt_01"};
        for(const auto*name:names){const std::string key="d:/ymir work/tree/"+std::string(name)+".spt";auto asset=runtime.Load(key,[&](std::string_view p,std::vector<std::byte>&b){b=Read(compiled/fs::path(p));return true;});Check(bool(asset),asset.error);std::string error;
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
            CSpeedTreeRT tree;Setup(tree,client/"assets/Tree/ymir work/tree"/(std::string(name)+".spt"));
            const auto model=asset.asset->geometry.Model(0);CSpeedTreeRT::STextures sdkTextures;tree.GetTextures(sdkTextures);
            const auto sdkPath=[&](const char* path){return path&&*path?Vegetation::NormalizeKey((fs::path(key).parent_path()/fs::path(path).replace_extension(".dds")).generic_string()):std::string{};};
            const std::array<std::string,3> texturePaths{sdkPath(sdkTextures.m_pBranchTextureFilename),sdkPath(sdkTextures.m_pCompositeFilename),sdkPath(sdkTextures.m_pSelfShadowFilename)};
            Check(texturePaths[0]==model.Get()->materials[0].textures[0]&&texturePaths[1]==model.Get()->materials[1].textures[0]&&texturePaths[2]==asset.asset->metadata.shadowTexture,"independent SDK texture paths match compiled material paths");std::array<Renderer::TerrainTexturePtr,4>textures;
            for(unsigned i=0;i<3;++i)if(!texturePaths[i].empty())textures[i]=LoadStaticObjectTextureFile(texturePaths[i].c_str(),reference);
            const float height=asset.asset->metadata.bounds.max[2]-asset.asset->metadata.bounds.min[2];Vegetation::Instance instance(asset.asset,Vegetation::Identity);
            const std::array<unsigned char,4>maskPixel{255,255,255,200};Renderer::TerrainTextureData maskData;maskData.width=maskData.height=1;maskData.format=Renderer::TerrainTextureFormat::RGBA8;maskData.mips.push_back({maskPixel.data(),4,4});auto mask=renderer.UploadTexture(maskData);textures[3]=reference.UploadTexture(maskData);Check(mask&&textures[3],"camera mask upload");bool windChecked=false;

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
                std::array<std::vector<std::uint8_t>,2>images;unsigned width{},heightPixels{},referenceDraws{};const auto previousDraws=Vegetation::statistics.submitted;
                for(unsigned path=0;path<2;++path){Check(backend.BeginFrame(),"begin parity frame");backend.Clear({true,Renderer::ClearColor{0,0,0,1}});renderer.ResetFrame();reference.ResetFrame();
                    if(path==0)Reference(tree,reference,c,level,textures,instance.transform);else Check(Vegetation::Draw(instance,*render,renderer,c)&&!renderer.Failed(),"compiled vegetation draw");
                    if(path==0)for(unsigned kind=0;kind<4;++kind)referenceDraws+=reference.DrawCount(Renderer::TreePart(kind));
                    Check(backend.CaptureRGB(images[path],width,heightPixels),"GPU readback");backend.EndFrame();backend.Present();renderer.ReleaseBindings();
                }
                std::size_t intersection=0,united=0,changed=0;double sum=0;
                for(std::size_t i=0;i<images[0].size();i+=3){bool a=false,b=false,diff=false;for(unsigned k=0;k<3;++k){a|=images[0][i+k]>4;b|=images[1][i+k]>4;const int d=std::abs(int(images[0][i+k])-int(images[1][i+k]));sum+=d;diff|=d>12;}intersection+=a&&b;united+=a||b;changed+=diff;}
                const double iou=united?double(intersection)/united:0,mae=sum/images[0].size(),fraction=double(changed)/(images[0].size()/3);report<<name<<'\t'<<view<<'\t'<<level<<'\t'<<iou<<'\t'<<mae<<'\t'<<fraction<<'\t'<<referenceDraws<<'\t'<<(Vegetation::statistics.submitted-previousDraws)<<'\n';
                Check(Vegetation::statistics.submitted-previousDraws<=referenceDraws&&Vegetation::statistics.submitted-previousDraws<=5,"draw count does not exceed reference");
                if((view==0||view>=3)&&(level==1||level==0)){const auto suffix=std::string(name)+(view==3?"-world-linear":view==4?"-world-density":"")+(level==1?"-near":level==0?"-far":"-mid");Save(evidence/(suffix+"-reference.bmp"),images[0],width,heightPixels);Save(evidence/(suffix+"-ziinan.bmp"),images[1],width,heightPixels);}
                passed&=united>20&&iou>.99&&mae<.25&&fraction<.005;
                if(!windChecked&&view==0&&level==1&&instance.lod.meshes[2]>=0){std::array<std::vector<std::uint8_t>,3>windImages;unsigned w{},h{};
                    for(unsigned sample=0;sample<3;++sample){c.windStrength=1;c.time=sample==1?1.25f:0;Check(backend.BeginFrame(),"wind frame");backend.Clear({true,Renderer::ClearColor{0,0,0,1}});renderer.ResetFrame();Check(Vegetation::Draw(instance,*render,renderer,c),"wind draw");Check(backend.CaptureRGB(windImages[sample],w,h),"wind readback");backend.EndFrame();backend.Present();renderer.ReleaseBindings();}
                    Check(windImages[0]==windImages[2]&&windImages[0]!=windImages[1],"wind changes with time and repeats deterministically");windChecked=true;
                }

            }
            Check(backend.Resize(0,0)&&backend.Resize(400,384)&&backend.Resize(384,384),"minimize/restore/resize");
        }
        resources.DestroyDeletingList();resources.Destroy();runtime.Clear();reference.ResetFrame();renderer.ReleaseBindings();Check(Vegetation::liveAssets==0&&Vegetation::liveInstances==0&&Vegetation::liveGeometry==0&&Vegetation::liveRenderAssets==0,"vegetation resource lifetime");Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0&&reference.LiveGeometryCount()==0&&reference.LiveTextureCount()==0,"GPU resource lifetime");
    }backend.Shutdown();DestroyWindow(window);window=nullptr;Check(passed,"visual parity threshold failed; see TSV and captures");std::cout<<"PASS 34 real types x 5 camera/environment cases x near/mid/far/mid/near; reference/compiled readbacks; resize/minimize/restore; resources=0\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';if(window)DestroyWindow(window);return 1;}}
