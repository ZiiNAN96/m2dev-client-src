#include "DiligentModernRenderer.h"
#include "DiligentD3D11BackendInternal.h"
#include "DiligentAtmosphere.h"
#include "DiligentWater.h"
#include "Graphics/AtmosphereConfig.h"
#include "ModernMeshShader.h"
#include "ModernTerrainShader.h"
#include "GraphicsConfig.h"
#include "Diagnostics.h"
#include "FirstUseAudit.h"
#include "Components/interface/ShadowMapManager.hpp"
#include "PostProcess/Common/interface/PostFXContext.hpp"
#include "PostProcess/ScreenSpaceAmbientOcclusion/interface/ScreenSpaceAmbientOcclusion.hpp"
#include "PostProcess/Bloom/interface/Bloom.hpp"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include "Graphics/GraphicsTools/interface/ShaderMacroHelper.hpp"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Common/interface/AdvancedMath.hpp"
#include <cstring>
#include <map>
#include <stdexcept>
#include <chrono>

namespace Diligent::HLSL
{
#include "Shaders/PostProcess/ScreenSpaceAmbientOcclusion/public/ScreenSpaceAmbientOcclusionStructures.fxh"
#include "Shaders/PostProcess/Bloom/public/BloomStructures.fxh"
}

namespace Renderer
{
using namespace Diligent;
namespace
{
void Require(bool condition,const char* message) {if(!condition)throw std::runtime_error(message);}
float4x4 Matrix(const std::array<float,16>& data) {float4x4 result;std::memcpy(&result,data.data(),64);return result;}
struct ObjectConstants
{
    float4x4 world,viewProjection;
    float4 baseColor,surface,emissive;
    uint4 maps;
    float4 alpha,cardRight,cardForward,cardUp,wind,cardPitch;
    uint4 card;
    float4x4 cameraMaskMatrix;
    float4 legacyAlpha;
    float4 legacyTint;
    float4x4 legacyView;
    float4 worldWind,branchWind,foliage;
};
struct LightConstants {float4 direction,color,ambient,camera,environment;};
struct TerrainConstants {
    TerrainMatrices matrices;
    std::array<float,16> colorTransform,alphaTransform;
    std::array<float,4> factor,fogColor,fogParameters;
    uint4 modes;
};
struct CompositeConstants {CameraAttribs camera;ShadowMapAttribs shadows;float4 options,horizonColor,sunDirection,sunRadiance;};
struct ToneConstants {float4 exposure;};
float4 Vector(const std::array<float,4>& a) {return {a[0],a[1],a[2],a[3]};}
constexpr char compositeShader[]=R"(
#include "BasicStructures.fxh"
#include "ShaderUtilities.fxh"
#include "SRGBUtilities.fxh"
#define PCF_FILTER_SIZE 3
#define FILTER_ACROSS_CASCADES 1
#include "Shadows.fxh"
cbuffer Composite {CameraAttribs Camera;ShadowMapAttribs Shadows;float4 Options;float4 HorizonColor;float4 SunDirection;float4 SunRadiance;};
Texture2D Direct;Texture2D Indirect;Texture2D Emission;Texture2D Depth;Texture2D AO;Texture2D Background;
Texture2D Sky;SamplerState SkySampler;
Texture2DArray<float> ShadowMap;SamplerComparisonState ShadowSampler;
float4 CompositeVS(uint id:SV_VertexID):SV_POSITION {
 return float4(id==2?3:-1,id==1?3:-1,0,1);
}
float4 CompositePS(float4 pixel:SV_POSITION):SV_TARGET {
 int2 coord=int2(pixel.xy);float depth=Depth.Load(int3(coord,0)).r;
 float2 uv=pixel.xy*Camera.f4ViewportSize.zw;
 float4 world=mul(float4(TexUVToNormalizedDeviceXY(uv),DepthToNormalizedDeviceZ(depth),1),Camera.mViewProjInv);
 world/=world.w;
 float3 ray=normalize(world.xyz-Camera.f4Position.xyz);
 float2 skyUV=float2(atan2(ray.y,ray.x)/(2*3.14159265359)+.5,1-saturate(ray.z));
 float3 sky=Sky.SampleLevel(SkySampler,skyUV,0).rgb;
 // Horizon tint and soft clouds are already in the shared atmosphere atlas.
 if(depth>=1) {
  if(Options.z==0)return float4(FastSRGBToLinear(Background.Load(int3(coord,0)).rgb),1);
  // A 32 arc-minute disk in sky space; the direction is opposite the rays.
  float angularRadius=.00465421134;
  float cosine=dot(ray,-SunDirection.xyz);
  float edge=max(fwidth(cosine),1e-7);
  float disk=smoothstep(cos(angularRadius)-edge,cos(angularRadius)+edge,cosine);
  return float4(sky+SunRadiance.rgb*disk*8,1);
 }
 float shadow=1;
 if(Options.x!=0) {
  float3 light=mul(world,Shadows.mWorldToLightView).xyz;
  float cameraZ=abs(mul(world,Camera.mView).z);
  shadow=FilterShadowMap(Shadows,ShadowMap,ShadowSampler,light,ddx(light),ddy(light),cameraZ).fLightAmount;
 }
 float ao=Options.y!=0?AO.Load(int3(coord,0)).r:1;
 float3 linearColor=Direct.Load(int3(coord,0)).rgb*shadow+Indirect.Load(int3(coord,0)).rgb*ao+Emission.Load(int3(coord,0)).rgb;
 float3 color=max(linearColor,0);
 return float4(color,1);
}
)";
constexpr char toneShader[]=R"(
#include "ShaderUtilities.fxh"
#define TONE_MAPPING_MODE TONE_MAPPING_MODE_REINHARD
#include "ToneMapping.fxh"
cbuffer Tone {float4 Exposure;}
Texture2D Scene;
float4 ToneVS(uint id:SV_VertexID):SV_POSITION {return float4(id==2?3:-1,id==1?3:-1,0,1);}
float4 TonePS(float4 pixel:SV_POSITION):SV_TARGET {
 ToneMappingAttribs settings=(ToneMappingAttribs)0;
 settings.fMiddleGray=1;settings.fLuminanceSaturation=1;
 float3 mapped=ToneMap(Scene.Load(int3(int2(pixel.xy),0)).rgb,settings,1/Exposure.x);
 return float4(FastLinearToSRGB(mapped),1);
}
)";
}
struct DiligentModernRenderer::Impl
{
    DiligentD3D11Backend& backend;
    bool active{},forward{},hasCamera{},hasDepth{},shadowsPrepared{},worldOpen{},deferToneMapping{},sceneReady{};
    std::array<ViewFrustum,4> shadowFrusta;
    bool PrepareShadows();
    ModernFrameStats stats;
    Uint32 width{},height{},frame{};
    Graphics::SceneLighting lighting;
    Graphics::GraphicsRuntimeConfig config;
    CameraAttribs camera{};
    RefCntAutoPtr<ITexture> depth,background,white,motion;
    RefCntAutoPtr<ITexture> hdrScene;
    RefCntAutoPtr<ITexture> brdf,unitEnvironment;
    RefCntAutoPtr<ISampler> iblSampler;
    std::array<RefCntAutoPtr<ITexture>,4> surfaces;
    RefCntAutoPtr<IBuffer> objectCB,lightCB,compositeCB,cameraCB;
    RefCntAutoPtr<ISampler> shadowSampler;
    std::map<unsigned,RefCntAutoPtr<ISampler>> materialSamplers;
    struct Pipeline {RefCntAutoPtr<IPipelineState> pso;RefCntAutoPtr<IShaderResourceBinding> srb;};
    std::map<unsigned,Pipeline> pipelines;
    std::array<RefCntAutoPtr<IShader>,36> vertexShaders,pixelShaders;
    std::array<RefCntAutoPtr<IShader>,4> terrainVS,terrainPS;
    Pipeline composite;
    Pipeline tone;
    RefCntAutoPtr<IBuffer> toneCB;
    std::unique_ptr<DiligentAtmosphere> atmosphere;
    std::unique_ptr<ShadowMapManager> shadow;
    unsigned shadowSize{},cascades{};
    ShadowMapAttribs shadowAttribs{};
    std::unique_ptr<PostFXContext> post;
    std::unique_ptr<ScreenSpaceAmbientOcclusion> ao;
    std::unique_ptr<Bloom> bloom;
    std::unique_ptr<DiligentWater> water;
    bool waterStarted{},waterFinished{},waterWarmed{};
    std::vector<ModernMeshSubmission> casters;
    std::vector<ModernMeshSubmission> transparent;
    std::vector<ModernTerrainSubmission> terrainCasters;
    RefCntAutoPtr<IBuffer> terrainCB;
    std::map<unsigned,Pipeline> terrainPipelines;
    explicit Impl(DiligentD3D11Backend& b):backend(b) {}
    auto& State() {return *backend.m_impl;}
    void Buffer(RefCntAutoPtr<IBuffer>& buffer,Uint32 size,const char* name) {
        if(buffer)return;
        BufferDesc desc;desc.Name=name;desc.Size=size;desc.Usage=USAGE_DYNAMIC;
        desc.BindFlags=BIND_UNIFORM_BUFFER;desc.CPUAccessFlags=CPU_ACCESS_WRITE;
        State().device->CreateBuffer(desc,nullptr,&buffer);Require(bool(buffer),name);
        if(std::addressof(buffer)==std::addressof(lightCB))++stats.lightBufferCreations;
    }
    RefCntAutoPtr<ITexture> Texture(TEXTURE_FORMAT format,const char* name,BIND_FLAGS binds) {
        TextureDesc desc;desc.Name=name;desc.Type=RESOURCE_DIM_TEX_2D;desc.Width=width;desc.Height=height;
        desc.Format=format;desc.BindFlags=binds;
        RefCntAutoPtr<ITexture> result;State().device->CreateTexture(desc,nullptr,&result);Require(bool(result),name);return result;
    }
    void Resources() {
        const auto& swap=State().swapChain->GetDesc();
        if(depth&&width==swap.Width&&height==swap.Height)return;
        width=swap.Width;height=swap.Height;
        depth=Texture(TEX_FORMAT_R24G8_TYPELESS,"G-DX shared scene depth",BIND_DEPTH_STENCIL|BIND_SHADER_RESOURCE);
        background=Texture(TEX_FORMAT_RGBA8_UNORM,"G-DX background",BIND_RENDER_TARGET|BIND_SHADER_RESOURCE);
        hdrScene=Texture(TEX_FORMAT_RGBA16_FLOAT,"G56 HDR world scene",BIND_RENDER_TARGET|BIND_SHADER_RESOURCE);
        for(unsigned i=0;i<4;++i)surfaces[i]=Texture(TEX_FORMAT_RGBA16_FLOAT,
            "G-DX direct/indirect/emission/normal",BIND_RENDER_TARGET|BIND_SHADER_RESOURCE);
        motion=Texture(TEX_FORMAT_RG16_FLOAT,"G-DX spatial AO scratch (history disabled)",BIND_RENDER_TARGET|BIND_SHADER_RESOURCE);
        white=Texture(TEX_FORMAT_R8_UNORM,"G-DX neutral AO",BIND_RENDER_TARGET|BIND_SHADER_RESOURCE);
        float one[]{1,1,1,1},zero[]{0,0,0,0};
        State().context->ClearRenderTarget(white->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),one,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        State().context->ClearRenderTarget(motion->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),zero,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        composite.srb.Release();
        tone.srb.Release();
    }
    RefCntAutoPtr<IShader> Shader(const char* source,const char* entry,SHADER_TYPE stage,const ShaderMacroArray& macros={}) {
        FirstUseAudit timing("shader",entry);
        ShaderCreateInfo ci;ci.Desc.Name=entry;ci.Desc.ShaderType=stage;ci.EntryPoint=entry;
        ci.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL;ci.Source=source;ci.Macros=macros;
        ci.CompileFlags=SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;
        ci.pShaderSourceStreamFactory=&DiligentFXShaderSourceStreamFactory::GetInstance();
        RefCntAutoPtr<IShader> shader;State().device->CreateShader(ci,&shader);Require(bool(shader),entry);return shader;
    }
    void IBL() {
        if(brdf)return;
        FirstUseAudit timing("fx-total","BRDF-IBL");
        auto& state=State();TextureDesc desc;desc.Name="G-DX FX BRDF LUT";desc.Type=RESOURCE_DIM_TEX_2D;
        desc.Width=desc.Height=256;desc.Format=TEX_FORMAT_RG16_FLOAT;desc.BindFlags=BIND_RENDER_TARGET|BIND_SHADER_RESOURCE;
        state.device->CreateTexture(desc,nullptr,&brdf);Require(bool(brdf),"G-DX BRDF LUT");
        ShaderMacroHelper macros;macros.Add("NUM_SAMPLES",512u);
        auto vs=Shader("#include \"FullScreenTriangleVS.fx\"","FullScreenTriangleVS",SHADER_TYPE_VERTEX);
        auto ps=Shader("#include \"PrecomputeBRDF.psh\"","PrecomputeBRDF_PS",SHADER_TYPE_PIXEL,macros);
        GraphicsPipelineStateCreateInfo ci;ci.PSODesc.Name="G-DX FX BRDF preintegration";ci.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        auto& g=ci.GraphicsPipeline;g.NumRenderTargets=1;g.RTVFormats[0]=desc.Format;g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        g.RasterizerDesc.CullMode=CULL_MODE_NONE;g.DepthStencilDesc.DepthEnable=False;ci.pVS=vs;ci.pPS=ps;
        RefCntAutoPtr<IPipelineState> pso;{FirstUseAudit timing("pso",ci.PSODesc.Name);state.device->CreateGraphicsPipelineState(ci,&pso);}Require(bool(pso),"G-DX BRDF PSO");
        auto* target=brdf->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);state.context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        Viewport viewport{0,0,256,256,0,1};state.context->SetViewports(1,&viewport,256,256);
        state.context->SetPipelineState(pso);state.context->Draw(DrawAttribs{3,DRAW_FLAG_VERIFY_ALL});
        // A unit cube is the exact irradiance/prefiltered result for a constant
        // environment. Directional sky sources can later supply filtered cubes.
        desc.Name="G-DX constant environment";desc.Type=RESOURCE_DIM_TEX_CUBE;desc.Width=desc.Height=1;desc.ArraySize=6;
        desc.Format=TEX_FORMAT_RGBA32_FLOAT;desc.BindFlags=BIND_SHADER_RESOURCE;desc.Usage=USAGE_IMMUTABLE;
        const float pixels[]{1,1,1,1};TextureSubResData faces[6];for(auto& face:faces){face.pData=pixels;face.Stride=16;}
        TextureData data{faces,6};state.device->CreateTexture(desc,&data,&unitEnvironment);Require(bool(unitEnvironment),"G-DX environment cube");
        SamplerDesc sampling;sampling.AddressU=sampling.AddressV=sampling.AddressW=TEXTURE_ADDRESS_CLAMP;
        state.device->CreateSampler(sampling,&iblSampler);Require(bool(iblSampler),"G-DX IBL sampler");
    }
    void MeshShaders(unsigned shaderIndex) {
        if(vertexShaders[shaderIndex])return;
        const auto geometry=shaderIndex%3,pass=(shaderIndex%9)/3;
        ShaderMacroHelper macros;macros.Add("GDX_SKIN",geometry==1);macros.Add("GDX_AUX",geometry==2);
        macros.Add("GDX_SHADOW",pass==1);macros.Add("GDX_FORWARD",pass==2);macros.Add("GDX_TANGENT",shaderIndex%18>=9);
        macros.Add("H2_INSTANCED",shaderIndex>=18);
        vertexShaders[shaderIndex]=Shader(modernMeshShader,"ModernVS",SHADER_TYPE_VERTEX,macros);
        pixelShaders[shaderIndex]=Shader(modernMeshShader,"ModernPS",SHADER_TYPE_PIXEL,macros);
        ++stats.meshShaderVariants;
    }
    Pipeline& GetPipeline(const ModernMeshSubmission& draw,bool shadowPass,bool forwardPass) {
        const unsigned geometry=draw.skinned?1:draw.auxiliary?2:0;
        const unsigned shaderIndex=geometry+(shadowPass?3:forwardPass?6:0)+(draw.tangents?9:0)+(draw.instances?18:0);
        unsigned cull=static_cast<unsigned>(draw.draw.cull);
        const auto world=Matrix(draw.draw.matrices.world);
        const bool mirrored=dot(cross(float3{world._11,world._12,world._13},float3{world._21,world._22,world._23}),float3{world._31,world._32,world._33})<0;
        const unsigned key=cull|(unsigned(draw.draw.blend)<<2)|(unsigned(draw.draw.depthWrite)<<3)|(shaderIndex<<4)|(unsigned(mirrored)<<10);
        auto& result=pipelines[key];if(result.pso)return result;
        Buffer(objectCB,sizeof(ObjectConstants),"G-DX material/object constants");
        Buffer(lightCB,sizeof(LightConstants),"G-DX shared sun constants");
        MeshShaders(shaderIndex);
        LayoutElement rigid[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32}};
        LayoutElement skin[]={{0,0,3,VT_FLOAT32,False,0,40},{1,0,3,VT_FLOAT32,False,20,40},{2,0,2,VT_FLOAT32,False,32,40},{3,0,4,VT_UINT8,False,12,40},{4,0,4,VT_UINT8,False,16,40}};
        LayoutElement aux[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32},
            {3,1,4,VT_FLOAT32,False,0,64},{4,1,2,VT_FLOAT32,False,16,64},{5,1,3,VT_FLOAT32,False,24,64},{6,1,1,VT_FLOAT32,False,36,64},{7,1,3,VT_FLOAT32,False,40,64},{8,1,3,VT_FLOAT32,False,52,64}};
        GraphicsPipelineStateCreateInfo ci;ci.PSODesc.Name="G-DX FX mesh pipeline";ci.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        ci.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        auto& g=ci.GraphicsPipeline;g.NumRenderTargets=shadowPass?0:forwardPass?1:4;
        for(unsigned i=0;i<g.NumRenderTargets;++i)g.RTVFormats[i]=surfaces[i]->GetDesc().Format;
        g.DSVFormat=shadowPass?TEX_FORMAT_D32_FLOAT:TEX_FORMAT_D24_UNORM_S8_UINT;
        g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        std::vector<LayoutElement> layout=draw.skinned?std::vector<LayoutElement>{skin,skin+5}:draw.auxiliary?std::vector<LayoutElement>{aux,aux+9}:std::vector<LayoutElement>{rigid,rigid+3};
        if(draw.tangents)layout.emplace_back(9,2,4,VT_FLOAT32,False,0,16);
        if(draw.instances)for(unsigned i=0;i<5;++i)layout.emplace_back(10+i,3,4,VT_FLOAT32,False,i*16,80,INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,1);
        g.InputLayout={layout.data(),static_cast<Uint32>(layout.size())};
        g.RasterizerDesc.CullMode=cull==0?CULL_MODE_NONE:CULL_MODE_BACK;
        // Legacy CW-cull convention also defines front faces when culling is off.
        g.RasterizerDesc.FrontCounterClockwise=(cull!=2)!=mirrored;
        g.RasterizerDesc.DepthClipEnable=True;
        if(shadowPass){g.RasterizerDesc.DepthBias=100;g.RasterizerDesc.SlopeScaledDepthBias=1.f;g.RasterizerDesc.DepthBiasClamp=.002f;}
        g.DepthStencilDesc.DepthEnable=True;g.DepthStencilDesc.DepthWriteEnable=shadowPass||draw.draw.depthWrite;
        g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;
        for(auto& blend:g.BlendDesc.RenderTargets) {
            blend.BlendEnable=!shadowPass&&draw.draw.blend;blend.SrcBlend=blend.SrcBlendAlpha=BLEND_FACTOR_SRC_ALPHA;
            blend.DestBlend=blend.DestBlendAlpha=BLEND_FACTOR_INV_SRC_ALPHA;
        }
        ci.pVS=vertexShaders[shaderIndex];ci.pPS=pixelShaders[shaderIndex];
        {FirstUseAudit timing("pso",ci.PSODesc.Name);State().device->CreateGraphicsPipelineState(ci,&result.pso);}Require(bool(result.pso),"G-DX mesh PSO");
        ++stats.psoCount;
        result.pso->CreateShaderResourceBinding(&result.srb,true);Require(bool(result.srb),"G-DX mesh SRB");return result;
    }
    static void Set(IShaderResourceBinding* srb,SHADER_TYPE stage,const char* name,IDeviceObject* resource) {
        if(auto* variable=srb->GetVariableByName(stage,name))variable->Set(resource);
    }
    ISampler* MaterialSampler(const TerrainSampling& input,bool anisotropic,unsigned anisotropy) {
        anisotropy=std::max(1u,std::min(16u,anisotropy));
        const unsigned key=unsigned(input.linearMin)|(unsigned(input.linearMag)<<1)|(unsigned(input.linearMip)<<2)|
            (unsigned(input.wrapU)<<3)|(unsigned(input.wrapV)<<4)|(unsigned(input.useMips)<<5)|(unsigned(anisotropic)<<6)|(anisotropy<<7);
        auto& result=materialSamplers[key];if(result)return result;
        SamplerDesc desc;desc.MinFilter=input.linearMin?FILTER_TYPE_LINEAR:FILTER_TYPE_POINT;
        desc.MagFilter=input.linearMag?FILTER_TYPE_LINEAR:FILTER_TYPE_POINT;desc.MipFilter=input.linearMip?FILTER_TYPE_LINEAR:FILTER_TYPE_POINT;
        if(anisotropic){desc.MinFilter=desc.MagFilter=desc.MipFilter=FILTER_TYPE_ANISOTROPIC;desc.MaxAnisotropy=anisotropy;}
        desc.AddressU=input.wrapU?TEXTURE_ADDRESS_WRAP:TEXTURE_ADDRESS_CLAMP;desc.AddressV=input.wrapV?TEXTURE_ADDRESS_WRAP:TEXTURE_ADDRESS_CLAMP;
        if(!input.useMips)desc.MaxLOD=0;
        State().device->CreateSampler(desc,&result);Require(bool(result),"G-DX material sampler");return result;
    }
    void DrawMesh(const ModernMeshSubmission& item,bool shadowPass,const float4x4& vp,bool forwardPass=false) {
        auto& state=State();auto& pipeline=GetPipeline(item,shadowPass,forwardPass);
        const MaterialRuntimeData defaults;const auto& material=item.draw.material?*item.draw.material:defaults;
        ObjectConstants constants{};constants.world=Matrix(item.draw.matrices.world);constants.viewProjection=vp;
        constants.baseColor=Vector(material.baseColor);constants.surface={material.roughness,material.metallic,material.normalScale,material.occlusionStrength};
        constants.emissive={material.emissive[0],material.emissive[1],material.emissive[2],
            material.model==AssetRuntime::MaterialModel::PBRMetallicRoughness?1.f:0.f};
        unsigned mask=0;for(unsigned i=1;i<item.textures.size();++i)if(item.textures[i])mask|=1u<<i;
        constants.maps={mask,material.roughnessChannel,material.metallicChannel,material.occlusionChannel};
        constants.alpha={float(item.draw.alphaReference),float(item.draw.alphaTest),item.draw.cull==StaticObjectCull::None?1.f:0.f,1};
        constants.cardRight=Vector(item.draw.cardRight);constants.cardForward=Vector(item.draw.cardForward);constants.cardUp=Vector(item.draw.cardUp);
        constants.wind=Vector(item.draw.wind);constants.cardPitch=Vector(item.draw.cardPitch);constants.card={item.draw.cardMode,item.draw.modernVegetation?1u:0u,0,0};
        constants.worldWind=Vector(item.draw.worldWind);constants.branchWind=Vector(item.draw.branchWind);constants.foliage=Vector(item.draw.foliage);
        constants.legacyView=Matrix(item.draw.matrices.view);
        constants.cameraMaskMatrix=item.sphereMap?Matrix(item.draw.cameraAlphaTransform):
            constants.legacyView*Matrix(item.draw.cameraAlphaTransform);
        constants.legacyAlpha={float(item.draw.factorAlphaOnly?4:item.draw.factorAlpha?3:item.draw.diffuseAlphaOnly?2:item.draw.textureAlpha?1:0),
            item.draw.ambient[3],item.draw.textureFactor[3],item.cameraAlpha?(item.draw.modulateCameraAlpha?2.f:1.f):0.f};
        // Apply authored linear baseColor once. The Classic bridge mirrors it
        // into its texture stage; that copy is not an additional game tint.
        constants.legacyTint={item.draw.textureFactor[0],item.draw.textureFactor[1],item.draw.textureFactor[2],
            item.draw.materialBaseColorInFactor?0.f:float(item.draw.actorStage)};
        {MapHelper<ObjectConstants> mapped(state.context,objectCB,MAP_WRITE,MAP_FLAG_DISCARD);Require(bool(mapped),"G-DX object map");*mapped=constants;}
        for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})Set(pipeline.srb,stage,"ModernObject",objectCB);
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"ModernLighting",lightCB);
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"PreintegratedBRDF",brdf->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"IrradianceMap",unitEnvironment->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"SkyIrradiance",atmosphere->Irradiance());
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"SkyEnvironment",atmosphere->PrefilteredEnvironment());
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"IBLSampler",iblSampler);
        if(forwardPass) {
            Set(pipeline.srb,SHADER_TYPE_PIXEL,"Composite",compositeCB);
            Set(pipeline.srb,SHADER_TYPE_PIXEL,"ShadowMap",shadow->GetSRV());Set(pipeline.srb,SHADER_TYPE_PIXEL,"ShadowSampler",shadowSampler);
            auto* aoTexture=config.ambientOcclusion!=Graphics::AmbientOcclusionQuality::Off&&ao?ao->GetAmbientOcclusionSRV():white->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
            Set(pipeline.srb,SHADER_TYPE_PIXEL,"ScreenAO",aoTexture);
        }
        Set(pipeline.srb,SHADER_TYPE_VERTEX,"SkinningPalette",item.palette);
        const char* names[]{"BaseMap","NormalMap","RoughnessMap","MetallicMap","AOMap","EmissiveMap"};
        for(unsigned i=0;i<6;++i)Set(pipeline.srb,SHADER_TYPE_PIXEL,names[i],item.textures[i]?item.textures[i].RawPtr():item.textures[0].RawPtr());
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"MaterialSampler",MaterialSampler(item.draw.sampling,item.draw.anisotropic,item.draw.maxAnisotropy));
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"CameraAlphaTexture",item.sphereMap?item.sphereMap.RawPtr():
            item.cameraAlpha?item.cameraAlpha.RawPtr():item.textures[0].RawPtr());
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"CameraAlphaSampler",MaterialSampler(item.draw.cameraAlphaSampling,item.draw.cameraAlphaAnisotropic,item.draw.cameraAlphaMaxAnisotropy));
        state.context->SetPipelineState(pipeline.pso);
        IBuffer* buffers[]{item.vertices,item.extras,item.tangents,item.instances};Uint64 offsets[]{0,0,item.tangentOffset,0};
        state.context->SetVertexBuffers(0,item.instances?4:item.tangents?3:item.auxiliary?2:1,buffers,offsets,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        state.context->SetIndexBuffer(item.indices,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        state.context->CommitShaderResources(pipeline.srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs args{item.draw.indexCount,item.indexType,DRAW_FLAG_VERIFY_ALL};args.FirstIndexLocation=item.draw.firstIndex;args.BaseVertex=item.baseVertex;
        args.NumInstances=item.instanceCount;
        state.context->DrawIndexed(args);
        if(shadowPass)++stats.shadowDraws;else {
            ++stats.meshDraws;
            if(material.model==AssetRuntime::MaterialModel::PBRMetallicRoughness)++stats.pbrMaterialDraws;
            else ++stats.legacyMaterialDraws;
            if(item.sphereMap&&item.draw.actorStage==ActorMaterialStage::Specular)++stats.authoredShimmerDraws;
        }
    }
    void Camera(const TerrainMatrices& matrices);
    void TerrainShaders(unsigned variant) {
        if(terrainVS[variant])return;
        ShaderMacroHelper macros;macros.Add("GDX_SHADOW",bool(variant&1));macros.Add("GDX_SOLID",bool(variant&2));
        terrainVS[variant]=Shader(modernTerrainShader,"TerrainVS",SHADER_TYPE_VERTEX,macros);
        terrainPS[variant]=Shader(modernTerrainShader,"TerrainPS",SHADER_TYPE_PIXEL,macros);
        ++stats.terrainShaderVariants;
    }
    void DrawTerrain(const ModernTerrainSubmission& item,bool shadowPass,const float4x4& lightVP) {
        auto& state=State();const auto& p=item.parameters;
        const unsigned key=unsigned(item.strip)+2*unsigned(p.blend)+4*unsigned(shadowPass)+8*unsigned(item.solid);
        auto& pipeline=terrainPipelines[key];
        if(!pipeline.pso) {
            const unsigned variant=unsigned(shadowPass)|unsigned(item.solid)<<1;
            TerrainShaders(variant);
            auto vs=terrainVS[variant],ps=terrainPS[variant];
            LayoutElement layout[]={{0,0,3,VT_FLOAT32,False,0,24},{1,0,3,VT_FLOAT32,False,12,24},
                {2,1,4,VT_FLOAT32,False,0,36},{3,1,1,VT_FLOAT32,False,16,36},{4,1,2,VT_FLOAT32,False,20,36},{5,1,2,VT_FLOAT32,False,28,36}};
            GraphicsPipelineStateCreateInfo ci;ci.PSODesc.Name="G-DX terrain splat adapter";ci.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            ci.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
            auto& g=ci.GraphicsPipeline;g.NumRenderTargets=shadowPass?0:4;
            for(unsigned i=0;i<4;++i)g.RTVFormats[i]=shadowPass?TEX_FORMAT_UNKNOWN:surfaces[i]->GetDesc().Format;
            g.DSVFormat=shadowPass?TEX_FORMAT_D32_FLOAT:TEX_FORMAT_D24_UNORM_S8_UINT;
            g.PrimitiveTopology=item.strip?PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP:PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            g.InputLayout={layout,shadowPass?2u:6u};g.RasterizerDesc.CullMode=CULL_MODE_NONE;
            if(shadowPass){g.RasterizerDesc.DepthBias=100;g.RasterizerDesc.SlopeScaledDepthBias=1.f;g.RasterizerDesc.DepthBiasClamp=.002f;}
            g.DepthStencilDesc.DepthEnable=True;g.DepthStencilDesc.DepthWriteEnable=True;g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;
            for(auto& b:g.BlendDesc.RenderTargets){b.BlendEnable=!shadowPass&&p.blend;b.SrcBlend=b.SrcBlendAlpha=BLEND_FACTOR_SRC_ALPHA;b.DestBlend=b.DestBlendAlpha=BLEND_FACTOR_INV_SRC_ALPHA;}
            ci.pVS=vs;ci.pPS=ps;{FirstUseAudit timing("pso",ci.PSODesc.Name);state.device->CreateGraphicsPipelineState(ci,&pipeline.pso);}Require(bool(pipeline.pso),"G-DX terrain PSO");
            ++stats.psoCount;
            pipeline.pso->CreateShaderResourceBinding(&pipeline.srb,true);
        }
        Buffer(terrainCB,sizeof(TerrainConstants),"G-DX terrain constants");
        TerrainConstants data{};data.matrices=item.matrices;
        if(shadowPass){const auto identity=float4x4::Identity();std::memcpy(data.matrices.view.data(),&identity,64);std::memcpy(data.matrices.projection.data(),&lightVP,64);}
        data.colorTransform=p.colorTransform;data.alphaTransform=p.alphaTransform;data.factor=p.textureFactor;
        data.modes={unsigned(p.colorOp)|(p.vertexUV?8u:0u),unsigned(p.alphaOp),0,unsigned(p.alphaReference+1)};
        {MapHelper<TerrainConstants> mapped(state.context,terrainCB,MAP_WRITE,MAP_FLAG_DISCARD);Require(bool(mapped),"G-DX terrain map");*mapped=data;}
        Set(pipeline.srb,SHADER_TYPE_VERTEX,"ModernTerrain",terrainCB);Set(pipeline.srb,SHADER_TYPE_PIXEL,"ModernTerrain",terrainCB);
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"ModernLighting",lightCB);
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"ColorTexture",item.color);Set(pipeline.srb,SHADER_TYPE_PIXEL,"AlphaTexture",item.alpha);
        Set(pipeline.srb,SHADER_TYPE_PIXEL,"ColorSampler",item.colorSampler);Set(pipeline.srb,SHADER_TYPE_PIXEL,"AlphaSampler",item.alphaSampler);
        state.context->SetPipelineState(pipeline.pso);
        IBuffer* buffers[]{item.vertices,item.attributes};Uint64 offsets[]{0,0};
        state.context->SetVertexBuffers(0,shadowPass?1:2,buffers,offsets,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        state.context->SetIndexBuffer(item.indices,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        state.context->CommitShaderResources(pipeline.srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        state.context->DrawIndexed(DrawIndexedAttribs{item.count,VT_UINT16,DRAW_FLAG_VERIFY_ALL});
        if(shadowPass)++stats.shadowDraws;else ++stats.terrainDraws;
    }
};

DiligentModernRenderer::DiligentModernRenderer(DiligentD3D11Backend& backend):impl_(std::make_unique<Impl>(backend)) {++liveModernRenderers;}
DiligentModernRenderer::~DiligentModernRenderer() {
    const auto stats=Stats();impl_.reset();--liveModernRenderers;
    if(verboseDiagnostics)try {
        std::ofstream log("gdx-renderer.log",std::ios::app);
        log<<"frames="<<stats.frames<<" meshDraws="<<stats.meshDraws<<" terrainDraws="<<stats.terrainDraws
           <<" shadowDraws="<<stats.shadowDraws<<" lightUploads="<<stats.lightUploads<<" lightBufferCreations="<<stats.lightBufferCreations
           <<" psoCount="<<stats.psoCount<<" meshShaderVariants="<<stats.meshShaderVariants
           <<" terrainShaderVariants="<<stats.terrainShaderVariants<<" ownedTargetBytes="<<stats.targetBytes
           <<" shadowCpuSubmitMs="<<stats.shadowSubmitMilliseconds<<" aoCpuSubmitMs="<<stats.aoSubmitMilliseconds
           <<" hdrTargetBytes="<<stats.hdrTargetBytes<<" atmosphereTargetBytes="<<stats.atmosphereTargetBytes
           <<" atmosphereCpuSubmitMs="<<stats.atmosphereSubmitMilliseconds<<" bloomCpuSubmitMs="<<stats.bloomSubmitMilliseconds
           <<" toneMapCpuSubmitMs="<<stats.toneMapSubmitMilliseconds<<" compositeCpuSubmitMs="<<stats.compositeSubmitMilliseconds
           <<" toneMappedFrames="<<stats.toneMappedFrames
           <<" legacyMaterialDraws="<<stats.legacyMaterialDraws<<" pbrMaterialDraws="<<stats.pbrMaterialDraws
           <<" authoredShimmerDraws="<<stats.authoredShimmerDraws
           <<" waterFrames="<<stats.waterFrames<<" waterDraws="<<stats.waterDraws<<" ssrFrames="<<stats.ssrFrames
           <<" ssrFallbacks="<<stats.ssrFallbacks<<" waterTargetBytes="<<stats.waterTargetBytes
           <<" waterResourceCreations="<<stats.waterResourceCreations<<" waterCpuSubmitMs="<<stats.waterSubmitMilliseconds
           <<" ssrCpuSubmitMs="<<stats.ssrSubmitMilliseconds<<" WaterRenderers="<<liveWaterRenderers
           <<" ModernRenderers="<<liveModernRenderers<<'\n';
    }catch(...){}
}
ModernFrameStats DiligentModernRenderer::Stats() const {
    auto result=impl_->stats;if(impl_->water){const auto w=impl_->water->Stats();
        result.waterFrames=w.frames;result.waterDraws=w.draws;result.ssrFrames=w.ssrFrames;result.ssrFallbacks=w.ssrFallbacks;
        result.waterTargetBytes=w.targetBytes;result.waterResourceCreations=w.resourceCreations;
        result.waterSubmitMilliseconds=w.waterCpuMilliseconds;result.ssrSubmitMilliseconds=w.ssrCpuMilliseconds;}
    return result;
}
bool DiligentModernRenderer::Active() const {return impl_->active||impl_->forward;}
void DiligentModernRenderer::BeginForwardWorld() {impl_->forward=impl_->hasCamera&&impl_->hasDepth&&!impl_->active;if(impl_->forward){impl_->worldOpen=true;BindWorldTarget();}}
void DiligentModernRenderer::EndForwardWorld() {
    auto& s=*impl_;s.forward=false;
    for(auto& pair:s.pipelines)for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})
        for(const char* name:{"BaseMap","NormalMap","RoughnessMap","MetallicMap","AOMap","EmissiveMap","SkinningPalette","ScreenAO","ShadowMap","CameraAlphaTexture"})Impl::Set(pair.second.srb,stage,name,nullptr);
    if(!s.deferToneMapping)FinishWorld();
}
void DiligentModernRenderer::ResetFrame() {shadowCasterCollection=false;impl_->worldOpen=impl_->sceneReady=impl_->active=impl_->forward=impl_->hasDepth=false;impl_->casters.clear();impl_->terrainCasters.clear();impl_->transparent.clear();if(impl_->water)impl_->water->ResetFrame();impl_->waterStarted=impl_->waterFinished=false;}
bool DiligentModernRenderer::HDRWorldActive() const {return impl_->worldOpen&&!impl_->active;}
void DiligentModernRenderer::BindWorldTarget() {
    auto& s=*impl_;auto* target=s.hdrScene->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    s.State().context->SetRenderTargets(1,&target,DepthView(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}
ITextureView* DiligentModernRenderer::DepthView() const {return impl_->hasDepth?impl_->depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL):nullptr;}
void DiligentModernRenderer::BindTargets() {
    auto& s=*impl_;
    if(s.forward||HDRWorldActive()){BindWorldTarget();return;}
    if(!s.active)return;ITextureView* targets[4];
    for(unsigned i=0;i<4;++i)targets[i]=s.surfaces[i]->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
    s.State().context->SetRenderTargets(4,targets,DepthView(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}
void DiligentModernRenderer::ReleaseWindowResources() {
    shadowCasterCollection=false;
    auto& s=*impl_;s.worldOpen=s.sceneReady=s.active=s.forward=s.hasDepth=false;s.casters.clear();s.composite.srb.Release();s.tone.srb.Release();s.hdrScene.Release();s.depth.Release();s.background.Release();s.motion.Release();s.white.Release();s.bloom.reset();
    for(auto& texture:s.surfaces)texture.Release();s.post.reset();s.ao.reset();s.terrainCasters.clear();s.transparent.clear();
    if(s.water)s.water->ReleaseWindowResources();s.waterWarmed=false;
}
void DiligentModernRenderer::Begin(const Graphics::SceneLighting& light,bool deferToneMapping) {
    shadowCasterCollection=false;impl_->shadowsPrepared=false;
    auto& s=*impl_;s.sceneReady=s.worldOpen=s.active=s.forward=false;s.deferToneMapping=deferToneMapping;s.hasDepth=false;s.config=GetGraphicsRuntimeConfig();s.casters.clear();s.terrainCasters.clear();s.transparent.clear();
    if(s.config.style!=Graphics::GraphicsStyle::Modern)return;
    s.Resources();s.IBL();
    if(loadingPrewarm) {
        // H2 adds auxiliary instanced meshes only: color, shadow and forward,
        // each with/without authored tangents. No unused instanced skin variants.
        for(unsigned variant=0;variant<s.vertexShaders.size();++variant)
            if(variant<18||variant%3==2)s.MeshShaders(variant);
        for(unsigned variant=0;variant<s.terrainVS.size();++variant)s.TerrainShaders(variant);
    }
    s.lighting=Graphics::ValidateSceneLighting(light);s.hasCamera=false;++s.frame;
    ++s.stats.frames;
    auto& state=s.State();
    CopyTextureAttribs copy;copy.pSrcTexture=state.swapChain->GetCurrentBackBufferRTV()->GetTexture();copy.pDstTexture=s.background;
    copy.SrcTextureTransitionMode=copy.DstTextureTransitionMode=RESOURCE_STATE_TRANSITION_MODE_TRANSITION;state.context->CopyTexture(copy);
    const auto atmosphereStart=std::chrono::steady_clock::now();
    if(!s.atmosphere)s.atmosphere=std::make_unique<DiligentAtmosphere>(state.device,state.context);
    s.atmosphere->Prepare(s.lighting,s.config.modernSky);
    if(!s.water)s.water=std::make_unique<DiligentWater>(state.device,state.context);
    s.water->Prepare(s.width,s.height,s.config);s.waterStarted=s.waterFinished=false;
    s.stats.atmosphereSubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-atmosphereStart).count();
    float clear[]{0,0,0,0};for(auto& target:s.surfaces)state.context->ClearRenderTarget(target->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),clear,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    state.context->ClearDepthStencil(s.depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL),CLEAR_DEPTH_FLAG,1,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    s.active=s.hasDepth=true;BindTargets();Viewport viewport{0,0,float(s.width),float(s.height),0,1};state.context->SetViewports(1,&viewport,s.width,s.height);
}
void DiligentModernRenderer::SetCamera(const TerrainMatrices& matrices){impl_->Camera(matrices);}
void DiligentModernRenderer::Impl::Camera(const TerrainMatrices& matrices) {
    auto& s=*this;
    if(!s.hasCamera){
        s.camera={};s.camera.mView=Matrix(matrices.view);s.camera.mProj=Matrix(matrices.projection);
        // FX camera-Z helpers use positive forward depth. Flip both sides of
        // the view/projection boundary; the world-to-clip product is unchanged.
        if(s.camera.mProj._34<0) {
            const auto flip=float4x4::Scale(1,1,-1);
            s.camera.mView=s.camera.mView*flip;s.camera.mProj=flip*s.camera.mProj;
        }
        s.camera.mViewProj=s.camera.mView*s.camera.mProj;s.camera.mViewInv=s.camera.mView.Inverse();s.camera.mProjInv=s.camera.mProj.Inverse();s.camera.mViewProjInv=s.camera.mViewProj.Inverse();
        s.camera.f4Position={s.camera.mViewInv._41,s.camera.mViewInv._42,s.camera.mViewInv._43,1};
        s.camera.f4ViewportSize={float(s.width),float(s.height),1.f/s.width,1.f/s.height};s.camera.fHandness=-1;s.camera.uiFrameIndex=waterTestSeconds>=0?0:s.frame;
        s.camera.mProj.GetNearFarClipPlanes(s.camera.fNearPlaneZ,s.camera.fFarPlaneZ,false);
        s.Buffer(s.lightCB,sizeof(LightConstants),"G-DX single sun buffer");
        const auto& l=s.lighting;LightConstants light{{l.sunDirection[0],l.sunDirection[1],l.sunDirection[2],0},
            {l.sunColor[0]*l.sunIntensity,l.sunColor[1]*l.sunIntensity,l.sunColor[2]*l.sunIntensity,0},
            {l.ambient[0],l.ambient[1],l.ambient[2],0},s.camera.f4Position,{l.environmentColor[0],l.environmentColor[1],l.environmentColor[2],l.skyIBLIntensity}};
        light.camera.w=Matrix(matrices.projection)._34<0?1.f:-1.f;
        MapHelper<LightConstants> mapped(s.State().context,s.lightCB,MAP_WRITE,MAP_FLAG_DISCARD);Require(bool(mapped),"G-DX light map");*mapped=light;s.hasCamera=true;++s.stats.lightUploads;
    }
}
void DiligentModernRenderer::DrawTerrain(const ModernTerrainSubmission& item) {
    auto& s=*impl_;if(!s.active)return;s.Camera(item.matrices);
    if(!shadowCasterCollection)s.DrawTerrain(item,false,s.camera.mViewProj);
    // A patch's layers share geometry. Keep only the immutable depth caster;
    // the dynamic STP attribute stream is consumed synchronously above.
    if(std::none_of(s.terrainCasters.begin(),s.terrainCasters.end(),[&](const auto& c){return c.vertices==item.vertices&&c.indices==item.indices&&c.matrices.world==item.matrices.world;})) {
        auto caster=item;caster.attributes.Release();caster.color.Release();caster.alpha.Release();caster.colorSampler.Release();caster.alphaSampler.Release();
        s.terrainCasters.push_back(std::move(caster));
    }
}
void DiligentModernRenderer::Draw(const ModernMeshSubmission& item) {
    auto& s=*impl_;if(s.forward){s.DrawMesh(item,false,Matrix(item.draw.matrices.view)*Matrix(item.draw.matrices.projection),true);return;}
    if(!s.active)return;s.Camera(item.draw.matrices);
    if(shadowCasterCollection){if(!item.draw.blend&&item.draw.depthWrite)s.casters.push_back(item);return;}
    if(item.draw.blend){s.transparent.push_back(item);return;}
    s.DrawMesh(item,false,s.camera.mViewProj);
    if(!item.draw.blend&&item.draw.depthWrite)s.casters.push_back(item);
}

bool DiligentModernRenderer::Impl::PrepareShadows() {
    auto& s=*this;auto& state=State();
    if(shadowsPrepared)return hasCamera&&config.shadows!=Graphics::ShadowQuality::Off&&lighting.sunIntensity>0;
    shadowsPrepared=true;
    const bool useShadows=s.hasCamera&&s.config.shadows!=Graphics::ShadowQuality::Off&&s.lighting.sunIntensity>0;
    unsigned size=1,count=1;
    if(useShadows){
        switch(s.config.shadows){
        case Graphics::ShadowQuality::Low: size=512;count=1;break;
        case Graphics::ShadowQuality::LegacySolo:
        case Graphics::ShadowQuality::Medium:size=1024;count=2;break;
        case Graphics::ShadowQuality::High:size=1536;count=3;break;
        case Graphics::ShadowQuality::Ultra:size=2048;count=4;break;
        default:break;
        }
    }
    if(!s.shadowSampler){SamplerDesc desc;desc.MinFilter=desc.MagFilter=FILTER_TYPE_COMPARISON_LINEAR;desc.MipFilter=FILTER_TYPE_COMPARISON_POINT;
        desc.ComparisonFunc=COMPARISON_FUNC_LESS_EQUAL;desc.AddressU=desc.AddressV=TEXTURE_ADDRESS_CLAMP;
        state.device->CreateSampler(desc,&s.shadowSampler);Require(bool(s.shadowSampler),"G-DX shadow sampler");}
    if(!s.shadow||s.shadowSize!=size||s.cascades!=count){
        s.shadow=std::make_unique<ShadowMapManager>();ShadowMapManager::InitInfo init;
        init.Format=TEX_FORMAT_D32_FLOAT;init.Resolution=size;init.NumCascades=count;init.ShadowMode=SHADOW_MODE_PCF;init.pComparisonSampler=s.shadowSampler;
        s.shadow->Initialize(state.device,nullptr,init);Require(s.shadow->GetSRV()!=nullptr,"FX shadow map");s.shadowSize=size;s.cascades=count;
        s.composite.srb.Release();
    }
    if(useShadows){
        float3 direction{s.lighting.sunDirection[0],s.lighting.sunDirection[1],s.lighting.sunDirection[2]};
        ShadowMapManager::DistributeCascadeInfo distribution;
        distribution.pCameraView=&s.camera.mView;distribution.pCameraWorld=&s.camera.mViewInv;distribution.pCameraProj=&s.camera.mProj;
        distribution.pLightDir=&direction;distribution.PackMatrixRowMajor=true;
        distribution.SnapCascades=distribution.StabilizeExtents=true;
        const float distance=std::min({s.config.viewDistance,2500.f*count,s.camera.fFarPlaneZ});
        distribution.LightSpaceDepthPadding=distance;
        distribution.AdjustCascadeRange=[distance](int cascade,float& minZ,float& maxZ){if(cascade==-1)maxZ=std::max(minZ+.01f,std::min(maxZ,distance));};
        s.shadow->DistributeCascades(distribution,s.shadowAttribs);
        // The FX adapter scales this per cascade to preserve the original
        // world-space receiver bias after extending the caster depth range.
        s.shadowAttribs.fFixedDepthBias=.0005f;
        for(unsigned cascade=0;cascade<count;++cascade)
            ExtractViewFrustumPlanesFromMatrix(s.shadow->GetCascadeTranform(cascade).WorldToLightProjSpace,s.shadowFrusta[cascade],false);
    }
    return useShadows;
}
bool DiligentModernRenderer::BeginShadowCollection() {
    auto& s=*impl_;const auto start=std::chrono::steady_clock::now();
    if(!s.active||!s.PrepareShadows())return false;
    s.stats.shadowSubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    s.casters.clear();s.terrainCasters.clear();shadowCasterCollection=true;return true;
}
void DiligentModernRenderer::EndShadowCollection() {shadowCasterCollection=false;}
bool DiligentModernRenderer::ShadowCasterVisible(const std::array<float,3>& center,float radius) const {
    const auto& s=*impl_;if(!s.shadowsPrepared||!std::isfinite(radius)||radius<0)return false;
    const float3 c{center[0],center[1],center[2]},r{radius,radius,radius};
    for(unsigned i=0;i<s.cascades;++i)
        if(GetBoxVisibility(s.shadowFrusta[i],BoundBox{c-r,c+r})!=BoxVisibility::Invisible)return true;
    return false;
}

void DiligentModernRenderer::End() {
    auto& s=*impl_;if(!s.active)return;
    auto& state=s.State();
    const auto shadowStart=std::chrono::steady_clock::now();
    state.context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const bool useShadows=s.PrepareShadows();
    const auto size=s.shadowSize,count=s.cascades;
    if(useShadows){
        for(unsigned cascade=0;cascade<count;++cascade){
            auto* dsv=s.shadow->GetCascadeDSV(cascade);state.context->SetRenderTargets(0,nullptr,dsv,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            state.context->ClearDepthStencil(dsv,CLEAR_DEPTH_FLAG,1,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            Viewport viewport{0,0,float(size),float(size),0,1};state.context->SetViewports(1,&viewport,size,size);
            for(const auto& caster:s.casters)s.DrawMesh(caster,true,s.shadow->GetCascadeTranform(cascade).WorldToLightProjSpace);
            for(const auto& caster:s.terrainCasters)s.DrawTerrain(caster,true,s.shadow->GetCascadeTranform(cascade).WorldToLightProjSpace);
        }
    } else state.context->ClearDepthStencil(s.shadow->GetCascadeDSV(0),CLEAR_DEPTH_FLAG,1,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    state.context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ITextureView* aoView=s.white->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    s.stats.shadowSubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-shadowStart).count();
    const auto aoStart=std::chrono::steady_clock::now();
    const bool useAO=s.hasCamera&&s.config.ambientOcclusion!=Graphics::AmbientOcclusionQuality::Off;
    if(useAO||s.config.bloom||Graphics::ResolveWater(s.config).ssr||loadingPrewarm){
        FirstUseAudit timing("fx-total","PostFX-SSAO-first",!s.post||!s.ao);
        if(!s.post){PostFXContext::CreateInfo ci;ci.PackMatrixRowMajor=true;s.post=std::make_unique<PostFXContext>(state.device,ci);}
        s.post->PrepareResources(state.device,{waterTestSeconds>=0?0:s.frame,s.width,s.height,s.width,s.height},PostFXContext::FEATURE_FLAG_NONE);
        s.Buffer(s.cameraCB,2*sizeof(CameraAttribs),"G-DX shared current/previous camera");
        {MapHelper<CameraAttribs> camera(state.context,s.cameraCB,MAP_WRITE,MAP_FLAG_DISCARD);Require(bool(camera),"G-DX camera map");camera[0]=camera[1]=s.camera;}
        PostFXContext::RenderAttributes attributes;attributes.pDevice=state.device;attributes.pDeviceContext=state.context;
        attributes.pCurrDepthBufferSRV=attributes.pPrevDepthBufferSRV=s.depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        attributes.pMotionVectorsSRV=s.motion->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);attributes.pCameraAttribsCB=s.cameraCB;
        s.post->Execute(attributes);
        Require(s.post->IsPSOsReady(),"FX shared PostFX pipelines are unavailable");
    }
    if(useAO){
        if(!s.ao)s.ao=std::make_unique<ScreenSpaceAmbientOcclusion>(state.device,ScreenSpaceAmbientOcclusion::CreateInfo{});
        const auto flags=s.config.ambientOcclusion==Graphics::AmbientOcclusionQuality::SSAO?ScreenSpaceAmbientOcclusion::FEATURE_FLAG_HALF_RESOLUTION:ScreenSpaceAmbientOcclusion::FEATURE_FLAG_NONE;
        s.ao->PrepareResources(state.device,state.context,s.post.get(),flags);
        HLSL::ScreenSpaceAmbientOcclusionAttribs settings;settings.EffectRadius=120.f;
        // Spatial-only until previous animation/wind/object state is available.
        // The scratch motion/depth inputs are never presented as valid history.
        settings.ResetAccumulation=TRUE;settings.TemporalStabilityFactor=0;
        ScreenSpaceAmbientOcclusion::RenderAttributes aoAttributes;
        aoAttributes.pDevice=state.device;aoAttributes.pDeviceContext=state.context;aoAttributes.pPostFXContext=s.post.get();
        aoAttributes.pDepthBufferSRV=s.depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);aoAttributes.pNormalBufferSRV=s.surfaces[3]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);aoAttributes.pSSAOAttribs=&settings;
        s.ao->Execute(aoAttributes);Require(s.ao->IsPSOsReady(),"FX SSAO pipelines are unavailable; placeholder AO is not accepted");
        aoView=s.ao->GetAmbientOcclusionSRV();Require(aoView!=nullptr,"FX AO output");
    }
    s.stats.aoSubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-aoStart).count();
    s.stats.hdrTargetBytes=std::uint64_t(s.width)*s.height*8;
    s.stats.atmosphereTargetBytes=s.atmosphere->TargetBytes();
    s.stats.targetBytes=std::uint64_t(s.width)*s.height*53+std::uint64_t(size)*size*count*4+256*256*4+s.stats.atmosphereTargetBytes;
    const auto compositeStart=std::chrono::steady_clock::now();
    if(!s.composite.pso){
        GraphicsPipelineStateCreateInfo ci;ci.PSODesc.Name="G-DX direct shadow / indirect AO composition";ci.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        ci.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        ci.GraphicsPipeline.NumRenderTargets=1;ci.GraphicsPipeline.RTVFormats[0]=TEX_FORMAT_RGBA16_FLOAT;
        ci.GraphicsPipeline.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;ci.GraphicsPipeline.RasterizerDesc.CullMode=CULL_MODE_NONE;
        ci.GraphicsPipeline.DepthStencilDesc.DepthEnable=False;
        auto vs=s.Shader(compositeShader,"CompositeVS",SHADER_TYPE_VERTEX),ps=s.Shader(compositeShader,"CompositePS",SHADER_TYPE_PIXEL);
        ci.pVS=vs;ci.pPS=ps;{FirstUseAudit timing("pso",ci.PSODesc.Name);state.device->CreateGraphicsPipelineState(ci,&s.composite.pso);}Require(bool(s.composite.pso),"G-DX composition pipeline");
    }
    if(!s.composite.srb)s.composite.pso->CreateShaderResourceBinding(&s.composite.srb,true);
    Require(bool(s.composite.srb),"G-DX composition binding");
    s.Buffer(s.compositeCB,sizeof(CompositeConstants),"G-DX composition constants");
    {MapHelper<CompositeConstants> data(state.context,s.compositeCB,MAP_WRITE,MAP_FLAG_DISCARD);Require(bool(data),"G-DX composition map");
        data->camera=s.camera;data->shadows=s.shadowAttribs;data->options={useShadows?1.f:0.f,useAO?1.f:0.f,s.hasCamera?1.f:0.f,0};
        data->horizonColor={std::pow(s.lighting.fogColor[0],2.2f),std::pow(s.lighting.fogColor[1],2.2f),std::pow(s.lighting.fogColor[2],2.2f),1};
        data->sunDirection={s.lighting.sunDirection[0],s.lighting.sunDirection[1],s.lighting.sunDirection[2],0};
        data->sunRadiance={s.lighting.sunColor[0]*s.lighting.sunIntensity,s.lighting.sunColor[1]*s.lighting.sunIntensity,s.lighting.sunColor[2]*s.lighting.sunIntensity,0};}
    Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"Composite",s.compositeCB);
    const char* names[]{"Direct","Indirect","Emission"};
    for(unsigned i=0;i<3;++i)Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,names[i],s.surfaces[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"Depth",s.depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"Background",s.background->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"AO",aoView);Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"ShadowMap",s.shadow->GetSRV());
    Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"ShadowSampler",s.shadowSampler);
    Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"Sky",s.atmosphere->Sky());Impl::Set(s.composite.srb,SHADER_TYPE_PIXEL,"SkySampler",s.atmosphere->Sampler());
    auto* target=s.hdrScene->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);state.context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Viewport viewport{0,0,float(s.width),float(s.height),0,1};state.context->SetViewports(1,&viewport,s.width,s.height);
    state.context->SetPipelineState(s.composite.pso);state.context->CommitShaderResources(s.composite.srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    state.context->Draw(DrawAttribs{3,DRAW_FLAG_VERIFY_ALL});
    s.active=false;s.worldOpen=s.sceneReady=true;
    s.stats.compositeSubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-compositeStart).count();
    state.context->SetRenderTargets(1,&target,DepthView(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    for(const auto& item:s.transparent)s.DrawMesh(item,false,s.camera.mViewProj,true);
    s.transparent.clear();
    s.casters.clear();
    s.terrainCasters.clear();
    // Dynamic SRBs must not keep assets or palettes alive across map changes.
    for(auto& pair:s.pipelines)for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})
        for(const char* name:{"BaseMap","NormalMap","RoughnessMap","MetallicMap","AOMap","EmissiveMap","SkinningPalette","ScreenAO","ShadowMap","CameraAlphaTexture"})Impl::Set(pair.second.srb,stage,name,nullptr);
    for(auto& pair:s.terrainPipelines)for(const char* name:{"ColorTexture","AlphaTexture"})Impl::Set(pair.second.srb,SHADER_TYPE_PIXEL,name,nullptr);
    if(!s.deferToneMapping)FinishWorld();
}

void DiligentModernRenderer::DrawWater(const EffectVertex* vertices,unsigned count,const EffectDraw& draw) {
    auto& s=*impl_;if(!s.worldOpen||!s.sceneReady||!s.water||s.waterFinished)return;
    if(!s.waterStarted) {
        WaterFrameInputs input;input.camera=s.camera;input.shadows=s.shadowAttribs;input.lighting=s.lighting;
        input.scene=s.hdrScene->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);input.depth=s.depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        input.normal=s.surfaces[3]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        input.sky=s.atmosphere->Sky();input.skySampler=s.atmosphere->Sampler();input.shadow=s.shadow->GetSRV();input.shadowSampler=s.shadowSampler;
        input.shadowsEnabled=s.config.shadows!=Graphics::ShadowQuality::Off&&s.lighting.sunIntensity>0;
        s.water->Begin(input);s.waterStarted=true;
    }
    s.water->Draw(vertices,count,draw);
}
void DiligentModernRenderer::FinishWater() {
    auto& s=*impl_;if(s.waterFinished||!s.worldOpen||!s.sceneReady||!s.water)return;
    // Prime all water/SSR passes during Modern initialization, even on dry maps.
    if(!s.waterStarted&&(!s.waterWarmed||s.water->NeedsPrewarm()||loadingPrewarm))DrawWater(nullptr,0,{});
    if(s.waterStarted)s.water->Finish(s.post.get(),s.cameraCB,s.motion->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    s.waterFinished=s.waterWarmed=true;BindWorldTarget();
}
void DiligentModernRenderer::FinishWorld() {
    auto& s=*impl_;if(!s.worldOpen||!s.sceneReady)return;auto& state=s.State();
    FinishWater();
    state.context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    ITextureView* source=s.hdrScene->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    const auto config=Graphics::ResolveAtmosphere(s.lighting,s.config);
    if(s.config.bloom||loadingPrewarm) {
        const auto start=std::chrono::steady_clock::now();FirstUseAudit timing("fx-total","bloom-first",!s.bloom);
        if(!s.bloom)s.bloom=std::make_unique<Bloom>(state.device,Bloom::CreateInfo{});
        s.bloom->PrepareResources(state.device,state.context,s.post.get(),Bloom::FEATURE_FLAG_NONE);
        HLSL::BloomAttribs settings;settings.Threshold=config.bloomThreshold;settings.Intensity=config.bloomIntensity;
        settings.SoftTreshold=.05f;settings.Radius=config.bloomRadius;
        Bloom::RenderAttributes attributes;attributes.pDevice=state.device;attributes.pDeviceContext=state.context;
        attributes.pPostFXContext=s.post.get();attributes.pColorBufferSRV=source;attributes.pBloomAttribs=&settings;
        s.bloom->Execute(attributes);Require(s.bloom->IsPSOsReady(),"G56 FX Bloom pipelines unavailable");
        if(s.config.bloom)source=s.bloom->GetBloomTextureSRV();
        s.stats.bloomSubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    }
    const auto start=std::chrono::steady_clock::now();
    if(!s.tone.pso) {
        auto vs=s.Shader(toneShader,"ToneVS",SHADER_TYPE_VERTEX),ps=s.Shader(toneShader,"TonePS",SHADER_TYPE_PIXEL);
        GraphicsPipelineStateCreateInfo ci;ci.PSODesc.Name="G56 Diligent tone mapping SDR output";ci.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        ci.PSODesc.ResourceLayout.DefaultVariableType=SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;
        ci.GraphicsPipeline.NumRenderTargets=1;ci.GraphicsPipeline.RTVFormats[0]=state.swapChain->GetDesc().ColorBufferFormat;
        ci.GraphicsPipeline.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;ci.GraphicsPipeline.RasterizerDesc.CullMode=CULL_MODE_NONE;
        ci.GraphicsPipeline.DepthStencilDesc.DepthEnable=False;ci.pVS=vs;ci.pPS=ps;
        {FirstUseAudit timing("pso",ci.PSODesc.Name);state.device->CreateGraphicsPipelineState(ci,&s.tone.pso);}Require(bool(s.tone.pso),"G56 tone mapping PSO");
    }
    if(!s.tone.srb)s.tone.pso->CreateShaderResourceBinding(&s.tone.srb,true);
    s.Buffer(s.toneCB,sizeof(ToneConstants),"G56 fixed exposure");
    {MapHelper<ToneConstants> data(state.context,s.toneCB,MAP_WRITE,MAP_FLAG_DISCARD);Require(bool(data),"G56 exposure map");data->exposure={config.exposure,0,0,0};}
    Impl::Set(s.tone.srb,SHADER_TYPE_PIXEL,"Tone",s.toneCB);Impl::Set(s.tone.srb,SHADER_TYPE_PIXEL,"Scene",source);
    auto* target=state.swapChain->GetCurrentBackBufferRTV();state.context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Viewport viewport{0,0,float(s.width),float(s.height),0,1};state.context->SetViewports(1,&viewport,s.width,s.height);
    state.context->SetPipelineState(s.tone.pso);state.context->CommitShaderResources(s.tone.srb,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    state.context->Draw(DrawAttribs{3,DRAW_FLAG_VERIFY_ALL});
    Impl::Set(s.tone.srb,SHADER_TYPE_PIXEL,"Scene",nullptr);
    s.worldOpen=s.forward=false;++s.stats.toneMappedFrames;
    state.context->SetRenderTargets(1,&target,DepthView(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    s.stats.toneMapSubmitMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
}
