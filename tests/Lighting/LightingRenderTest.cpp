#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentTerrainRenderer.h"
#include "Renderer/SceneLightingRuntime.h"
#include "Renderer/ActorRenderData.h"
#include "Platform/PlatformWindow.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace Renderer;
using RGB=std::vector<std::uint8_t>;
static constexpr std::array<float,16> I{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static void Check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static void Style(bool modern){Graphics::GraphicsSettings s;s.style=modern?Graphics::GraphicsStyle::Modern:Graphics::GraphicsStyle::Classic;ApplyGraphicsRuntimeConfig(Graphics::Resolve(s,modern?2:1));}
static int Center(const RGB& rgb,unsigned c=0){return rgb[(64*128+64)*3+c];}
static RGB Capture(DiligentD3D11Backend& backend,const std::string& name={})
{
    RGB rgb;unsigned w{},h{};Check(backend.CaptureRGB(rgb,w,h)&&w==128&&h==128,"read actual framebuffer");
    if(!name.empty()){
        // Lossless, top-down 24-bit BMP, directly from the GPU readback.
        std::array<unsigned char,54> header{};header[0]='B';header[1]='M';
        const auto put=[&](unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)header[offset+i]=static_cast<unsigned char>(value>>(i*8));};
        put(2,54+w*h*3);put(10,54);put(14,40);put(18,w);put(22,std::uint32_t(-int(h)));header[26]=1;header[28]=24;
        RGB bgr=rgb;for(std::size_t p=0;p<bgr.size();p+=3)std::swap(bgr[p],bgr[p+2]);
        std::ofstream out(name,std::ios::binary);out.write(reinterpret_cast<const char*>(header.data()),header.size());out.write(reinterpret_cast<const char*>(bgr.data()),bgr.size());
    }return rgb;
}
static TerrainTexturePtr Pixel(ITextureUploader& renderer,std::array<std::uint8_t,4> rgba={180,180,180,255})
{TerrainTextureData data;data.width=data.height=1;data.format=TerrainTextureFormat::RGBA8;data.mips.push_back({rgba.data(),4,4});return renderer.UploadTexture(data);}
static TerrainTexturePtr BlackWhite(ITextureUploader& renderer)
{const std::array<std::uint8_t,8> pixels{0,0,0,255,255,255,255,255};TerrainTextureData data;data.width=2;data.height=1;data.format=TerrainTextureFormat::RGBA8;data.mips.push_back({pixels.data(),8,8});return renderer.UploadTexture(data);}
static StaticObjectSource Quad(std::array<float,3> normal={0,0,-1},bool auxiliary=false)
{
    StaticObjectSource result;
    for(auto p:{std::array<float,2>{-.8f,-.8f},{-.8f,.8f},{.8f,.8f},{.8f,-.8f}})
        result.vertices.push_back({p[0],p[1],.5f,normal[0],normal[1],normal[2],(p[0]+.8f)/1.6f,(p[1]+.8f)/1.6f});
    result.indices={0,2,1,0,3,2};if(auxiliary)result.vertexExtras.resize(4);return result;
}
static StaticObjectDraw DrawState(const MaterialRuntimePtr& material)
{
    StaticObjectDraw draw;draw.material=material;draw.matrices={I,I,I};draw.normalTransform=I;draw.cull=StaticObjectCull::None;
    draw.vertexCount=4;draw.indexCount=6;return draw;
}
static void Begin(DiligentD3D11Backend& backend){Check(backend.BeginFrame(),"begin frame");backend.Clear({true,ClearColor{0,0,0,1}});}
static void End(DiligentD3D11Backend& backend){backend.EndFrame();backend.Present();}
static void NormalPixel(const RGB& rgb,std::array<float,3> expected,const char* why)
{
    const float length=std::hypot(expected[0],expected[1],expected[2]);
    for(unsigned c=0;c<3;++c)Check(std::abs(Center(rgb,c)-int((expected[c]/length*.5f+.5f)*255+.5f))<=3,why);
}
static std::array<float,3> MappedExpected(std::array<float,3> n,std::array<float,3> t,float sign,bool back)
{
    const auto normalize=[](auto v){const float length=std::hypot(v[0],v[1],v[2]);for(auto& c:v)c/=length;return v;};
    n=normalize(n);float dot=n[0]*t[0]+n[1]*t[1]+n[2]*t[2];for(unsigned c=0;c<3;++c)t[c]-=n[c]*dot;t=normalize(t);
    const std::array<float,3> b{(n[1]*t[2]-n[2]*t[1])*sign,(n[2]*t[0]-n[0]*t[2])*sign,(n[0]*t[1]-n[1]*t[0])*sign};
    const auto sample=normalize(std::array<float,3>{210.f/255*2-1,128.f/255*2-1,230.f/255*2-1});
    std::array<float,3> result;for(unsigned c=0;c<3;++c)result[c]=(t[c]*sample[0]+b[c]*sample[1]+n[c]*sample[2])*(back?-1:1);return result;
}
static void Rigid(DiligentD3D11Backend& backend,DiligentStaticObjectRenderer& renderer)
{
    auto texture=Pixel(renderer);auto material=std::make_shared<MaterialRuntime>();material->maps[0]=material->classicDiffuse=texture;
    auto geometry=renderer.UploadGeometry(Quad());auto draw=DrawState(material);Style(true);
    const auto render=[&](const std::string& file={}){Begin(backend);renderer.Draw(geometry,texture,draw);Check(!renderer.Failed(),"production rigid lighting");auto rgb=Capture(backend,file);End(backend);return rgb;};
    Graphics::SceneLighting light;light.ambientIntensity=0;
    std::array<int,4> brightness{};unsigned index=0;
    for(const auto dir:{Graphics::LightVector{0,0,-1},{1,0,0},{0,0,1},{0,1,0}}){light.sun.direction=dir;sceneLighting.Set(light);brightness[index]=Center(render("g2x-sun-"+std::to_string(index)+".bmp"));++index;}
    Check(brightness[0]>75&&brightness[1]<2&&brightness[2]<2&&brightness[3]<2,"front versus side/back/top respects N dot L");
    light.sun.direction={.4f,0,-1};sceneLighting.Set(light);auto before=render();
    draw.matrices.view={0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1};auto roll=render();
    for(unsigned c=0;c<3;++c)Check(std::abs(Center(before,c)-Center(roll,c))<=1,"camera roll does not move world sun");draw.matrices.view=I;
    geometry=renderer.UploadGeometry(Quad({.6f,.2f,-.774596669f}));materialDebugView=MaterialDebugView::Normal;
    const auto doubleSided=render();draw.cull=StaticObjectCull::Clockwise;
    Check(render()==doubleSided,"disabling culling preserves front-face normals");draw.cull=StaticObjectCull::None;
    draw.matrices.world={0,2,0,0,-.5f,0,0,0,0,0,1,0,0,0,0,1};
    NormalPixel(render("g2x-normal-nonuniform.bmp"),{-.4f,.3f,-.774596669f},"inverse transpose for rotation and nonuniform world scale");
    draw.matrices.world[12]=.1f;NormalPixel(render(),{-.4f,.3f,-.774596669f},"translation never enters normals");
    draw.matrices.world=I;draw.matrices.world[0]=-1;
    NormalPixel(render("g2x-normal-mirror.bmp"),{.6f,-.2f,.774596669f},"mirrored world plus backface policy");
    draw.matrices.world=I;materialDebugView=MaterialDebugView::Lit;
    light.sun.intensity=0;light.ambientIntensity=1;light.ambientSkyColor={.5f,.5f,.5f};light.ambientGroundColor={.05f,.05f,.05f};sceneLighting.Set(light);
    geometry=renderer.UploadGeometry(Quad({0,0,1}));const int sky=Center(render());
    geometry=renderer.UploadGeometry(Quad());const int ground=Center(render());Check(sky>ground+50&&ground>10,"world-Z hemisphere leaves finite ground-side ambient");
    // Twenty submissions share the single scene buffer; warm toggles create no PSOs.
    render();const auto uploads=lightBufferUpdates.load(),pipelines=livePBRPipelines.load(),lightPipelines=liveLightingPipelines.load();
    for(bool modern:{false,true,false,true}){
        Style(modern);renderer.ResetFrame();auto start=std::chrono::steady_clock::now();
        Begin(backend);for(unsigned actor=0;actor<20;++actor)renderer.Draw(geometry,texture,draw);backend.EndFrame();
        auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
        Check(renderer.DrawCount()==20&&liveLightBuffers==1&&livePBRPipelines==pipelines&&liveLightingPipelines==lightPipelines&&lightBufferUpdates==uploads,"20 draws and live toggle reuse scene buffer and pipelines");
        std::cout<<(modern?"Modern":"Classic")<<" CPU_submit_us="<<us<<" draws=20 PBR_PSOs="<<pipelines<<" light_uploads=0\n";
        backend.Present();
    }
    Check(backend.Resize(0,0)&&backend.Resize(128,128),"Modern suspend/restore");
    draw.material.reset();material.reset();geometry.reset();texture.reset();renderer.ReleaseBindings();renderer.ResetFrame();
}
static void Skinned(DiligentD3D11Backend& backend,DiligentStaticObjectRenderer& renderer)
{
    auto skeleton=std::make_shared<SkeletonLayout>();skeleton->names={"arm"};skeleton->parents={-1};
    auto mesh=std::make_shared<StaticSkinnedMeshData>();mesh->meshBoneCount=1;mesh->meshToSourceSkeleton={0};mesh->indices={0,2,1,0,3,2};
    for(const auto& v:Quad({.6f,.2f,-.774596669f}).vertices){SkinningVertex s{};std::copy_n(v.data(),3,s.position);std::copy_n(v.data()+3,3,s.normal);std::copy_n(v.data()+6,2,s.uv);s.weights[0]=255;mesh->vertices.push_back(s);}
    SkinningModelData data;data.skeleton=skeleton;data.meshes={mesh};data.status={SkinDataStatus::Ready};
    ActorModelSource source;source.vertexCount=source.deformVertexCount=4;source.indices=mesh->indices;source.materialVertices.resize(4);
    for(unsigned i=0;i<4;++i){source.materialVertices[i].tangent={.316227766f,-.948683298f,0,1};source.materialVertices[i].uv={mesh->vertices[i].uv[0],mesh->vertices[i].uv[1]};}
    auto remap=std::make_shared<BoneRemap>();remap->destination=skeleton;remap->meshToSkeleton={0};
    BonePalette palette;palette.skeleton=skeleton;palette.ready=true;palette.revision=1;palette.matrices={I};
    auto texture=Pixel(renderer);auto material=std::make_shared<MaterialRuntime>();material->maps[0]=material->classicDiffuse=texture;
    auto draw=DrawState(material);StaticObjectGeometryPtr geometry;materialDebugView=MaterialDebugView::Normal;Style(true);
    const auto frame=[&](const std::string& name){Begin(backend);Check(renderer.PreparePrototype(geometry,data,{remap},palette,&source),"real GPU palette and mesh setup");renderer.Draw(geometry,texture,draw);Check(!renderer.Failed(),"production GPU PBR normals");auto rgb=Capture(backend,name);End(backend);return rgb;};
    NormalPixel(frame("g2x-skinned-rest.bmp"),{.6f,.2f,-.774596669f},"rest normal");
    palette.matrices[0]={0,2,0,0,-.5f,0,0,0,0,0,1,0,0,0,0,1};++palette.revision;
    NormalPixel(frame("g2x-skinned-posed.bmp"),{-.4f,.3f,-.774596669f},"posed nonuniform bone normal follows deformation");
    draw.material.reset();material.reset();renderer.ResetFrame();material=std::make_shared<MaterialRuntime>();material->maps[0]=material->classicDiffuse=texture;
    material->maps[1]=Pixel(renderer,{210,128,230,255});draw.material=material;
    NormalPixel(frame("g2x-skinned-normalmap.bmp"),MappedExpected({-.4f,.3f,-.774596669f},{.474341649f,.632455532f,0},1,false),"authored tangent follows posed bone with normalmap");
    palette.matrices[0]=I;palette.matrices[0][0]=-1;++palette.revision;
    NormalPixel(frame("g2x-skinned-mirrored-normalmap.bmp"),MappedExpected({-.6f,.2f,-.774596669f},{-.316227766f,-.948683298f,0},-1,true),"mirrored skin tangent sign and complete mapped backface normal flip");
    materialDebugView=MaterialDebugView::Lit;geometry.reset();draw.material.reset();material.reset();texture.reset();renderer.ReleaseBindings();renderer.ResetFrame();
}
static void Vegetation(DiligentD3D11Backend& backend,DiligentStaticObjectRenderer& renderer)
{
    auto geometry=renderer.UploadGeometry(Quad({0,0,-1},true));auto texture=Pixel(renderer);auto draw=DrawState({});
    Graphics::SceneLighting light;light.sun.direction={0,0,-1};sceneLighting.Set(light);
    const auto run=[&](bool modern,const std::string& name){Style(modern);Begin(backend);renderer.Draw(geometry,texture,draw);Check(!renderer.Failed(),"vegetation auxiliary render");auto rgb=Capture(backend,name);End(backend);return rgb;};
    const auto classic=run(false,"g2x-vegetation-classic.bmp"),front=run(true,"g2x-vegetation-modern.bmp");
    light.sun.direction={0,0,1};sceneLighting.Set(light);const auto back=run(true,{});
    Check(Center(front)>Center(back)+25&&Center(back)>10,"vegetation responds to same sun and ambient");
    Check(run(false,{})==classic,"vegetation Classic ignores scene light changes");
    draw.matrices.world[0]=-1;const auto flipped=run(true,{});Check(Center(flipped)>Center(back)+25,"two-sided vegetation reverses backface normal");
    // Black/white halfway filtering must match linear 0.5 (sRGB 188), not encoded 128.
    auto midpoint=Quad({0,0,-1},true);for(auto& v:midpoint.vertices)v[6]=v[7]=.5f;
    geometry=renderer.UploadGeometry(midpoint);draw.matrices.world=I;draw.sampling.linearMin=draw.sampling.linearMag=true;
    texture=Pixel(renderer,{188,188,188,255});const auto gray=run(true,{});
    texture=BlackWhite(renderer);Check(std::abs(Center(run(true,{}))-Center(gray))<=2,"vegetation color decodes before filtering");
    texture=Pixel(renderer,{255,255,255,255});for(auto& v:midpoint.vertexExtras)v.color={.5f,.5f,.5f,1};
    geometry=renderer.UploadGeometry(midpoint);
    Check(std::abs(Center(run(true,{}))-Center(gray))<=2,"vegetation vertex tint is a linear factor, not sRGB input");
    for(auto& v:midpoint.vertexExtras)v.color={1,1,1,1};geometry=renderer.UploadGeometry(midpoint);
    texture=Pixel(renderer,{255,255,255,255});draw.cameraAlphaTransform={};draw.cameraAlphaTransform[12]=draw.cameraAlphaTransform[13]=.5f;
    draw.cameraAlphaSampling.linearMin=draw.cameraAlphaSampling.linearMag=true;
    draw.vertexShadow=Pixel(renderer,{188,188,188,255});const auto grayShadow=run(true,{});
    draw.vertexShadow=BlackWhite(renderer);Check(std::abs(Center(run(true,{}))-Center(grayShadow))<=2,"vegetation shadow color decodes before filtering");
    draw.vertexShadow.reset();
    geometry.reset();texture.reset();renderer.ReleaseBindings();renderer.ResetFrame();
}
static void Terrain(DiligentD3D11Backend& backend)
{
    DiligentTerrainRenderer renderer(backend);Check(renderer.Initialize(),"terrain pipelines");
    std::array<std::array<float,6>,289> vertices{};
    for(unsigned y=0;y<17;++y)for(unsigned x=0;x<17;++x)vertices[y*17+x]={-.8f+x*.1f,-.8f+y*.1f,.5f,0,0,-1};
    const std::array<std::uint16_t,6> indices{0,16,288,0,288,272};
    auto vb=renderer.UploadVertices(vertices.data(),289,24),ib=renderer.UploadIndices(indices.data(),6);
    auto color=Pixel(renderer);const std::uint8_t alpha=255;TerrainTextureData mask;mask.width=mask.height=1;mask.format=TerrainTextureFormat::Alpha8;mask.mips.push_back({&alpha,1,1});
    auto alphaMap=renderer.UploadTexture(mask);auto material=renderer.CreateSplatMaterial(color,alphaMap);Check(bool(material),"terrain splat material");
    TerrainSplatParameters params;params.blend=false;params.alphaReference=-1;
    const auto run=[&](bool modern,const std::string& name){Style(modern);Begin(backend);renderer.BeginTerrain({I,I,I},true);renderer.SetSplatVertices(nullptr,0);renderer.DrawSplat(vb,ib,6,false,material,params);Check(!renderer.Failed(),"production terrain lighting");auto rgb=Capture(backend,name);End(backend);return rgb;};
    Graphics::SceneLighting light;light.sun.direction={0,0,-1};sceneLighting.Set(light);
    const auto classic=run(false,"g2x-terrain-classic.bmp"),front=run(true,"g2x-terrain-modern.bmp");
    light.sun.direction={0,0,1};sceneLighting.Set(light);const auto back=run(true,{});
    Check(Center(front)>Center(back)+25&&Center(back)>10,"terrain reads source normals and shared scene lighting");
    Check(run(false,{})==classic,"terrain Classic byte-identical under changed modern scene light");
    params.colorTransform={};params.colorTransform[12]=params.colorTransform[13]=.5f;
    params.colorSampling.linearMin=params.colorSampling.linearMag=true;
    renderer.ReleaseSplatMaterial(material);renderer.ReleaseTexture(color);color=Pixel(renderer,{188,188,188,255});material=renderer.CreateSplatMaterial(color,alphaMap);
    const auto gray=run(true,{});
    renderer.ReleaseSplatMaterial(material);renderer.ReleaseTexture(color);color=BlackWhite(renderer);material=renderer.CreateSplatMaterial(color,alphaMap);
    Check(std::abs(Center(run(true,{}))-Center(gray))<=2,"terrain color decodes before filtering");
    renderer.ReleaseSplatMaterial(material);renderer.ReleaseTexture(color);renderer.ReleaseTexture(alphaMap);vb.reset();ib.reset();
    Check(renderer.LiveMaterialCount()==0&&renderer.LiveTextureCount()==0,"terrain resources zero");
}
int main(){try{
    std::cout<<std::unitbuf;Platform::PlatformWindow window;Platform::WindowCreateInfo info;info.title="G2 lighting proof";
    Check(window.Create(info),"native window");window.SetSize(128,128);DiligentD3D11Backend backend;Check(backend.Initialize({window.GetNativeHandle().value,128,128}),"D3D11 backend");
    {DiligentStaticObjectRenderer renderer(backend);Check(renderer.Initialize(true),"rigid, skinned and vegetation pipelines");
        Rigid(backend,renderer);Skinned(backend,renderer);Vegetation(backend,renderer);Terrain(backend);
        Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0&&livePBRBindings==0,"mesh resources zero");}
    backend.Shutdown();window.Destroy();
    Check(liveLightBuffers==0&&liveLightingPipelines==0&&liveSceneLightingResources==0&&liveMaterialRuntimeObjects==0&&livePBRPipelines==0&&liveSkinMeshes==0&&liveBonePalettes==0&&liveBoneRemaps==0,"complete shutdown zero");
    std::cout<<"PASS world sun, normals, hemisphere, skinning, 20 draws, terrain, vegetation, toggles, resources=0\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
