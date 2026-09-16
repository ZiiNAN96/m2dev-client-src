#include "DiligentAtmosphere.h"
#include "FirstUseAudit.h"
#include "Graphics/AtmosphereConfig.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsTools/interface/ShaderMacroHelper.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "Common/interface/BasicMath.hpp"
#include <stdexcept>
#include <vector>
#include <string>
#include <chrono>

namespace Diligent::G56Atmosphere
{
using uint=Uint32;
#include "Shaders/Common/public/ShaderDefinitions.fxh"
#include "Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh"
#include "Shaders/PostProcess/EpipolarLightScattering/public/EpipolarLightScatteringStructures.fxh"
#include "AtmosphereCoefficients.hpp"
}
namespace Renderer
{
using namespace Diligent;
namespace
{
void Require(bool result,const char* text){if(!result)throw std::runtime_error(text);}
constexpr char skyShader[]=R"(
#include "AtmosphereShadersCommon.fxh"
cbuffer cbParticipatingMediaScatteringParams {AirScatteringAttribs g_MediaParams;}
cbuffer SkyInputs {float4 Direction;float4 Radiance;float4 Horizon;float4 Clouds;}
Texture3D<float3> Scattering;SamplerState ScatteringSampler;
#include "LookUpTables.fxh"
// Self-authored periodic value noise. Two-dimensional soft clouds only: no
// volume, raymarching, downloaded texture or additional runtime dependency.
float CloudHash(float2 p) {
 p-=floor(p/64)*64; // Euclidean wrap also for negative sky-plane coordinates.
 return frac(sin(dot(p,float2(127.1,311.7)))*43758.5453);
}
float CloudNoise(float2 p) {
 float2 cell=floor(p),f=frac(p);f=f*f*(3-2*f);
 return lerp(lerp(CloudHash(cell),CloudHash(cell+float2(1,0)),f.x),
             lerp(CloudHash(cell+float2(0,1)),CloudHash(cell+1),f.x),f.y);
}
float CloudShape(float2 p) {
 return .5*CloudNoise(p)+.25*CloudNoise(p*2)+.13*CloudNoise(p*4)+
        .07*CloudNoise(p*8)+.035*CloudNoise(p*16)+.015*CloudNoise(p*32);
}
float4 SkyPS(FullScreenTriangleVSOutput input):SV_TARGET {
 float2 uv=NormalizedDeviceXYToTexUV(input.f2NormalizedXY);
 float elevation=saturate(1-uv.y);
 float azimuth=(uv.x-.5)*2*PI;
 float horizontal=sqrt(saturate(1-elevation*elevation));
 // The FX table is Y-up/metres; the game's sun is Z-up, along the rays.
 float3 view=float3(cos(azimuth)*horizontal,max(.001,elevation),sin(azimuth)*horizontal);
 float3 sun=normalize(-Direction.xzy);
 float4 coords=-1;
 float3 scattering=LookUpPrecomputedScattering(float3(0,100,0),normalize(view),
  float3(0,-g_MediaParams.fEarthRadius,0),g_MediaParams.fEarthRadius,sun,
  g_MediaParams.fAtmBottomAltitude,g_MediaParams.fAtmTopAltitude,Scattering,ScatteringSampler,coords);
 // A sky-only radiometric/chromatic calibration; world exposure and the
 // authored diffuse colours are unchanged. Keep blue above green (no cyan).
 float3 sky=max(scattering*Radiance.rgb*float3(.82,1.08,1.28)*2.8,0);
 // The authored tint belongs to the horizon, never to terrain/objects. Bake
 // it here so the visible sky, water and filtered IBL consume the same colour.
 sky=lerp(sky,Horizon.rgb,.06*pow(1-elevation,4));
 if(Clouds.z>0) {
  float2 plane=view.xz/(.18+view.y)*1.6+Clouds.xy*64;
  float shape=CloudShape(plane);
  float alpha=smoothstep(.49,.64,shape)*Clouds.z*smoothstep(.025,.22,elevation);
  float daylight=smoothstep(-.08,.25,sun.y);
  float3 colour=min(Radiance.rgb*.24+sky*.12,1.35);
  colour*=lerp(.35,1,daylight)*lerp(.8,1,smoothstep(.5,.8,shape));
  sky=lerp(sky,colour,alpha);
 }
 return float4(sky,1);
}
)";
}
struct DiligentAtmosphere::Impl
{
    RefCntAutoPtr<IRenderDevice> device;
    RefCntAutoPtr<IDeviceContext> context;
    RefCntAutoPtr<ITexture> density,scattering;
    std::array<RefCntAutoPtr<ITexture>,2> sky;
    RefCntAutoPtr<IBuffer> media,inputs;
    RefCntAutoPtr<ISampler> sampler;
    RefCntAutoPtr<IPipelineState> skyPSO;
    RefCntAutoPtr<IShaderResourceBinding> skySRB;
    struct FilterConstants {float4x4 rotation;float roughness,width,height,mips;Uint32 samples,padding[3];};
    RefCntAutoPtr<IBuffer> filterCB;
    std::array<RefCntAutoPtr<ITexture>,2> cubes;
    std::array<std::vector<RefCntAutoPtr<ITextureView>>,2> faces;
    std::array<RefCntAutoPtr<IPipelineState>,2> filterPSOs;
    std::array<RefCntAutoPtr<IShaderResourceBinding>,2> filterSRBs;
    Graphics::SceneLighting previous;
    bool valid{},quality{};
    std::array<float,2> previousOffset{};
    RefCntAutoPtr<IShader> Shader(const char* source,const char* entry,SHADER_TYPE stage) {
        FirstUseAudit timing("shader",entry);
        ShaderMacroHelper macros;
        macros.Add("PRECOMPUTED_SCTR_LUT_DIM","float4(16.0,128.0,32.0,16.0)");
        macros.Add("ENABLE_LIGHT_SHAFTS",0);macros.Add("SINGLE_SCATTERING_MODE",2);
        macros.Add("MULTIPLE_SCATTERING_MODE",0);macros.Add("AUTO_EXPOSURE",0);
        ShaderCreateInfo ci;ci.Desc={entry,stage};ci.EntryPoint=entry;ci.Source=source;
        ci.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL;ci.Macros=macros;
        ci.CompileFlags=SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
        ci.pShaderSourceStreamFactory=&DiligentFXShaderSourceStreamFactory::GetInstance();
        RefCntAutoPtr<IShader> result;device->CreateShader(ci,&result);Require(bool(result),entry);return result;
    }
    RefCntAutoPtr<ITexture> Texture(const char* name,TEXTURE_FORMAT format,unsigned width,unsigned height,unsigned depth=1) {
        TextureDesc desc;desc.Name=name;desc.Type=depth==1?RESOURCE_DIM_TEX_2D:RESOURCE_DIM_TEX_3D;
        desc.Width=width;desc.Height=height;desc.Depth=depth;desc.Format=format;
        desc.BindFlags=BIND_SHADER_RESOURCE|(depth==1?BIND_RENDER_TARGET:BIND_UNORDERED_ACCESS);
        RefCntAutoPtr<ITexture> result;device->CreateTexture(desc,nullptr,&result);Require(bool(result),name);return result;
    }
    static void Set(IShaderResourceBinding* srb,SHADER_TYPE stage,const char* name,IDeviceObject* resource) {
        auto* variable=srb->GetVariableByName(stage,name);Require(variable!=nullptr,name);variable->Set(resource);
    }
    RefCntAutoPtr<IPipelineState> Fullscreen(const char* name,const char* source,const char* entry,TEXTURE_FORMAT format) {
        auto vs=Shader("#include \"FullScreenTriangleVS.fx\"","FullScreenTriangleVS",SHADER_TYPE_VERTEX);
        auto ps=Shader(source,entry,SHADER_TYPE_PIXEL);
        GraphicsPipelineStateCreateInfo ci;ci.PSODesc.Name=name;ci.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        ci.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        ci.GraphicsPipeline.NumRenderTargets=1;ci.GraphicsPipeline.RTVFormats[0]=format;
        ci.GraphicsPipeline.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        ci.GraphicsPipeline.RasterizerDesc.CullMode=CULL_MODE_NONE;ci.GraphicsPipeline.DepthStencilDesc.DepthEnable=False;
        ci.pVS=vs;ci.pPS=ps;RefCntAutoPtr<IPipelineState> result;
        {FirstUseAudit timing("pso",name);device->CreateGraphicsPipelineState(ci,&result);}Require(bool(result),name);return result;
    }
    void Draw(IPipelineState* pso,IShaderResourceBinding* srb,ITexture* texture) {
        auto* target=texture->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        const auto& desc=texture->GetDesc();Viewport viewport{0,0,float(desc.Width),float(desc.Height),0,1};
        context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->SetViewports(1,&viewport,desc.Width,desc.Height);context->SetPipelineState(pso);
        context->CommitShaderResources(srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->Draw(DrawAttribs{3,DRAW_FLAG_VERIFY_ALL});
        context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    Impl(IRenderDevice* d,IDeviceContext* c):device(d),context(c) {
        FirstUseAudit timing("fx-total","atmosphere-tables");
        G56Atmosphere::AirScatteringAttribs coefficients{};
        G56Atmosphere::EpipolarLightScatteringAttribs settings;
        // Clear fantasy daylight: reduce the default dense aerosol veil, while
        // retaining FX Rayleigh/ozone scattering and its single shared sun.
        settings.fAerosolDensityScale=.12f;
        G56Atmosphere::InitializeAtmosphereCoefficients(coefficients,settings);
        BufferDesc desc;desc.Name="G56 FX physical media";desc.Size=sizeof(coefficients);
        desc.BindFlags=BIND_UNIFORM_BUFFER;desc.Usage=USAGE_IMMUTABLE;
        BufferData data{&coefficients,sizeof(coefficients)};device->CreateBuffer(desc,&data,&media);
        Require(bool(media),"G56 atmosphere media");
        desc.Name="G8 shared sun sky inputs";desc.Size=64;desc.Usage=USAGE_DEFAULT;
        device->CreateBuffer(desc,nullptr,&inputs);Require(bool(inputs),"G56 atmosphere inputs");
        SamplerDesc sampling;sampling.AddressU=sampling.AddressV=sampling.AddressW=TEXTURE_ADDRESS_CLAMP;
        device->CreateSampler(sampling,&sampler);Require(bool(sampler),"G56 atmosphere sampler");
        density=Texture("G56 FX optical depth",TEX_FORMAT_RG32_FLOAT,256,256);
        scattering=Texture("G56 FX single scattering",TEX_FORMAT_RGBA16_FLOAT,16,128,32*16);
        sky[0]=Texture("G56 sky low",TEX_FORMAT_RGBA16_FLOAT,128,64);
        sky[1]=Texture("G8 sky high",TEX_FORMAT_RGBA16_FLOAT,512,256);
        auto opticalPSO=Fullscreen("G56 FX optical depth precompute","#include \"PrecomputeNetDensityToAtmTop.fx\"","PrecomputeNetDensityToAtmTopPS",TEX_FORMAT_RG32_FLOAT);
        RefCntAutoPtr<IShaderResourceBinding> opticalSRB;opticalPSO->CreateShaderResourceBinding(&opticalSRB,true);
        Set(opticalSRB,SHADER_TYPE_PIXEL,"cbParticipatingMediaScatteringParams",media);
        Draw(opticalPSO,opticalSRB,density);
        ComputePipelineStateCreateInfo ci;ci.PSODesc.Name="G56 FX single scattering precompute";
        ci.PSODesc.PipelineType=PIPELINE_TYPE_COMPUTE;ci.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        auto compute=Shader("#include \"PrecomputeSingleScattering.fx\"","PrecomputeSingleScatteringCS",SHADER_TYPE_COMPUTE);ci.pCS=compute;
        RefCntAutoPtr<IPipelineState> pso;device->CreateComputePipelineState(ci,&pso);Require(bool(pso),ci.PSODesc.Name);
        RefCntAutoPtr<IShaderResourceBinding> srb;pso->CreateShaderResourceBinding(&srb,true);
        Set(srb,SHADER_TYPE_COMPUTE,"cbParticipatingMediaScatteringParams",media);
        Set(srb,SHADER_TYPE_COMPUTE,"g_tex2DOccludedNetDensityToAtmTop",density->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        Set(srb,SHADER_TYPE_COMPUTE,"g_tex2DOccludedNetDensityToAtmTop_sampler",sampler);
        Set(srb,SHADER_TYPE_COMPUTE,"g_rwtex3DSingleScattering",scattering->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
        context->SetPipelineState(pso);context->CommitShaderResources(srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->DispatchCompute(DispatchComputeAttribs{1,8,512});
        skyPSO=Fullscreen("G56 FX atmosphere sky lookup",skyShader,"SkyPS",TEX_FORMAT_RGBA16_FLOAT);
        skyPSO->CreateShaderResourceBinding(&skySRB,true);
        Set(skySRB,SHADER_TYPE_PIXEL,"cbParticipatingMediaScatteringParams",media);
        Set(skySRB,SHADER_TYPE_PIXEL,"SkyInputs",inputs);
        Set(skySRB,SHADER_TYPE_PIXEL,"Scattering",scattering->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        Set(skySRB,SHADER_TYPE_PIXEL,"ScatteringSampler",sampler);
        // Reuse the existing FX diffuse convolution and GGX prefilter. The only
        // shader adaptation is the game's Z-up sky-atlas coordinate mapping.
        desc.Name="G56 FX environment filter constants";desc.Size=sizeof(FilterConstants);
        device->CreateBuffer(desc,nullptr,&filterCB);Require(bool(filterCB),desc.Name);
        auto cubeVS=Shader("#include \"CubemapFace.vsh\"","main",SHADER_TYPE_VERTEX);
        for(unsigned kind=0;kind<2;++kind) {
            TextureDesc cube;cube.Name=kind?"G56 sky specular cube":"G56 sky irradiance cube";
            cube.Type=RESOURCE_DIM_TEX_CUBE;cube.ArraySize=6;cube.Width=cube.Height=kind?32:16;
            cube.MipLevels=kind?6:1;cube.Format=TEX_FORMAT_RGBA16_FLOAT;cube.BindFlags=BIND_RENDER_TARGET|BIND_SHADER_RESOURCE;
            device->CreateTexture(cube,nullptr,&cubes[kind]);Require(bool(cubes[kind]),cube.Name);
            for(unsigned mip=0;mip<cube.MipLevels;++mip)for(unsigned face=0;face<6;++face) {
                TextureViewDesc view;view.ViewType=TEXTURE_VIEW_RENDER_TARGET;view.TextureDim=RESOURCE_DIM_TEX_2D_ARRAY;
                view.MostDetailedMip=mip;view.NumMipLevels=1;view.FirstArraySlice=face;view.NumArraySlices=1;
                RefCntAutoPtr<ITextureView> target;cubes[kind]->CreateView(view,&target);Require(bool(target),cube.Name);
                faces[kind].push_back(std::move(target));
            }
            std::string source=R"(
#include "ShaderUtilities.fxh"
float2 G56SkyUV(float3 d){return float2(atan2(d.y,d.x)/(2*3.14159265359)+.5,1-saturate(d.z));}
#define TransformDirectionToSphereMapUV G56SkyUV
#define ENV_MAP_TYPE_CUBE 0
#define ENV_MAP_TYPE_SPHERE 1
#define ENV_MAP_TYPE 1
#define OPTIMIZE_SAMPLES 0
)";
            source+=kind?"#include \"PrefilterEnvMap.psh\"":"#include \"ComputeIrradianceMap.psh\"";
            auto ps=Shader(source.c_str(),"main",SHADER_TYPE_PIXEL);
            GraphicsPipelineStateCreateInfo pipeline;pipeline.PSODesc.Name=cube.Name;
            pipeline.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            pipeline.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
            pipeline.GraphicsPipeline.NumRenderTargets=1;pipeline.GraphicsPipeline.RTVFormats[0]=cube.Format;
            pipeline.GraphicsPipeline.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            pipeline.GraphicsPipeline.RasterizerDesc.CullMode=CULL_MODE_NONE;
            pipeline.GraphicsPipeline.DepthStencilDesc.DepthEnable=False;pipeline.pVS=cubeVS;pipeline.pPS=ps;
            device->CreateGraphicsPipelineState(pipeline,&filterPSOs[kind]);Require(bool(filterPSOs[kind]),cube.Name);
            filterPSOs[kind]->CreateShaderResourceBinding(&filterSRBs[kind],true);
            Set(filterSRBs[kind],SHADER_TYPE_VERTEX,"cbTransform",filterCB);
            Set(filterSRBs[kind],SHADER_TYPE_PIXEL,"FilterAttribs",filterCB);
            Set(filterSRBs[kind],SHADER_TYPE_PIXEL,"g_EnvironmentMap_sampler",sampler);
        }
    }
    void FilterSky(bool highQuality) {
        constexpr float pi=3.14159265358979323846f;
        const float4x4 rotations[]={float4x4::RotationY(-pi/2),float4x4::RotationY(pi/2),
            float4x4::RotationX(pi/2),float4x4::RotationX(-pi/2),float4x4::Identity(),float4x4::RotationY(-pi)};
        for(unsigned kind=0;kind<2;++kind) {
            Set(filterSRBs[kind],SHADER_TYPE_PIXEL,"g_EnvironmentMap",sky[highQuality]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            for(unsigned index=0;index<faces[kind].size();++index) {
                const unsigned mip=index/6,size=cubes[kind]->GetDesc().Width>>mip;
                FilterConstants data{rotations[index%6],float(mip)/5,float(sky[highQuality]->GetDesc().Width),float(sky[highQuality]->GetDesc().Height),1,128,{}};
                context->UpdateBuffer(filterCB,0,sizeof(data),&data,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                auto* target=faces[kind][index].RawPtr();Viewport viewport{0,0,float(size),float(size),0,1};
                context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                context->SetViewports(1,&viewport,size,size);context->SetPipelineState(filterPSOs[kind]);
                context->CommitShaderResources(filterSRBs[kind],RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                context->Draw(DrawAttribs{4,DRAW_FLAG_VERIFY_ALL});
            }
        }
        context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
};
DiligentAtmosphere::DiligentAtmosphere(IRenderDevice* d,IDeviceContext* c):impl_(std::make_unique<Impl>(d,c)){}
DiligentAtmosphere::~DiligentAtmosphere()=default;
void DiligentAtmosphere::Prepare(const Graphics::SceneLighting& lighting,bool highQuality) {
    auto& s=*impl_;const auto light=Graphics::ValidateSceneLighting(lighting);
    static const auto epoch=std::chrono::steady_clock::now();
    const double seconds=Graphics::developmentSkySeconds>=0?Graphics::developmentSkySeconds:
        std::chrono::duration<double>(std::chrono::steady_clock::now()-epoch).count();
    const auto offset=Graphics::CloudOffset(seconds);
    const bool relight=!s.valid||s.quality!=highQuality||s.previous.sunDirection!=light.sunDirection||
        s.previous.sunColor!=light.sunColor||s.previous.sunIntensity!=light.sunIntensity||s.previous.fogColor!=light.fogColor;
    if(!relight&&s.previous.cloudCoverage==light.cloudCoverage&&
       (light.cloudCoverage==0||s.previousOffset==offset))return;
    struct {float4 direction,radiance,horizon,clouds;} data{{light.sunDirection[0],light.sunDirection[1],light.sunDirection[2],0},
        {light.sunColor[0]*light.sunIntensity,light.sunColor[1]*light.sunIntensity,light.sunColor[2]*light.sunIntensity,0},
        {std::pow(light.fogColor[0],2.2f),std::pow(light.fogColor[1],2.2f),std::pow(light.fogColor[2],2.2f),0},
        {offset[0],offset[1],0,0}};
    if(relight) {
        // Slowly moving clouds do not rebuild the 42 IBL convolution faces.
        // Low-frequency ambient fill uses the same calibrated clear atmosphere.
        s.context->UpdateBuffer(s.inputs,0,sizeof(data),&data,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        s.Draw(s.skyPSO,s.skySRB,s.sky[highQuality]);s.FilterSky(highQuality);
    }
    data.clouds.z=light.cloudCoverage;
    s.context->UpdateBuffer(s.inputs,0,sizeof(data),&data,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    s.Draw(s.skyPSO,s.skySRB,s.sky[highQuality]);
    s.previous=light;s.previousOffset=offset;s.quality=highQuality;s.valid=true;
}
ITextureView* DiligentAtmosphere::Sky() const {return impl_->sky[impl_->quality]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);}
ISampler* DiligentAtmosphere::Sampler() const {return impl_->sampler;}
ITextureView* DiligentAtmosphere::Irradiance() const {return impl_->cubes[0]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);}
ITextureView* DiligentAtmosphere::PrefilteredEnvironment() const {return impl_->cubes[1]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);}
std::uint64_t DiligentAtmosphere::TargetBytes() const {return 256*256*8+16*128*32*16*8+(128*64+512*256)*8+(16*16+1365)*6*8;}
}
