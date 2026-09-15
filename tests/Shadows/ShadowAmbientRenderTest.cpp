#include "ShadowFixture.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
#include "Renderer/DiligentTerrainRenderer.h"
#include "Renderer/SceneLightingRuntime.h"
#include "Renderer/ShadowAmbientRuntime.h"
#include "Renderer/ActorRenderData.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include "Platform/PlatformWindow.h"
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace Renderer;
using namespace ShadowFixture;
using RGB=std::vector<std::uint8_t>;
static unsigned diligentWarnings{};
static void DILIGENT_CALL_TYPE DebugMessage(Diligent::DEBUG_MESSAGE_SEVERITY severity,const char* message,const char*,const char*,int){
    if(severity>=Diligent::DEBUG_MESSAGE_SEVERITY_WARNING){++diligentWarnings;std::cerr<<message<<'\n';}
}
namespace Renderer {
class BackendTestAccess {public:
    static void MissingDepth(DiligentD3D11Backend& b){b.m_impl->depthEffects.Composite(b.m_impl->swapChain,nullptr);}
};
}
static void Check(bool v,const char* message){if(!v)throw std::runtime_error(message);}
static RGB Capture(DiligentD3D11Backend& backend,const std::string& name){
    RGB rgb;unsigned width{},height{};Check(backend.CaptureRGB(rgb,width,height),"read framebuffer");
    std::array<unsigned char,54> header{};header[0]='B';header[1]='M';
    const auto put=[&](unsigned offset,std::uint32_t value){for(unsigned i=0;i<4;++i)header[offset+i]=static_cast<unsigned char>(value>>(i*8));};
    put(2,54+width*height*3);put(10,54);put(14,40);put(18,width);put(22,std::uint32_t(-int(height)));header[26]=1;header[28]=24;
    RGB bgr=rgb;for(std::size_t p=0;p<bgr.size();p+=3)std::swap(bgr[p],bgr[p+2]);
    std::ofstream out(name+".bmp",std::ios::binary);out.write(reinterpret_cast<const char*>(header.data()),header.size());out.write(reinterpret_cast<const char*>(bgr.data()),bgr.size());return rgb;
}
static StaticObjectSource Cube(){
    StaticObjectSource s;
    const auto face=[&](std::array<Vector3,4> p,Vector3 n){auto first=unsigned(s.vertices.size());for(unsigned i=0;i<4;++i)s.vertices.push_back({p[i][0],p[i][1],p[i][2],n[0],n[1],n[2],float(i==1||i==2),float(i>=2)});
        for(unsigned index:{0,1,2,0,2,3})s.indices.push_back(std::uint16_t(first+index));};
    face({Vector3{-50,-50,100},{50,-50,100},{50,50,100},{-50,50,100}},{0,0,1});
    face({Vector3{-50,-50,0},{50,-50,0},{50,-50,100},{-50,-50,100}},{0,-1,0});
    face({Vector3{50,-50,0},{50,50,0},{50,50,100},{50,-50,100}},{1,0,0});
    face({Vector3{50,50,0},{-50,50,0},{-50,50,100},{50,50,100}},{0,1,0});
    face({Vector3{-50,50,0},{-50,-50,0},{-50,-50,100},{-50,50,100}},{-1,0,0});return s;
}
static StaticObjectSource Ground(){StaticObjectSource s;s.vertices={{-800,-800,0,0,0,1,0,0},{800,-800,0,0,0,1,1,0},{800,800,0,0,0,1,1,1},{-800,800,0,0,0,1,0,1}};s.indices={0,1,2,0,2,3};return s;}
static TerrainTexturePtr Pixel(ITextureUploader& renderer,std::array<std::uint8_t,4> rgba){TerrainTextureData d;d.width=d.height=1;d.format=TerrainTextureFormat::RGBA8;d.mips.push_back({rgba.data(),4,4});return renderer.UploadTexture(d);}
struct Difference {unsigned pixels{};double x{},y{},sum{};};
static Difference DifferenceFrom(const RGB& reference,const RGB& shadow,unsigned width=320){Difference d;
    for(std::size_t i=0;i<reference.size();i+=3){int diff=int(reference[i])-int(shadow[i]);if(diff<3)continue;auto pixel=i/3;d.pixels++;d.x+=(pixel%width)*diff;d.y+=(pixel/width)*diff;d.sum+=diff;}
    if(d.sum){d.x/=d.sum;d.y/=d.sum;}return d;
}
static void TerrainProof(DiligentD3D11Backend& backend){
    DiligentTerrainRenderer renderer(backend);Check(renderer.Initialize(),"terrain shadow proof renderer");
    std::array<std::array<float,6>,289> vertices{};std::vector<std::uint16_t> indices;
    for(unsigned y=0;y<17;++y)for(unsigned x=0;x<17;++x){float px=-800+x*100.f,py=-800+y*100.f;
        float height=160*std::max(0.f,1-std::abs(px)/200)*std::max(0.f,1-std::abs(py)/300);
        vertices[y*17+x]={px,py,height,0,0,1};
        if(x<16&&y<16){auto a=std::uint16_t(y*17+x);for(unsigned i:{unsigned(a),unsigned(a+1),unsigned(a+18),unsigned(a),unsigned(a+18),unsigned(a+17)})indices.push_back(std::uint16_t(i));}}
    auto vb=renderer.UploadVertices(vertices.data(),289,24),ib=renderer.UploadIndices(indices.data(),unsigned(indices.size()));
    auto color=Pixel(renderer,{175,175,175,255});std::uint8_t alpha=255;TerrainTextureData mask;mask.width=mask.height=1;mask.format=TerrainTextureFormat::Alpha8;mask.mips.push_back({&alpha,1,1});
    auto alphaMap=renderer.UploadTexture(mask);auto material=renderer.CreateSplatMaterial(color,alphaMap);TerrainSplatParameters params;params.blend=false;params.alphaReference=-1;
    Graphics::SceneLighting light;light.sun.direction={-1,0,.3f};light.sun.intensity=2;sceneLighting.Set(light);
    const auto view=View(),projection=Projection();
    const auto render=[&](bool shadow){Graphics::GraphicsSettings settings;settings.style=Graphics::GraphicsStyle::Modern;settings.shadows=shadow?Graphics::ShadowQuality::High:Graphics::ShadowQuality::Off;
        ApplyGraphicsRuntimeConfig(Graphics::Resolve(settings,shadow?302:301));Check(backend.BeginFrame(),"terrain shadow frame");backend.Clear({true,ClearColor{.1f,.15f,.2f,1}});renderer.ResetFrame();
        unsigned count=backend.BeginModernScene(view,projection);
        for(unsigned c=0;c<count;++c){backend.BeginSunCascade(c);renderer.BeginTerrain({Identity4,view,projection},true);renderer.DrawTerrain(vb,ib,unsigned(indices.size()),false);}
        backend.EndSunCascades();renderer.BeginTerrain({Identity4,view,projection},true);renderer.SetSplatVertices(nullptr,0);renderer.DrawSplat(vb,ib,unsigned(indices.size()),false,material,params);backend.EndModernScene();
        Check(!renderer.Failed(),"production hill caster and receiver");auto rgb=Capture(backend,shadow?"g34-terrain-hill-shadow":"g34-terrain-hill-off");backend.EndFrame();backend.Present();return rgb;};
    auto off=render(false),shadow=render(true);const auto difference=DifferenceFrom(off,shadow);
    std::cout<<"Terrain hill shadow pixels="<<difference.pixels<<'\n';Check(difference.pixels>20&&difference.pixels<320*240/5,"terrain relief casts localized shadows on terrain");
    renderer.ReleaseSplatMaterial(material);renderer.ReleaseTexture(color);renderer.ReleaseTexture(alphaMap);vb.reset();ib.reset();
}
int main(){try{
    Diligent::GetEngineFactoryD3D11()->SetMessageCallback(DebugMessage);
    Platform::PlatformWindow window;Platform::WindowCreateInfo info;info.title="G34 shadow and ambient GPU proof";Check(window.Create(info),"window");window.SetSize(320,240);
    DiligentD3D11Backend backend;Check(backend.Initialize({window.GetNativeHandle().value,320,240}),"D3D11 backend");
    {
        DiligentStaticObjectRenderer renderer(backend);Check(renderer.Initialize(true),"production shaders");
        auto groundSource=Ground(),cubeSource=Cube();auto ground=renderer.UploadGeometry(groundSource),cube=renderer.UploadGeometry(cubeSource);
        auto gray=Pixel(renderer,{180,180,180,255}),red=Pixel(renderer,{160,90,55,255});
        auto groundMaterial=std::make_shared<MaterialRuntime>(),cubeMaterial=std::make_shared<MaterialRuntime>();
        groundMaterial->maps[0]=groundMaterial->classicDiffuse=gray;cubeMaterial->maps[0]=cubeMaterial->classicDiffuse=red;
        StaticObjectDraw gd,cd;gd.material=groundMaterial;cd.material=cubeMaterial;
        gd.matrices=cd.matrices={Identity4,View(),Projection()};gd.normalTransform=cd.normalTransform=Identity4;gd.cull=cd.cull=StaticObjectCull::None;
        gd.vertexCount=4;gd.indexCount=6;cd.vertexCount=unsigned(cubeSource.vertices.size());cd.indexCount=unsigned(cubeSource.indices.size());
        cd.viewport={0,0,320,240}; // Native actor snapshots explicitly carry the full viewport.
        Graphics::SceneLighting light;light.ambientSkyColor={.3f,.3f,.3f};light.ambientGroundColor={.15f,.15f,.15f};light.sun.intensity=2;
        std::uint64_t revision=20;
        std::function<void()> prepareSkin;bool missingDepth=false;
        auto render=[&](Vector3 sun,unsigned shadows,unsigned ao,const std::string& name){
            light.sun.direction=sun;sceneLighting.Set(light);Graphics::GraphicsSettings s;s.style=Graphics::GraphicsStyle::Modern;s.shadows=Graphics::ShadowQuality(shadows);s.ambientOcclusion=Graphics::AmbientOcclusionQuality(ao);
            ApplyGraphicsRuntimeConfig(Graphics::Resolve(s,++revision));Check(backend.BeginFrame(),"begin");backend.Clear({true,ClearColor{.1f,.15f,.2f,1}});
            if(prepareSkin)prepareSkin();
            unsigned count=backend.BeginModernScene(gd.matrices.view,gd.matrices.projection);
            for(unsigned c=0;c<count;++c){Check(backend.BeginSunCascade(c),"cascade target");renderer.Draw(ground,gray,gd);renderer.Draw(cube,red,cd);}
            backend.EndSunCascades();renderer.Draw(ground,gray,gd);renderer.Draw(cube,red,cd);
            if(missingDepth)BackendTestAccess::MissingDepth(backend);else backend.EndModernScene();Check(!renderer.Failed(),"shadow/ambient draws");
            auto image=Capture(backend,name);std::cout<<name<<" draws="<<shadowDraws<<" culled="<<shadowCulled<<" shadowBytes="<<shadowMemory<<" aoBytes="<<aoMemory<<" aoCpuUs="<<aoCpuUs<<'\n';backend.EndFrame();backend.Present();return image;
        };
        const auto leftOff=render({-1,0,1},0,0,"g34-left-off"),left=render({-1,0,1},4,0,"g34-left-shadow");
        const auto rightOff=render({1,0,1},0,0,"g34-right-off"),right=render({1,0,1},4,0,"g34-right-shadow");
        auto ld=DifferenceFrom(leftOff,left),rd=DifferenceFrom(rightOff,right);std::cout<<"left pixels="<<ld.pixels<<" centroid="<<ld.x<<","<<ld.y<<" right pixels="<<rd.pixels<<" centroid="<<rd.x<<","<<rd.y<<'\n';
        Check(ld.pixels>30&&rd.pixels>30&&std::abs(ld.x-rd.x)>8,"opposite sun directions move GPU shadow");
        const auto lowOff=render({-1,0,.3f},0,0,"g34-low-off"),low=render({-1,0,.3f},4,0,"g34-low-shadow");
        Check(DifferenceFrom(lowOff,low).pixels>ld.pixels*1.25,"low sun gives longer projected shadow");
        const auto highAO=render({-1,0,1},4,2,"g34-high-ao"),lowAO=render({-1,0,1},4,1,"g34-low-ao");
        auto ad=DifferenceFrom(left,highAO);std::cout<<"AO affected="<<ad.pixels<<" darkness="<<ad.sum<<'\n';Check(ad.pixels>10&&ad.pixels<320*240/5,"AO visible and localized");
        ambientDebugView=1;render({-1,0,1},4,2,"g34-ao-only");ambientDebugView=0;
        missingDepth=true;Check(render({-1,0,1},4,2,"g34-missing-depth")==left,"missing depth safely preserves the lit shadowed scene");missingDepth=false;
        light.ambientIntensity=0;const auto noAmbient=render({-1,0,1},4,0,"g34-direct-only"),withAO=render({-1,0,1},4,2,"g34-direct-ao");Check(noAmbient==withAO,"AO leaves direct sunlight unchanged");light.ambientIntensity=1;
        for(unsigned quality:{0u,1u,4u,5u,0u,4u})render({-1,0,1},quality,quality?1:0,"g34-toggle");
        Check(backend.Resize(0,0)&&!backend.BeginFrame()&&backend.Resize(320,240),"minimize restore AO targets");
        render({-1,0,1},4,2,"g34-restored");
        Check(backend.Resize(400,300),"resize to new viewport");
        render({-1,0,1},4,2,"g34-resized");Check(aoMemory==400*300*13,"AO targets track resized viewport");
        Check(backend.Resize(320,240),"restore original viewport");
        backend.ResetModernScene();Check(liveAOTargets==0&&liveShadowMaps==1,"map reset frees targets, neutral depth remains");
        // Leaf-card alpha is evaluated by the same material policy in both passes.
        auto leaf=Ground();for(auto& v:leaf.vertices){v[0]*=.1f;v[1]*=.1f;v[2]=120;}
        cube=renderer.UploadGeometry(leaf);cd.vertexCount=4;cd.indexCount=6;cd.textureAlpha=true;cd.alphaTest=StaticObjectAlphaTest::Greater;cd.alphaReference=127;
        const auto solidOff=render({-1,0,1},0,0,"g34-leaf-solid-off"),solid=render({-1,0,1},4,0,"g34-leaf-solid-shadow");
        std::array<std::uint8_t,16*16*4> mask{};for(unsigned y=0;y<16;++y)for(unsigned x=0;x<16;++x){unsigned i=(y*16+x)*4;mask[i]=160;mask[i+1]=90;mask[i+2]=55;mask[i+3]=(x>=6&&x<=9)||(y>=6&&y<=9)?255:0;}
        TerrainTextureData td;td.width=td.height=16;td.format=TerrainTextureFormat::RGBA8;td.mips.push_back({mask.data(),mask.size(),64});red=renderer.UploadTexture(td);Check(bool(red),"alpha fixture upload");
        cubeMaterial=std::make_shared<MaterialRuntime>();cubeMaterial->maps[0]=cubeMaterial->classicDiffuse=red;cd.material=cubeMaterial;
        const auto maskOff=render({-1,0,1},0,0,"g34-leaf-mask-off"),masked=render({-1,0,1},4,0,"g34-leaf-mask-shadow");
        Check(DifferenceFrom(maskOff,masked).pixels<DifferenceFrom(solidOff,solid).pixels*.8,"alpha-masked shadow excludes card rectangle");
        cd.cull=StaticObjectCull::Clockwise;const auto oneSided=render({-1,0,1},4,0,"g34-leaf-front");
        Check(oneSided==masked,"shadow/main double-sided front winding agrees");cd.cull=StaticObjectCull::None;
        // A single current GPU palette is consumed by shadow and main passes.
        auto skeleton=std::make_shared<SkeletonLayout>();skeleton->names={"root"};skeleton->parents={-1};
        auto skinMesh=std::make_shared<StaticSkinnedMeshData>();skinMesh->meshBoneCount=1;skinMesh->meshToSourceSkeleton={0};skinMesh->indices=leaf.indices;
        for(const auto& v:leaf.vertices){SkinningVertex sv{};std::copy_n(v.data(),3,sv.position);std::copy_n(v.data()+3,3,sv.normal);std::copy_n(v.data()+6,2,sv.uv);sv.weights[0]=255;skinMesh->vertices.push_back(sv);}
        SkinningModelData skinData;skinData.skeleton=skeleton;skinData.meshes={skinMesh};skinData.status={SkinDataStatus::Ready};
        auto remap=std::make_shared<BoneRemap>();remap->destination=skeleton;remap->meshToSkeleton={0};
        BonePalette palette;palette.skeleton=skeleton;palette.ready=true;palette.revision=1;palette.matrices={Identity4};cube.reset();
        prepareSkin=[&]{Check(renderer.PreparePrototype(cube,skinData,{remap},palette),"GPU caster palette preparation");};
        auto skinOff=render({-1,0,1},0,0,"g34-skin-rest-off"),skinRest=render({-1,0,1},4,0,"g34-skin-rest");
        Check(skinRest==masked,"GPU and rigid masked casters match exactly at rest");
        palette.matrices[0][12]=160;++palette.revision;
        auto posedOff=render({-1,0,1},0,0,"g34-skin-posed-off"),posed=render({-1,0,1},4,0,"g34-skin-posed");
        Check(std::abs(DifferenceFrom(posedOff,posed).x-DifferenceFrom(skinOff,skinRest).x)>15,"shadow follows the animated GPU palette");
        prepareSkin={};
        cd.material.reset();gd.material.reset();cubeMaterial.reset();groundMaterial.reset();cube.reset();ground.reset();red.reset();gray.reset();renderer.ReleaseBindings();renderer.ResetFrame();
    }
    TerrainProof(backend);
    backend.Shutdown();Check(liveShadowMaps==0&&liveShadowViews==0&&liveShadowPipelines==0&&liveShadowBuffers==0&&liveAOTargets==0&&liveAOViews==0&&liveAOPipelines==0&&liveAOBuffers==0,"all depth-effect resources zero");
    Check(diligentWarnings==0,"no Diligent shadow/AO validation warnings or errors");
    std::cout<<"PASS G34 GPU sun direction, length, contact AO, live quality and resources\n";return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
