#include "ShaderLoadAudit.h"
#include "DiligentWater.h"
#include "EterBase/MapLoadTrace.h"
#include "ModernWaterShader.h"
#include "FirstUseAudit.h"
#include "Diagnostics.h"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "PostProcess/ScreenSpaceReflection/interface/ScreenSpaceReflection.hpp"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include <chrono>
#include <cstring>
#include <stdexcept>

namespace Diligent::HLSL {
#include "Shaders/PostProcess/ScreenSpaceReflection/public/ScreenSpaceReflectionStructures.fxh"
}
namespace Renderer
{
using namespace Diligent;
namespace {
constexpr unsigned StreamVertices=32766; // Whole triangles, reused for every patch.
void RequireWater(bool value,const char* name){if(!value)throw std::runtime_error(name);}
struct Constants {
    CameraAttribs camera;ShadowMapAttribs shadows;float4x4 world;
    float4 wave,offset,material,absorption,deepColor,options,sunDirection,sunRadiance,horizon;
};
const auto ClockOrigin=std::chrono::steady_clock::now();
}
struct DiligentWater::Impl
{
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> context;
    RefCntAutoPtr<ITexture> scene,depth,ssrDepth,normals,sceneNormals,roughness,normalMap,black;
    RefCntAutoPtr<IBuffer> constants,vertices;
    RefCntAutoPtr<ISampler> normalSampler,sceneSampler;
    struct Pipeline {RefCntAutoPtr<IPipelineState> pso;RefCntAutoPtr<IShaderResourceBinding> srb;};
    Pipeline prepass,materialPass,composite;
    std::unique_ptr<ScreenSpaceReflection> ssr;
    Graphics::WaterConfig config;
    WaterFrameInputs input;
    Constants data{};
    WaterStats stats;
    unsigned width{},height{};
    bool open{},drawn{};
    Impl(IRenderDevice* d,IDeviceContext* c):device(d),context(c){}
    static void Set(IShaderResourceBinding* srb,SHADER_TYPE stage,const char* name,IDeviceObject* value) {
        if(srb)if(auto* variable=srb->GetVariableByName(stage,name))variable->Set(value);
    }
    void Upload() {
        MapHelper<Constants> map(context,constants,MAP_WRITE,MAP_FLAG_DISCARD);
        RequireWater(bool(map),"water constant map");*map=data;
    }
    RefCntAutoPtr<ITexture> Texture(TEXTURE_FORMAT format,BIND_FLAGS binds,const char* name) {
        TextureDesc desc;desc.Name=name;desc.Type=RESOURCE_DIM_TEX_2D;desc.Width=width;desc.Height=height;desc.Format=format;desc.BindFlags=binds;
        RefCntAutoPtr<ITexture> texture;{ MapLoadTrace::Scope p0lCreate("GPU resources","CreateTexture","gpu-api"); MapLoadTrace::Count("CreateTexture","",0,true); device->CreateTexture(desc,nullptr,&texture); }RequireWater(bool(texture),name);++stats.resourceCreations;return texture;
    }
    RefCntAutoPtr<IShader> Shader(const char* entry,SHADER_TYPE type) {
        FirstUseAudit timing("shader",entry);ShaderCreateInfo ci;ci.Desc.Name=entry;ci.Desc.ShaderType=type;
        ci.Source=modernWaterShader;ci.EntryPoint=entry;ci.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL;
        ci.CompileFlags=SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
        ci.pShaderSourceStreamFactory=&DiligentFXShaderSourceStreamFactory::GetInstance();
        RefCntAutoPtr<IShader> shader;{ MapLoadTrace::Scope p0lCreate("Shaders / PSOs","CreateShader","cpu"); MapLoadTrace::Count("CreateShader","",0,true); device->CreateShader(ci,&shader); }RequireWater(bool(shader),entry);return shader;
    }
    void MakePipeline(Pipeline& result,const char* pixel,TEXTURE_FORMAT format,bool geometry=false) {
        ShaderLoadAudit::Domain p0lDomain("water");
        if(result.pso)return;
        auto vs=Shader(geometry?"WaterVS":"WaterScreenVS",SHADER_TYPE_VERTEX),ps=Shader(pixel,SHADER_TYPE_PIXEL);
        GraphicsPipelineStateCreateInfo ci;ci.PSODesc.Name=pixel;ci.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        ci.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        auto& g=ci.GraphicsPipeline;g.NumRenderTargets=geometry?2:1;g.RTVFormats[0]=format;if(geometry)g.RTVFormats[1]=format;g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        g.RasterizerDesc.CullMode=CULL_MODE_NONE;g.DepthStencilDesc.DepthEnable=geometry;g.DepthStencilDesc.DepthWriteEnable=geometry;
        g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;g.DSVFormat=geometry?TEX_FORMAT_D24_UNORM_S8_UINT:TEX_FORMAT_UNKNOWN;
        LayoutElement layout[]={{0,0,3,VT_FLOAT32,False,0,sizeof(EffectVertex)},{1,0,4,VT_UINT8,True,12,sizeof(EffectVertex)}};
        if(geometry)g.InputLayout={layout,2};ci.pVS=vs;ci.pPS=ps;
        {FirstUseAudit timing("pso",pixel);{ MapLoadTrace::Scope p0lCreate("Shaders / PSOs","CreateGraphicsPipelineState","gpu-api"); MapLoadTrace::Count("CreateGraphicsPipelineState","",0,true); device->CreateGraphicsPipelineState(ci,&result.pso); }}
        RequireWater(bool(result.pso),pixel);ShaderLoadAudit::CreateSRB(result.pso,&result.srb,true);++stats.resourceCreations;
    }
    void SharedResources() {
        if(!constants) {
            BufferDesc desc;desc.Name="G7 shared water constants";desc.Size=sizeof(Constants);desc.Usage=USAGE_DYNAMIC;
            desc.BindFlags=BIND_UNIFORM_BUFFER;desc.CPUAccessFlags=CPU_ACCESS_WRITE;{ MapLoadTrace::Scope p0lCreate("GPU resources","CreateBuffer","gpu-api"); MapLoadTrace::Count("CreateBuffer","",0,true); device->CreateBuffer(desc,nullptr,&constants); }
            desc.Name="G7 shared water vertex stream";desc.Size=StreamVertices*sizeof(EffectVertex);desc.BindFlags=BIND_VERTEX_BUFFER;
            { MapLoadTrace::Scope p0lCreate("GPU resources","CreateBuffer","gpu-api"); MapLoadTrace::Count("CreateBuffer","",0,true); device->CreateBuffer(desc,nullptr,&vertices); }RequireWater(constants&&vertices,"water shared buffers");stats.resourceCreations+=2;
        }
        if(!normalMap) {
            // Deterministic tileable analytic height derivatives, generated once.
            // Two differently scaled/scrolled samples share this mipmapped texture.
            std::array<std::vector<uint8_t>,8> pixels;std::array<TextureSubResData,8> levels;
            for(unsigned mip=0;mip<8;++mip) {
                const unsigned size=128>>mip;pixels[mip].resize(size*size*4);
                for(unsigned y=0;y<size;++y)for(unsigned x=0;x<size;++x) {
                    const float u=(x+.5f)/size,v=(y+.5f)/size,tau=6.28318530718f;
                    float dx=0,dy=0;
                    constexpr float waves[][4]={{1,2,.45f,.2f},{3,-1,.24f,1.7f},{-2,5,.11f,2.1f},{7,3,.065f,4.2f}};
                    for(const auto& wave:waves) {
                        const float filter=std::min(1.f,float(size)/(8*std::max(std::abs(wave[0]),std::abs(wave[1]))));
                        const float slope=std::cos(tau*(u*wave[0]+v*wave[1])+wave[3])*wave[2]*filter;
                        dx+=wave[0]*slope;dy+=wave[1]*slope;
                    }
                    const float length=std::sqrt(dx*dx+dy*dy+1);const float n[]{-dx/length,-dy/length,1/length};
                    for(unsigned c=0;c<3;++c)pixels[mip][(y*size+x)*4+c]=uint8_t(std::clamp((n[c]*.5f+.5f)*255.f,0.f,255.f));
                    pixels[mip][(y*size+x)*4+3]=255;
                }
                levels[mip].pData=pixels[mip].data();levels[mip].Stride=size*4;
            }
            TextureDesc desc;desc.Name="G7 generated tileable wave normals";desc.Type=RESOURCE_DIM_TEX_2D;
            desc.Width=desc.Height=128;desc.MipLevels=8;desc.Format=TEX_FORMAT_RGBA8_UNORM;desc.BindFlags=BIND_SHADER_RESOURCE;desc.Usage=USAGE_IMMUTABLE;
            TextureData source{levels.data(),8};{ MapLoadTrace::Scope p0lCreate("GPU resources","CreateTexture","gpu-api"); MapLoadTrace::Count("CreateTexture","",0,true); device->CreateTexture(desc,&source,&normalMap); }RequireWater(bool(normalMap),"water normal map");
            const float zero[4]{};TextureSubResData blackPixel{zero,16};TextureData blackData{&blackPixel,1};
            desc.Name="G7 no SSR radiance fallback";desc.Width=desc.Height=desc.MipLevels=1;desc.Format=TEX_FORMAT_RGBA32_FLOAT;
            { MapLoadTrace::Scope p0lCreate("GPU resources","CreateTexture","gpu-api"); MapLoadTrace::Count("CreateTexture","",0,true); device->CreateTexture(desc,&blackData,&black); }RequireWater(bool(black),"water fallback");
            SamplerDesc sampler;sampler.AddressU=sampler.AddressV=sampler.AddressW=TEXTURE_ADDRESS_WRAP;
            device->CreateSampler(sampler,&normalSampler);sampler.AddressU=sampler.AddressV=sampler.AddressW=TEXTURE_ADDRESS_CLAMP;
            device->CreateSampler(sampler,&sceneSampler);stats.resourceCreations+=4;
        }
        MakePipeline(prepass,"WaterNormalPS",TEX_FORMAT_RGBA16_FLOAT,true);
        MakePipeline(materialPass,"WaterRoughnessPS",TEX_FORMAT_R8_UNORM);
        MakePipeline(composite,"WaterCompositePS",TEX_FORMAT_RGBA16_FLOAT);
    }
    void Copy(ITexture* from,ITexture* to) {
        CopyTextureAttribs copy;copy.pSrcTexture=from;copy.pDstTexture=to;
        copy.SrcTextureTransitionMode=copy.DstTextureTransitionMode=RESOURCE_STATE_TRANSITION_MODE_TRANSITION;context->CopyTexture(copy);
    }
    float ReadPeak(ITexture* source) {
        // Explicit GPU-test readback only; never allocated by a production frame.
        auto desc=source->GetDesc();desc.Name="G7 diagnostic HDR readback";desc.Usage=USAGE_STAGING;
        desc.BindFlags=BIND_NONE;desc.CPUAccessFlags=CPU_ACCESS_READ;desc.MiscFlags=MISC_TEXTURE_FLAG_NONE;
        RefCntAutoPtr<ITexture> staging;{ MapLoadTrace::Scope p0lCreate("GPU resources","CreateTexture","gpu-api"); MapLoadTrace::Count("CreateTexture","",0,true); device->CreateTexture(desc,nullptr,&staging); }RequireWater(bool(staging),"HDR diagnostic staging");
        context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);Copy(source,staging);context->WaitForIdle();
        MappedTextureSubresource mapped;context->MapTextureSubresource(staging,0,0,MAP_READ,MAP_FLAG_NONE,nullptr,mapped);
        RequireWater(mapped.pData!=nullptr,"HDR diagnostic map");float peak=0;
        for(unsigned y=0;y<desc.Height;++y){const auto* row=reinterpret_cast<const uint16_t*>(static_cast<const uint8_t*>(mapped.pData)+y*mapped.Stride);
            for(unsigned x=0;x<desc.Width;++x)for(unsigned channel=0;channel<3;++channel){const auto value=row[x*4+channel];const int exponent=(value>>10)&31;
                if(exponent==31){waterTestFinite=false;continue;}
                const float number=std::ldexp(float((value&1023)+(exponent?1024:0)),exponent?exponent-25:-24)*((value&32768)?-1.f:1.f);
                peak=std::max(peak,number);}}
        context->UnmapTextureSubresource(staging,0,0);return peak;
    }
    void Screen(Pipeline& pass,ITextureView* target) {
        Set(pass.srb,SHADER_TYPE_PIXEL,"Water",constants);
        context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->SetPipelineState(pass.pso);context->CommitShaderResources(pass.srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->Draw(DrawAttribs{3,DRAW_FLAG_VERIFY_ALL});
    }
    void ClearBindings() {
        for(auto* pass:{&prepass,&materialPass,&composite})for(const char* name:{"Scene","OpaqueDepth","WaterDepth","WaterNormal","Sky","Reflection","ShadowMap","SkySampler","ShadowSampler"})
            Set(pass->srb,SHADER_TYPE_PIXEL,name,nullptr);
        input={};
    }
};
DiligentWater::DiligentWater(IRenderDevice* device,IDeviceContext* context):impl_(std::make_unique<Impl>(device,context)){++liveWaterRenderers;}
DiligentWater::~DiligentWater(){impl_.reset();--liveWaterRenderers;}
WaterStats DiligentWater::Stats() const {return impl_->stats;}
bool DiligentWater::NeedsPrewarm() const {return impl_->config.ssr&&!impl_->ssr;}
void DiligentWater::ResetFrame(){impl_->open=impl_->drawn=false;impl_->ClearBindings();}
void DiligentWater::ReleaseWindowResources() {
    auto& s=*impl_;ResetFrame();s.ssr.reset();s.scene.Release();s.depth.Release();s.ssrDepth.Release();s.normals.Release();s.sceneNormals.Release();s.roughness.Release();s.width=s.height=0;s.stats.targetBytes=0;
}
void DiligentWater::Prepare(unsigned width,unsigned height,const Graphics::GraphicsRuntimeConfig& graphics) {
    auto& s=*impl_;s.config=Graphics::ResolveWater(graphics);
    if(width<2||height<2){ReleaseWindowResources();return;}
    s.SharedResources();
    if(!s.config.ssr)s.ssr.reset();
    if(s.width==width&&s.height==height&&s.scene)return;
    ReleaseWindowResources();s.width=width;s.height=height;
    s.scene=s.Texture(TEX_FORMAT_RGBA16_FLOAT,BIND_RENDER_TARGET|BIND_SHADER_RESOURCE,"G7 opaque HDR refraction source");
    s.depth=s.Texture(TEX_FORMAT_R24G8_TYPELESS,BIND_DEPTH_STENCIL|BIND_SHADER_RESOURCE,"G7 water and opaque depth");
    s.ssrDepth=s.Texture(TEX_FORMAT_R32_FLOAT,BIND_RENDER_TARGET|BIND_SHADER_RESOURCE,"G7 numeric depth for DiligentFX SSR");
    s.normals=s.Texture(TEX_FORMAT_RGBA16_FLOAT,BIND_RENDER_TARGET|BIND_SHADER_RESOURCE,"G7 visible water normals / coverage");
    s.sceneNormals=s.Texture(TEX_FORMAT_RGBA16_FLOAT,BIND_RENDER_TARGET|BIND_SHADER_RESOURCE,"G7 scene and water normals for SSR");
    s.roughness=s.Texture(TEX_FORMAT_R8_UNORM,BIND_RENDER_TARGET|BIND_SHADER_RESOURCE,"G7 water roughness mask");
    s.stats.targetBytes=std::uint64_t(width)*height*33+87380+StreamVertices*sizeof(EffectVertex);
}
void DiligentWater::Begin(const WaterFrameInputs& input) {
    auto& s=*impl_;if(s.open||!s.scene||!input.scene||!input.depth||!input.sky)return;
    s.input=input;s.open=true;s.drawn=false;
    s.context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    s.Copy(input.scene->GetTexture(),s.scene);s.Copy(input.depth->GetTexture(),s.depth);
    if(waterTestReadback){waterTestFinite=true;waterTestInputPeak=s.ReadPeak(s.scene);}
    if(input.normal)s.Copy(input.normal->GetTexture(),s.sceneNormals);
    const float clear[4]{};s.context->ClearRenderTarget(s.normals->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),clear,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const auto& m=s.config.material;auto& data=s.data;data={};data.camera=input.camera;data.shadows=input.shadows;
    data.wave={m.uvScale[0],m.uvScale[1],m.normalStrength,s.config.secondNormal?1.f:0.f};
    const double seconds=waterTestSeconds>=0?waterTestSeconds:std::chrono::duration<double>(std::chrono::steady_clock::now()-ClockOrigin).count();
    const auto offset=Graphics::WaterUVOffset(seconds,m);data.offset={offset[0],offset[1],offset[2],offset[3]};
    data.material={m.roughness,m.refractionPixels,m.shoreDistance,m.maxDepth};
    data.absorption={m.absorption[0],m.absorption[1],m.absorption[2],float(waterTestView)};data.deepColor={m.deepColor[0],m.deepColor[1],m.deepColor[2],0};
    data.options={s.config.refraction?1.f:0.f,s.config.absorption?1.f:0.f,0,input.shadowsEnabled?1.f:0.f};
    const auto light=Graphics::ValidateSceneLighting(input.lighting);
    data.sunDirection={light.sunDirection[0],light.sunDirection[1],light.sunDirection[2],0};
    data.sunRadiance={light.sunColor[0]*light.sunIntensity,light.sunColor[1]*light.sunIntensity,light.sunColor[2]*light.sunIntensity,0};
    data.horizon={std::pow(light.fogColor[0],2.2f),std::pow(light.fogColor[1],2.2f),std::pow(light.fogColor[2],2.2f),0};
}
void DiligentWater::Draw(const EffectVertex* vertices,unsigned count,const EffectDraw& draw) {
    auto& s=*impl_;if(!s.open||!vertices||count<3)return;
    const auto start=std::chrono::steady_clock::now();
    std::memcpy(&s.data.world,draw.matrices.world.data(),64);s.Upload();
    Impl::Set(s.prepass.srb,SHADER_TYPE_VERTEX,"Water",s.constants);Impl::Set(s.prepass.srb,SHADER_TYPE_PIXEL,"Water",s.constants);
    Impl::Set(s.prepass.srb,SHADER_TYPE_PIXEL,"NormalMap",s.normalMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Impl::Set(s.prepass.srb,SHADER_TYPE_PIXEL,"NormalSampler",s.normalSampler);
    ITextureView* targets[]{s.normals->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),s.sceneNormals->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)};s.context->SetRenderTargets(2,targets,s.depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Viewport viewport{0,0,float(s.width),float(s.height),0,1};s.context->SetViewports(1,&viewport,s.width,s.height);
    s.context->SetPipelineState(s.prepass.pso);s.context->CommitShaderResources(s.prepass.srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    // Native water is a triangle list. Test strips are expanded in bounded chunks.
    const unsigned triangles=draw.strip?count-2:count/3;
    for(unsigned first=0;first<triangles;) {
        const unsigned batch=std::min(triangles-first,StreamVertices/3);
        {MapHelper<EffectVertex> mapped(s.context,s.vertices,MAP_WRITE,MAP_FLAG_DISCARD);RequireWater(bool(mapped),"water vertex map");
            if(!draw.strip)std::memcpy(mapped,vertices+first*3,batch*3*sizeof(EffectVertex));
            else for(unsigned i=0;i<batch;++i){const unsigned t=first+i;mapped[3*i]=vertices[t];mapped[3*i+1]=vertices[t+1];mapped[3*i+2]=vertices[t+2];}}
        IBuffer* buffer=s.vertices;Uint64 offset=0;s.context->SetVertexBuffers(0,1,&buffer,&offset,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        s.context->Draw(DrawAttribs{batch*3,DRAW_FLAG_VERIFY_ALL});first+=batch;++s.stats.draws;
    }
    s.drawn=true;s.stats.waterCpuMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
void DiligentWater::Finish(PostFXContext* post,IBuffer* camera,ITextureView* motion) {
    auto& s=*impl_;if(!s.open)return;
    s.context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ITextureView* reflected=s.black->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    if(s.config.ssr&&post&&camera&&motion) {
        const auto start=std::chrono::steady_clock::now();FirstUseAudit timing("fx-total","G7-SSR-first",!s.ssr);
        if(!s.ssr)s.ssr=std::make_unique<ScreenSpaceReflection>(s.device,ScreenSpaceReflection::CreateInfo{});
        // FX's D3D11 CopyDepthToColor fast path requires float depth. D24 is
        // normalized integer data: convert numerically with the shared FX
        // depth-copy shader before it builds the R32_FLOAT hierarchy.
        PostFXContext::TextureOperationAttribs copy;copy.pDevice=s.device;copy.pDeviceContext=s.context;
        post->CopyTextureDepth(copy,s.depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE),s.ssrDepth->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET));
        PostFXContext::RenderAttributes common;common.pDevice=s.device;common.pDeviceContext=s.context;
        common.pCurrDepthBufferSRV=common.pPrevDepthBufferSRV=s.ssrDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        common.pMotionVectorsSRV=motion;common.pCameraAttribsCB=camera;post->Execute(common);
        Impl::Set(s.materialPass.srb,SHADER_TYPE_PIXEL,"WaterNormal",s.normals->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));s.Upload();
        s.Screen(s.materialPass,s.roughness->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET));
        s.context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        s.ssr->PrepareResources(s.device,s.context,post,s.config.halfResolution?ScreenSpaceReflection::FEATURE_FLAG_HALF_RESOLUTION:ScreenSpaceReflection::FEATURE_FLAG_NONE);
        HLSL::ScreenSpaceReflectionAttribs settings;settings.RoughnessThreshold=.55f;settings.MaxTraversalIntersections=s.config.maxTraversal;
        settings.TemporalRadianceStabilityFactor=settings.TemporalVarianceStabilityFactor=0;
        ScreenSpaceReflection::RenderAttributes attributes;attributes.pDevice=s.device;attributes.pDeviceContext=s.context;attributes.pPostFXContext=post;
        attributes.pColorBufferSRV=s.scene->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);attributes.pDepthBufferSRV=s.ssrDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        attributes.pNormalBufferSRV=s.sceneNormals->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);attributes.pMaterialBufferSRV=s.roughness->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        attributes.pMotionVectorsSRV=motion;attributes.pSSRAttribs=&settings;s.ssr->Execute(attributes);
        if(s.ssr->IsPSOsReady()&&s.ssr->GetSSRRadianceSRV()){reflected=s.ssr->GetSSRRadianceSRV();s.data.options.z=1;++s.stats.ssrFrames;}
        else ++s.stats.ssrFallbacks;
        if(waterTestView==3)reflected=s.ssr->GetIntersectionRadianceSRV();
        if(waterTestView==4)reflected=s.ssr->GetRoughnessSRV();
        s.stats.ssrCpuMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    } else if(s.config.ssr)++s.stats.ssrFallbacks;
    const auto start=std::chrono::steady_clock::now();
    if(s.drawn) {
        s.Upload();auto* binding=s.composite.srb.RawPtr();
        Impl::Set(binding,SHADER_TYPE_PIXEL,"Scene",s.scene->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        Impl::Set(binding,SHADER_TYPE_PIXEL,"OpaqueDepth",s.input.depth);
        Impl::Set(binding,SHADER_TYPE_PIXEL,"WaterDepth",s.depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        Impl::Set(binding,SHADER_TYPE_PIXEL,"WaterNormal",s.normals->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        Impl::Set(binding,SHADER_TYPE_PIXEL,"Sky",s.input.sky);Impl::Set(binding,SHADER_TYPE_PIXEL,"SkySampler",s.input.skySampler);
        Impl::Set(binding,SHADER_TYPE_PIXEL,"Reflection",reflected);Impl::Set(binding,SHADER_TYPE_PIXEL,"SceneSampler",s.sceneSampler);
        Impl::Set(binding,SHADER_TYPE_PIXEL,"ShadowMap",s.input.shadow);Impl::Set(binding,SHADER_TYPE_PIXEL,"ShadowSampler",s.input.shadowSampler);
        s.Screen(s.composite,s.input.scene->GetTexture()->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET));++s.stats.frames;
        if(waterTestReadback)waterTestOutputPeak=s.ReadPeak(s.input.scene->GetTexture());
    }
    s.stats.waterCpuMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    s.open=false;s.ClearBindings();
}
}
