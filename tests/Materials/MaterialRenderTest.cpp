#if defined(_DEBUG)
#include <crtdbg.h>
#endif
#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/Thing.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterLib/Camera.h"
#include "EterLib/ResourceManager.h"
#include "PackLib/PackManager.h"
#include "AssetRuntime/Providers.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/AssetMaterialRenderData.h"
#include "Renderer/GraphicsConfig.h"
#include "Renderer/SceneLightingRuntime.h"
#include <chrono>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace Renderer;
float CCamera::CAMERA_MAX_DISTANCE=2500.f;
static constexpr std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static void Check(bool ok,const char* text){if(!ok)throw std::runtime_error(text);}
static CResource* NewThing(const char* path){return new CGraphicThing(path);}
static CResource* NewImage(const char* path){return new CGraphicImage(path);}
static void Style(bool modern){Graphics::GraphicsSettings s;s.style=modern?Graphics::GraphicsStyle::Modern:Graphics::GraphicsStyle::Classic;ApplyGraphicsRuntimeConfig(Graphics::Resolve(s,modern?2:1));}
static std::vector<std::uint8_t> Capture(DiligentD3D11Backend& backend,const std::string& name={})
{
    std::vector<std::uint8_t> rgb;unsigned w{},h{};Check(backend.CaptureRGB(rgb,w,h),"GPU readback");
    if(!name.empty()){
        const auto stride=(w*3+3)&~3u;BITMAPFILEHEADER fh{0x4d42,DWORD(54+stride*h),0,0,54};BITMAPINFOHEADER info{};
        info.biSize=40;info.biWidth=LONG(w);info.biHeight=-LONG(h);info.biPlanes=1;info.biBitCount=24;
        std::ofstream out(name,std::ios::binary);out.write(reinterpret_cast<const char*>(&fh),sizeof(fh));out.write(reinterpret_cast<const char*>(&info),sizeof(info));
        std::vector<char> row(stride);for(unsigned y=0;y<h;++y){for(unsigned x=0;x<w;++x)for(unsigned c=0;c<3;++c)row[x*3+c]=char(rgb[(y*w+x)*3+2-c]);out.write(row.data(),row.size());}
        Check(bool(out),"proof image written");
    }
    return rgb;
}
static TerrainTexturePtr Pixel(DiligentStaticObjectRenderer& renderer,std::array<std::uint8_t,4> pixel)
{TerrainTextureData data;data.width=data.height=1;data.format=TerrainTextureFormat::RGBA8;data.mips.push_back({pixel.data(),4,4});return renderer.UploadTexture(data);}
static void BadOptionalMaps(DiligentStaticObjectRenderer& renderer)
{
    const auto bad=std::filesystem::absolute("g1x-invalid.png");{std::ofstream out(bad);out<<"invalid PNG proof";}
    const auto wrong=std::filesystem::absolute("g1x-wrong-texture-type.gr2");{std::ofstream out(wrong);out<<"non-image resource proof";}
    {
        AssetRuntime::MaterialAsset source;source.explicitRenderState=true;source.model=AssetRuntime::MaterialModel::PBRMetallicRoughness;
        source.pbr.maps[1].path=bad.generic_string();source.pbr.maps[2].path="g1x-missing-roughness.png";
        source.pbr.maps[3].path=wrong.generic_string();
        CGrannyMaterial material;Check(material.CreateFromAsset(source),"optional bad maps do not reject legacy base input");
        const auto runtime=material.GetRenderMaterial(renderer);
        Check(runtime->maps[0]&&!runtime->maps[1]&&!runtime->maps[2]&&!runtime->maps[3]&&!renderer.Failed(),"bad normal, missing roughness and wrong resource type retain defaults");
    }
    std::filesystem::remove(bad);std::filesystem::remove(wrong);renderer.ResetFrame();
}
static void Channels(DiligentD3D11Backend& backend,DiligentStaticObjectRenderer& renderer)
{
    Check(backend.Resize(128,128),"channel framebuffer");
    StaticObjectSource source;source.vertices={{{-.9f,-.9f,.5f,0,0,-1,0,0}},{{-.9f,.9f,.5f,0,0,-1,0,1}},{{.9f,.9f,.5f,0,0,-1,1,1}},{{.9f,-.9f,.5f,0,0,-1,1,0}}};source.indices={0,1,2,0,2,3};
    source.indices={0,2,1,0,3,2}; // Same front winding as the production glTF/GR2 geometry.
    auto geometry=renderer.UploadGeometry(source);Check(bool(geometry),"channel geometry");
    auto material=std::make_shared<MaterialRuntime>();material->parameters.roughness=1;material->parameters.metallic=1;material->parameters.emissive={1,1,1};
    auto color=Pixel(renderer,{128,128,128,255});auto data=Pixel(renderer,{32,64,192,255});
    Check(renderer.LiveTextureCount()==2,"linear and sRGB views share exactly two texture allocations");
    material->classicDiffuse=color;material->maps={color,{},data,data,color};
    StaticObjectDraw draw;draw.material=material;draw.matrices={identity,identity,identity};draw.normalTransform=identity;
    draw.cull=StaticObjectCull::None;draw.vertexCount=4;draw.indexCount=6;draw.ambient={.2f,.2f,.2f,1};draw.diffuse={1,1,1,1};draw.lightDirection={0,0,-1,0};
    const auto run=[&](MaterialDebugView mode){
        Graphics::SceneLighting light;light.sun.direction={draw.lightDirection[0],draw.lightDirection[1],draw.lightDirection[2]};
        light.sun.color={draw.diffuse[0],draw.diffuse[1],draw.diffuse[2]};
        light.ambientSkyColor=light.ambientGroundColor={draw.ambient[0],draw.ambient[1],draw.ambient[2]};sceneLighting.Set(light);
        materialDebugView=mode;Check(backend.BeginFrame(),"channel frame");backend.Clear({true,ClearColor{0,0,0,1}});renderer.Draw(geometry,color,draw);Check(!renderer.Failed(),"channel production PBR draw");auto rgb=Capture(backend);backend.EndFrame();backend.Present();return rgb;};
    Style(true);
    for(const auto [mode,expected]:{std::pair{MaterialDebugView::BaseColor,128},std::pair{MaterialDebugView::Roughness,64},std::pair{MaterialDebugView::Metallic,192},std::pair{MaterialDebugView::Occlusion,32},std::pair{MaterialDebugView::Emissive,128}}){
        auto rgb=run(mode);Check(std::abs(int(rgb[(64*128+64)*3])-expected)<=2,"sRGB roundtrip / linear G roughness, B metallic, R AO channels");
    }
    const auto flat=run(MaterialDebugView::Normal);
    Check(flat[(64*128+64)*3+2]<4,"missing normal map uses geometric normal");
    // New immutable runtime state for the normal-map test; existing bindings never need translation per draw.
    draw.material.reset();material.reset();renderer.ResetFrame();
    material=std::make_shared<MaterialRuntime>();material->maps[0]=color;material->maps[1]=Pixel(renderer,{210,128,220,255});draw.material=material;
    const auto mapped=run(MaterialDebugView::Normal);Check(std::abs(int(mapped[(64*128+64)*3])-int(flat[(64*128+64)*3]))>30,"normal map changes the production normal, derivative TBN without tangents");
    const auto replace=[&](){draw.material.reset();material.reset();renderer.ResetFrame();material=std::make_shared<MaterialRuntime>();material->maps[0]=color;draw.material=material;};
    replace();material->maps[1]=Pixel(renderer,{210,128,220,255});material->parameters.maps[1].uvTransform={-1,0,1,0,1,0};
    const auto mirrored=run(MaterialDebugView::Normal);
    Check((int(mapped[(64*128+64)*3])-128)*(int(mirrored[(64*128+64)*3])-128)<0,"mirrored UV flips tangent direction without flipping geometric normal");
    replace();material->maps[1]=Pixel(renderer,{210,128,220,255});material->parameters.normalScale=-1;
    const auto negativeScale=run(MaterialDebugView::Normal);
    Check((int(mapped[(64*128+64)*3])-128)*(int(negativeScale[(64*128+64)*3])-128)<0,"signed glTF normal scale flips tangent-space XY");
    replace();material->maps[1]=Pixel(renderer,{128,128,128,255});
    const auto invalid=run(MaterialDebugView::Normal);Check(invalid[(64*128+64)*3+2]<4,"invalid quantized zero normal uses geometric normal");
    // Material AO changes indirect light only. Directional-only output must be identical.
    replace();draw.ambient={0,0,0,1};const auto direct=run(MaterialDebugView::Lit);
    replace();material->maps[3]=data;const auto occludedDirect=run(MaterialDebugView::Lit);
    Check(direct==occludedDirect,"material AO never occludes direct light");
    draw.ambient={.6f,.6f,.6f,1};const auto occludedAmbient=run(MaterialDebugView::Lit);
    replace();const auto ambient=run(MaterialDebugView::Lit);
    Check(ambient[(64*128+64)*3]>occludedAmbient[(64*128+64)*3],"material AO reduces the ambient contribution");
    // Alpha/cull use the same existing state helper for Classic and Modern.
    for(bool modern:{false,true}) {
        Style(modern);
        for(auto mode:{AssetRuntime::AlphaMode::Opaque,AssetRuntime::AlphaMode::Mask,AssetRuntime::AlphaMode::Blend}) {
            replace();material->maps[0]=Pixel(renderer,{255,255,255,128});material->explicitRenderState=material->authored=true;
            material->parameters.baseColor={1,1,1,.5f};material->parameters.alpha=mode;
            AssetRuntime::MaterialAsset description;description.explicitRenderState=true;description.baseColorFactor={1,1,1,.5f};description.culling=AssetRuntime::Culling::None;
            description.alphaTest=mode==AssetRuntime::AlphaMode::Mask;description.blending=mode==AssetRuntime::AlphaMode::Blend;description.depthWrite=!description.blending;
            ApplyAssetMaterial(description,draw);draw.ambient={1,1,1,1};draw.diffuse={0,0,0,1};draw.normalizeNormals=true;
            // Classic uses the same alpha texture as the runtime, with legacy encoded white lighting.
            auto previous=color;color=material->maps[0];const auto rgba=run(MaterialDebugView::BaseColor);color=previous;
            const int expected=mode==AssetRuntime::AlphaMode::Opaque?255:mode==AssetRuntime::AlphaMode::Mask?0:64;
            Check(std::abs(int(rgba[(64*128+64)*3])-expected)<=2,"OPAQUE/MASK/BLEND preserve factor alpha exactly once in both styles");
        }
    }
    Style(true);replace();draw.blend=false;draw.depthWrite=true;draw.alphaTest=StaticObjectAlphaTest::Disabled;draw.textureFactor={1,1,1,1};
    draw.cull=StaticObjectCull::Clockwise;const auto firstCull=run(MaterialDebugView::BaseColor);
    draw.cull=StaticObjectCull::CounterClockwise;const auto oppositeCull=run(MaterialDebugView::BaseColor);
    Check((firstCull[(64*128+64)*3]==0)!=(oppositeCull[(64*128+64)*3]==0),"opposite cull modes select opposite faces");
    draw.cull=StaticObjectCull::None;Check(run(MaterialDebugView::BaseColor)[(64*128+64)*3]>0,"double-sided geometry renders");
    {
        const std::array<std::uint8_t,8> pixels{255,255,255,0,255,255,255,255};
        TerrainTextureData mask;mask.width=2;mask.height=1;mask.format=TerrainTextureFormat::RGBA8;mask.mips.push_back({pixels.data(),8,8});
        draw.cameraAlpha=renderer.UploadTexture(mask);draw.cameraAlphaTransform={};draw.cameraAlphaTransform[12]=1.25f;draw.cameraAlphaTransform[13]=.5f;
        draw.cameraAlphaSampling.wrapU=draw.cameraAlphaSampling.wrapV=false;draw.blend=true;draw.depthWrite=false;
        const auto clamped=run(MaterialDebugView::BaseColor);
        draw.cameraAlphaSampling.wrapU=true;const auto repeated=run(MaterialDebugView::BaseColor);
        Check(clamped[(64*128+64)*3]>100 && repeated[(64*128+64)*3]<5,"camera alpha retains native clamp/wrap sampler state");
        draw.cameraAlpha.reset();draw.blend=false;draw.depthWrite=true;
    }
    const auto psoCount=livePBRPipelines.load(),creationCount=materialRuntimeCreations.load();
    materialDebugView=MaterialDebugView::Lit;
    for(bool modern:{false,true}){
        Style(modern);renderer.ResetFrame();const auto before=pbrDraws.load();auto start=std::chrono::steady_clock::now();
        for(unsigned f=0;f<12;++f){Check(backend.BeginFrame(),"sanity frame");renderer.Draw(geometry,color,draw);backend.EndFrame();}
        auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        Check(!renderer.Failed()&&renderer.DrawCount()==12&&pbrDraws-before==(modern?12:0),"sanity measures twelve actual draws in the selected style");
        std::cout<<(modern?"Modern":"Classic")<<" submission_us_12_frames="<<us<<" PSOs="<<livePBRPipelines<<" material_creations="<<materialRuntimeCreations<<'\n';
    }
    Check(livePBRPipelines==psoCount&&materialRuntimeCreations==creationCount,"no per-frame PSO/material creation");
    draw.material.reset();material.reset();color.reset();data.reset();geometry.reset();renderer.ReleaseBindings();renderer.ResetFrame();
    Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0&&liveMaterialRuntimeObjects==0&&livePBRBindings==0,"channel resources released");
}
static void Balls(DiligentD3D11Backend& backend,DiligentStaticObjectRenderer& renderer,CResourceManager& resources,const char* path,bool lighting=false)
{
    Graphics::SceneLighting light;light.sun.direction={-.35f,-.6f,1};light.sun.intensity=1.6f;sceneLighting.Set(light);
    Check(backend.Resize(960,640),"balls framebuffer");StaticObjectLoadScope loadScope;
    CGraphicThing::TRef thing(resources.GetResourcePointer(path));Check(!thing.IsNull()&&!thing->IsEmpty(),"PBR GLB through normal ResourceManager/provider");
    auto* model=thing->GetModelPointer(0);Check(model&&model->GetStaticObjectSource(),"normal immutable mesh capture");
    CGrannyModelInstance instance;instance.SetMainModelPointer(model,nullptr);Math::Matrix world;Math::MatrixIdentity(&world);instance.DeformNoSkin(&world);
    auto geometry=renderer.UploadGeometry(*model->GetStaticObjectSource());Check(bool(geometry),"material balls GPU upload");
    Math::Vector3 eye(370,-900,400),target(0,0,170),up(0,0,1);Math::Matrix view,projection;
    if(lighting)eye={300,-1400,500};
    Math::MatrixLookAtRH(&view,&eye,&target,&up);Math::MatrixPerspectiveFovRH(&projection,.62f,1.5f,1,2500);
    std::vector<std::uint8_t> classic,modern;
    for(unsigned mode=0;mode<(lighting?5u:8u);++mode){
        Style(mode!=0);materialDebugView=lighting||mode<=1?MaterialDebugView::Lit:static_cast<MaterialDebugView>(mode-1);
        if(lighting&&mode){
            const Graphics::LightVector directions[]={{0,-1,.35f},{1,0,.35f},{0,1,.35f},{0,0,1}};
            light.sun.direction=directions[mode-1];sceneLighting.Set(light);
        }
        Check(backend.BeginFrame(),"material proof frame");renderer.ResetFrame();backend.Clear({true,ClearColor{.035f,.045f,.06f,1}});
        auto& palette=instance.GetStaticObjectMaterialPalette();unsigned draws=0;
        for(auto* node=model->GetMeshNodeList(CGrannyMesh::TYPE_RIGID,CGrannyMaterial::TYPE_DIFFUSE_PNT);node;node=node->pNextMeshNode){
            StaticObjectDraw draw;auto* matrix=instance.GetStaticObjectWorldMatrix(node->iMesh);
            std::memcpy(draw.matrices.world.data(),matrix,64);std::memcpy(draw.matrices.view.data(),&view,64);std::memcpy(draw.matrices.projection.data(),&projection,64);
            Math::Matrix normal=*matrix*view;Math::MatrixInverse(&normal,nullptr,&normal);Math::MatrixTranspose(&normal,&normal);std::memcpy(draw.normalTransform.data(),&normal,64);
            draw.ambient={.22f,.22f,.22f,1};draw.diffuse={1.6f,1.6f,1.6f,1};draw.lightDirection={-.35f,.6f,1,0};draw.normalizeNormals=true;
            draw.baseVertex=node->pMesh->GetVertexBasePosition();draw.vertexCount=node->pMesh->GetVertexCount();
            for(auto* group=node->pMesh->GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT);group;group=group->pNextTriGroupNode){
                auto& material=palette.GetMaterialRef(group->mtrlIndex);ApplyAssetMaterial(material.GetAsset(),draw);draw.material=material.GetRenderMaterial(renderer);
                draw.firstIndex=group->idxPos;draw.indexCount=group->triCount*3;renderer.Draw(geometry,draw.material->classicDiffuse,draw);++draws;
            }
        }
        Check(draws==(lighting?8u:6u)&&!renderer.Failed(),"production material groups");auto rgb=Capture(backend,(lighting?"g2x-balls-":"g1x-balls-")+std::to_string(mode)+".bmp");
        if(mode==0)classic=rgb;if(mode==1)modern=rgb;
        backend.EndFrame();backend.Present();renderer.ReleaseBindings();
    }
    Check(classic!=modern,"live style switch changes actual material rendering");materialDebugView=MaterialDebugView::Lit;
    Check(backend.Resize(0,0)&&backend.Resize(960,640),"PBR suspension/restore");
    instance.Clear();geometry.reset();thing.Clear();resources.DestroyDeletingList();resources.Destroy();renderer.ReleaseBindings();renderer.ResetFrame();
    Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0&&liveMaterialRuntimeObjects==0&&livePBRBindings==0,"material map change/shutdown resources");
}
int main(int argc,char** argv)
{
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    std::cout << std::unitbuf;
    HWND window=nullptr;
    try {
        Check(argc==2||(argc==3&&std::string_view(argv[2])=="--lighting"),"material balls fixture / optional lighting proof");CPackManager packs;CResourceManager resources;
        resources.RegisterResourceNewFunctionPointer("png",NewImage);
        for(auto extension:AssetRuntime::ModelExtensions())resources.RegisterResourceNewFunctionPointer(extension.data(),NewThing);
        window=CreateWindowW(L"STATIC",L"G1 production material proof",WS_OVERLAPPEDWINDOW,0,0,960,640,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend;Check(window&&backend.Initialize({window,960,640}),"Diligent backend");
        {DiligentStaticObjectRenderer renderer(backend);Check(renderer.Initialize(),"production PBR/Classic pipelines");staticObjectRenderer=&renderer;
            if(argc==2){Channels(backend,renderer);BadOptionalMaps(renderer);}Balls(backend,renderer,resources,argv[1],argc==3);staticObjectRenderer=nullptr;}
        backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(liveMaterialRuntimeObjects==0&&livePBRBindings==0&&livePBRPipelines==0&&AssetRuntime::liveDocuments==0,"all material/PSO/document owners zero");
        std::cout<<"PASS production PBR channels, six materials, live style, no per-frame creation, resources=0\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';if(window)DestroyWindow(window);return 1;}
}
