#include "DiligentStaticObjectRenderer.h"
#include "GpuSkinningShader.h"
#include "PBRShader.h"
#include "SceneLightingShader.h"
#include "ShadowAmbientShader.h"
#include "GraphicsConfig.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include "ActorRenderData.h"
#include "DiligentD3D11BackendInternal.h"
#include "Diagnostics.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/Sampler.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Renderer
{
using namespace Diligent;
namespace
{
struct Counters { uint32_t geometry=0, textures=0; };
// ZiiNAN: GPU skinning actor coverage
struct SkinMeshBuffers
{
    RefCntAutoPtr<IBuffer> vertices, indices, rigidVertices;
    RefCntAutoPtr<IBuffer> materialVertices;
    std::vector<std::shared_ptr<const StaticSkinnedMeshData>> meshes;
    std::vector<std::shared_ptr<const BoneRemap>> remaps;
    std::vector<uint16_t> validationIndices;
    uint32_t deformCount=0, vertexCount=0;
    SkinMeshBuffers() { ++livePrototypeStaticMeshes; }
    ~SkinMeshBuffers() { --livePrototypeStaticMeshes; }
};
struct SkinPoseBuffer
{
    RefCntAutoPtr<IBuffer> buffer;
    uint64_t identity=0, revision=0;
    std::shared_ptr<const SkeletonLayout> skeleton;
    SkinPoseBuffer() { ++livePrototypePalettes; }
    ~SkinPoseBuffer() { --livePrototypePalettes; }
};
struct Geometry final : StaticObjectGeometry
{
    RefCntAutoPtr<IBuffer> vertices, indices;
    RefCntAutoPtr<IBuffer> extras;
    RefCntAutoPtr<IBuffer> materialVertices;
    std::shared_ptr<SkinMeshBuffers> skin;
    std::shared_ptr<SkinPoseBuffer> pose;
    std::vector<uint16_t> validationIndices;
    std::vector<uint32_t> validationIndices32;
    VALUE_TYPE indexType=VT_UINT16;
    std::size_t IndexCount() const { return indexType==VT_UINT32 ? validationIndices32.size() : validationIndices.size(); }
    uint32_t IndexAt(std::size_t index) const { return indexType==VT_UINT32 ? validationIndices32[index] : validationIndices[index]; }
    uint32_t vertexCount=0;
    bool dynamic=false; // ZiiNAN: Only actor VBs use discard updates.
    Graphics::Vector3 boundsCenter{};
    float boundsRadius{};
    std::shared_ptr<Counters> counters;
    ~Geometry() override {
        if(skin) --livePrototypeGeometry;
        if(counters) --counters->geometry;
    }
};
struct Texture final : TerrainTexture
{
    RefCntAutoPtr<ITexture> texture;
    RefCntAutoPtr<ITextureView> linearView, srgbView;
    RefCntAutoPtr<ISampler> sampler;
    RefCntAutoPtr<IShaderResourceBinding> bindings[48];
    RefCntAutoPtr<ISampler> cameraSampler;
    std::weak_ptr<Texture> cameraImage;
    TerrainSampling cameraSampling{};
    bool cameraAnisotropic=false;
    uint32_t cameraMaxAnisotropy=1;
    TerrainSampling sampling{};
    bool anisotropic=false;
    uint32_t maxAnisotropy=1;
    std::shared_ptr<Counters> counters;
    ~Texture() override { if(counters) --counters->textures; }
};
struct Constants
{
    TerrainMatrices matrices;
    std::array<float,16> normal;
    std::array<float,4> ambient, diffuse, direction, fogColor, fogParameters;
    std::array<uint32_t,4> modes;
    std::array<float,16> cameraAlphaTransform;
    std::array<float,4> pointPositionRange,pointAttenuation,pointAmbient,pointDiffuse;
    std::array<uint32_t,4> alphaModes;
    std::array<float,4> textureFactor; // ZiiNAN: Existing actor stage constant.
    std::array<float,4> spotPositionRange,spotAttenuation,spotAmbient,spotDiffuse,spotDirection,spotCone;
    std::array<float,4> cardRight,cardForward,cardUp,wind,cardPitch;
    std::array<uint32_t,4> vertexModes;
};
static_assert(sizeof(Constants)%16==0 && sizeof(StaticObjectVertex)==32);
struct PBRConstants
{
    TerrainMatrices matrices;
    std::array<float,16> normal;
    std::array<float,4> fogColor,fogParameters,baseColor,emissive,factors;
    std::array<std::array<float,4>,10> uvRows;
    std::array<uint32_t,4> flags,alpha;
    std::array<float,4> fade;
    std::array<float,16> camera;
};
static_assert(sizeof(PBRConstants)%16==0&&sizeof(AssetRuntime::MaterialVertex)==24);
struct PBRBinding
{
    unsigned maps{},srgb{};
    std::array<std::shared_ptr<Texture>,AssetRuntime::MaterialMapCount> images;
    std::weak_ptr<const MaterialRuntime> material;
    RefCntAutoPtr<IShaderResourceBinding> bindings[48];
    IShaderResourceVariable* palettes[48]{};
    IShaderResourceVariable* cameraVariables[48]{};
    IShaderResourceVariable* cameraSamplers[48]{};
    PBRBinding(){++livePBRBindings;}
    ~PBRBinding(){--livePBRBindings;}
};
constexpr char shaderSource[] = R"(
cbuffer ObjectConstants {
 row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection;
 row_major float4x4 NormalTransform;
 float4 Ambient; float4 Diffuse; float4 LightDirection; float4 FogColor; float4 FogParameters;
 uint4 Modes;
 row_major float4x4 CameraAlphaTransform;
 float4 PointPositionRange; float4 PointAttenuation; float4 PointAmbient; float4 PointDiffuse;
 uint4 AlphaModes;
 float4 TextureFactor;
 float4 SpotPositionRange; float4 SpotAttenuation; float4 SpotAmbient; float4 SpotDiffuse; float4 SpotDirection; float4 SpotCone;
 float4 CardRight; float4 CardForward; float4 CardUp; float4 Wind; float4 CardPitch; uint4 VertexModes;
};
Texture2D DiffuseTexture;
SamplerState ObjectSampler;
Texture2D CameraAlphaTexture;
SamplerState CameraAlphaSampler;
struct Output { float4 position:SV_POSITION; float2 uv:TEXCOORD0; float4 diffuse:COLOR0; float fog:TEXCOORD1; float2 cameraUV:TEXCOORD2;
#ifdef MODERN_VEGETATION
 float3 worldNormal:TEXCOORD3;
 float3 worldPosition:TEXCOORD4;
#endif
};
Output VS(float3 position:ATTRIB0, float3 normal:ATTRIB1, float2 uv:ATTRIB2) {
 Output o;
 float4 eye=mul(mul(float4(position,1),World),View);
 o.position=mul(eye,Projection); o.uv=uv;
#ifdef MODERN_VEGETATION
 o.worldPosition=mul(float4(position,1),World).xyz;
#endif
 float3 n=mul(float4(normal,0),NormalTransform).xyz;
 if(Modes.z!=0) n=normalize(n);
 float3 lighting=Ambient.rgb + Diffuse.rgb*max(0,dot(n,LightDirection.xyz));
 if(PointPositionRange.w>0) {
   float3 delta=PointPositionRange.xyz-eye.xyz;
   float distance=length(delta);
   if(distance<=PointPositionRange.w) {
     float attenuation=1/max(1e-20,dot(PointAttenuation.xyz,float3(1,distance,distance*distance)));
     lighting+=attenuation*(PointAmbient.rgb+PointDiffuse.rgb*max(0,dot(n,delta/max(distance,1e-20))));
   }
 }
 o.diffuse=float4(saturate(lighting),Ambient.a);
 // ZiiNAN: The selection screen's existing fixed-function spotlight.
 if(SpotPositionRange.w>0) {
   float3 delta=SpotPositionRange.xyz-eye.xyz; float distance=length(delta);
   float rho=dot(-delta/max(distance,1e-20),SpotDirection.xyz);
   float cone=rho>=SpotCone.x ? 1 : (rho<=SpotCone.y ? 0 : pow(saturate((rho-SpotCone.y)/max(1e-20,SpotCone.x-SpotCone.y)),SpotCone.z));
   if(distance<=SpotPositionRange.w) lighting+=cone/max(1e-20,dot(SpotAttenuation.xyz,float3(1,distance,distance*distance)))*(SpotAmbient.rgb+SpotDiffuse.rgb*max(0,dot(n,delta/max(distance,1e-20))));
   o.diffuse.rgb=saturate(lighting);
 }
 o.cameraUV=mul(eye,CameraAlphaTransform).xy;
 // ZiiNAN: Native camera-space reflection vector, transformed at the vertex stage.
 if(Modes.w==3) o.cameraUV=mul(float4(reflect(normalize(eye.xyz),n),1),CameraAlphaTransform).xy;
 // Original fixed-function diffuse output is an 8-bit color before interpolation.
 o.diffuse=floor(o.diffuse*255+0.5)/255;
 float d=Modes.y!=0 ? length(eye.xyz) : abs(eye.z);
 o.fog=1;
 if(Modes.x==1) o.fog=exp(-FogParameters.z*d);
 if(Modes.x==2) o.fog=exp(-pow(FogParameters.z*d,2));
 if(Modes.x==3) o.fog=(FogParameters.y-d)/(FogParameters.y-FogParameters.x);
 o.fog=saturate(o.fog);
 return o;
}
#ifdef SHADOW_CASTER
cbuffer ShadowMaterial {float4 ShadowBase;float4 ShadowUV[2];};
void PS(Output i,bool front:SV_IsFrontFace) {
 float2 materialUV=float2(dot(ShadowUV[0].xyz,float3(i.uv,1)),dot(ShadowUV[1].xyz,float3(i.uv,1)));
 float4 color=DiffuseTexture.Sample(ObjectSampler,materialUV)*ShadowBase;
#else
float4 PS(Output i,bool front:SV_IsFrontFace
#ifdef AMBIENT_MRT
 ,out float4 ambientDelta:SV_TARGET1
#endif
):SV_TARGET0 {
 float4 color=DiffuseTexture.Sample(ObjectSampler,i.uv);
#endif
#ifdef MODERN_VEGETATION
 float3 n=LightingNormal(i.worldNormal,float3(0,0,1));if(!front)n=-n;
 float3 base=(VertexModes.w&2)?color.rgb:LightingToLinear(color.rgb);
 // Native vegetation COLOR_0 is a linear modulation factor, like glTF colors.
 float3 indirect=base*i.diffuse.rgb*SceneAmbient(n);
 color.rgb=indirect+base*i.diffuse.rgb*SceneSunColor.rgb*saturate(dot(n,SceneSunDirection.xyz))/3.14159265*SunVisibility(i.worldPosition,n);
#else
 color.rgb*=i.diffuse.rgb;
#endif
 if(AlphaModes.x==0) color.a*=i.diffuse.a;
 if(AlphaModes.x==2) color.a=i.diffuse.a;
 // ZiiNAN: Exact legacy factor/fade and stage-1 actor operations, before fog.
 if(AlphaModes.x==3) color.a*=TextureFactor.a;
 if(AlphaModes.x==4) color.a=TextureFactor.a;
 if(Modes.w==1) color.rgb=saturate(color.rgb+TextureFactor.rgb);
 if(Modes.w==2) color.rgb*=TextureFactor.rgb;
 if(Modes.w==3) color.rgb=saturate(color.rgb+color.a*CameraAlphaTexture.Sample(CameraAlphaSampler,i.cameraUV).rgb);
 if(AlphaModes.w!=0) {float mask=CameraAlphaTexture.Sample(CameraAlphaSampler,i.cameraUV).a;color.a=(VertexModes.w&1)!=0?color.a*mask:mask;}
 if(VertexModes.y!=0) {
#ifdef MODERN_VEGETATION
   float3 shadow=CameraAlphaTexture.Sample(CameraAlphaSampler,i.cameraUV).rgb;
   color.rgb*=(VertexModes.w&4)?shadow:LightingToLinear(shadow);
   indirect*=(VertexModes.w&4)?shadow:LightingToLinear(shadow);
#else
   color.rgb*=CameraAlphaTexture.Sample(CameraAlphaSampler,i.cameraUV).rgb;
#endif
 }
 // ZiiNAN: Native alpha test compares the 8-bit stage result, including filtered/factor alpha.
 float testedAlpha=floor(saturate(color.a)*255+0.5);
 if(AlphaModes.y==1 && testedAlpha<float(AlphaModes.z)) discard;
 if(AlphaModes.y==2 && testedAlpha<=float(AlphaModes.z)) discard;
#ifdef SHADOW_CASTER
 return;
#else
#ifdef AMBIENT_MRT
 float3 withoutAmbient=LightingToSRGB(max(0,color.rgb-indirect));
 withoutAmbient=lerp(FogColor.rgb,withoutAmbient,i.fog);
#endif
#ifdef MODERN_VEGETATION
 color.rgb=LightingToSRGB(color.rgb);
#endif
 color.rgb=lerp(FogColor.rgb,color.rgb,i.fog);
#ifdef AMBIENT_MRT
 ambientDelta=float4(max(0,LightingToLinear(color.rgb)-LightingToLinear(withoutAmbient)),color.a);
#endif
#ifdef MODERN_VEGETATION
 if(ShadowSettings.w!=0)color.rgb=lerp(color.rgb,CascadeColor(i.worldPosition),.5);
#endif
 return color;
#endif
}
Output AuxiliaryVS(float3 position:ATTRIB0,float3 normal:ATTRIB1,float2 uv:ATTRIB2,
                   float4 color:ATTRIB3,float2 uv1:ATTRIB4,float3 pivot:ATTRIB5,float flexibility:ATTRIB6,float3 pitchCos:ATTRIB7,float3 pitchSin:ATTRIB8) {
 float3 offset=position-pivot;
 float sway=sin(Wind.x*Wind.z+pivot.x*.013+pivot.y*.017)*Wind.y*flexibility;
#ifdef MODERN_VEGETATION
 float3x3 deform=float3x3(1,0,0,0,1,0,sway*CardRight.x,sway*CardRight.y,1);
 if(VertexModes.x!=0) {
   float3x3 rock=float3x3(cos(sway),0,sin(sway),0,1,0,-sin(sway),0,cos(sway));
   float3 pitch=VertexModes.x==1?CardPitch.x*pitchCos+CardPitch.y*pitchSin:float3(0,0,0);
   float3x3 tilt=float3x3(1,0,0,0,1,0,pitch.x,pitch.y,1+pitch.z);
   deform=mul(mul(rock,tilt),float3x3(CardRight.xyz,CardForward.xyz,CardUp.xyz));
 }
 float3 transformedNormal=LightingTransformNormal(normal,mul(deform,(float3x3)World));
#endif
 if(VertexModes.x!=0) {
   float2 rocked=float2(offset.x*cos(sway)-offset.z*sin(sway),offset.x*sin(sway)+offset.z*cos(sway));
   offset.x=rocked.x;offset.z=rocked.y;
   if(VertexModes.x==1)offset+=offset.z*(CardPitch.x*pitchCos+CardPitch.y*pitchSin);
   position=pivot+offset.x*CardRight.xyz+offset.y*CardForward.xyz+offset.z*CardUp.xyz;
 } else position.xy+=sway*position.z*CardRight.xy;
 Output o=VS(position,normal,uv);o.diffuse=color;
#ifdef MODERN_VEGETATION
 o.worldNormal=transformedNormal;
#endif
 if(VertexModes.y!=0)o.cameraUV=uv1;
 if(VertexModes.x==1&&AlphaModes.w!=0)o.cameraUV=float2(0,0);
 if(VertexModes.z!=0)o.fog=saturate((FogParameters.y-o.position.z)/(FogParameters.y-FogParameters.x));
 return o;
}
)";
}
struct DiligentStaticObjectRenderer::Impl
{
    DiligentD3D11Backend& backend;
    RefCntAutoPtr<IBuffer> constants;
    RefCntAutoPtr<IPipelineState> pipelines[60];
    RefCntAutoPtr<IPipelineState> pbrPipelines[48];
    RefCntAutoPtr<IPipelineState> shadowPipelines[9];
    RefCntAutoPtr<IShaderResourceBinding> shadowBindings[9];
    RefCntAutoPtr<IBuffer> shadowMaterial;
    RefCntAutoPtr<ISampler> shadowSampler;
    RefCntAutoPtr<IBuffer> pbrConstants;
    RefCntAutoPtr<ISampler> pbrSampler;
    std::unordered_map<const MaterialRuntime*,std::unique_ptr<PBRBinding>> pbrBindings;
    std::shared_ptr<Counters> counters=std::make_shared<Counters>();
    std::vector<std::weak_ptr<SkinMeshBuffers>> skinMeshes;
    std::vector<std::weak_ptr<SkinPoseBuffer>> skinPoses;
    uint32_t draws=0;
    bool failed=false;
    // Failure-only diagnostic: retain the original sticky failure state and draw behavior.
    void Fail(const char* reason, int line) noexcept
    {
        const bool first = !failed;
        failed = true;
        if (first) LogRendererFailure("DiligentStaticObjectRenderer.cpp", this, reason, line);
    }
    explicit Impl(DiligentD3D11Backend& b):backend(b) {}
    ~Impl(){for(const auto& pipeline:pbrPipelines)if(pipeline)--livePBRPipelines;
        for(unsigned i=36;i<60;++i)if(pipelines[i])--liveLightingPipelines;
        for(const auto& p:shadowPipelines)if(p)--liveShadowPipelines;if(shadowMaterial)--liveShadowBuffers;}
    bool InitializeShadows(bool gpu);
    void DrawShadow(const std::shared_ptr<Geometry>&,const std::shared_ptr<Texture>&,const StaticObjectDraw&,bool,bool,bool);
    bool InitializePBR(bool gpu);
    void DrawPBR(const std::shared_ptr<Geometry>&,const std::shared_ptr<Texture>&,
        const StaticObjectDraw&,bool skin,bool rigid,unsigned variant);
};
#include "DiligentPBRMaterial.inl"
#include "DiligentShadowCaster.inl"
DiligentStaticObjectRenderer::DiligentStaticObjectRenderer(DiligentD3D11Backend& b):m_impl(std::make_unique<Impl>(b)) {}
DiligentStaticObjectRenderer::~DiligentStaticObjectRenderer() = default;
void DiligentStaticObjectRenderer::ResetFrame() {
    m_impl->draws=0;
    std::erase_if(m_impl->pbrBindings,[](const auto& item){return item.second->material.expired();});
}
bool DiligentStaticObjectRenderer::Failed() const { return m_impl->failed; }
uint32_t DiligentStaticObjectRenderer::DrawCount() const { return m_impl->draws; }
uint32_t DiligentStaticObjectRenderer::LiveGeometryCount() const { return m_impl->counters->geometry; }
uint32_t DiligentStaticObjectRenderer::LiveTextureCount() const { return m_impl->counters->textures; }
bool DiligentStaticObjectRenderer::Initialize(bool gpuPrototype)
{
    auto& s=*m_impl;
    if(!s.backend.m_impl) return false;
    try {
        auto* device=s.backend.m_impl->device.RawPtr();
        BufferDesc buffer;
        buffer.Name="Static object original transforms/light"; buffer.Size=sizeof(Constants);
        buffer.Usage=USAGE_DYNAMIC; buffer.BindFlags=BIND_UNIFORM_BUFFER; buffer.CPUAccessFlags=CPU_ACCESS_WRITE;
        device->CreateBuffer(buffer,nullptr,&s.constants);
        if(!s.constants) return false;
        ShaderCreateInfo shader;
        shader.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL; shader.Source=shaderSource;
        shader.Desc.Name="Static rigid PNT VS"; shader.Desc.ShaderType=SHADER_TYPE_VERTEX; shader.EntryPoint="VS";
        RefCntAutoPtr<IShader> vs,ps;
        device->CreateShader(shader,&vs);
        shader.Desc.Name="Static diffuse PS"; shader.Desc.ShaderType=SHADER_TYPE_PIXEL; shader.EntryPoint="PS";
        device->CreateShader(shader,&ps);
        if(!vs || !ps) return false;
        // ZiiNAN: Diligent GPU skinning prototype
        RefCntAutoPtr<IShader> skinVS;
        const std::string skinSource=std::string(shaderSource)+gpuSkinningShader+R"(
Output SkinningVS(float3 position:ATTRIB0, float3 normal:ATTRIB1, float2 uv:ATTRIB2,
                  uint4 weights:ATTRIB3, uint4 indices:ATTRIB4) {
 float3 p,n; SkinVertex(position,normal,weights,indices,p,n); return VS(p,n,uv);
})";
        if(gpuPrototype) {
            shader.Source=skinSource.c_str(); shader.Desc.Name="B3 original PWNT skinning VS";
            shader.Desc.ShaderType=SHADER_TYPE_VERTEX; shader.EntryPoint="SkinningVS";
            device->CreateShader(shader,&skinVS);
            if(!skinVS) return false;
        }
        RefCntAutoPtr<IShader> auxiliaryVS;
        shader.Source=shaderSource;shader.Desc.Name="Mesh auxiliary colors and card pivots";
        shader.Desc.ShaderType=SHADER_TYPE_VERTEX;shader.EntryPoint="AuxiliaryVS";
        device->CreateShader(shader,&auxiliaryVS);if(!auxiliaryVS)return false;
        const std::string modernVegetationSource=std::string("#define MODERN_VEGETATION\n")+sceneLightingShader+shadowReceiverShader+shaderSource;
        RefCntAutoPtr<IShader> modernVegetationVS,modernVegetationPS;
        shader.Source=modernVegetationSource.c_str();shader.Desc.Name="G2 vegetation world normals";
        device->CreateShader(shader,&modernVegetationVS);
        shader.Desc.ShaderType=SHADER_TYPE_PIXEL;shader.EntryPoint="PS";shader.Desc.Name="G2 vegetation scene lighting";
        device->CreateShader(shader,&modernVegetationPS);if(!modernVegetationVS||!modernVegetationPS)return false;
        const std::string ambientSource=std::string("#define AMBIENT_MRT\n")+modernVegetationSource;
        RefCntAutoPtr<IShader> ambientPS;shader.Source=ambientSource.c_str();device->CreateShader(shader,&ambientPS);if(!ambientPS)return false;
        LayoutElement layout[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32}};
        LayoutElement skinLayout[]={{0,0,3,VT_FLOAT32,False,0,40},{1,0,3,VT_FLOAT32,False,20,40},
            {2,0,2,VT_FLOAT32,False,32,40},{3,0,4,VT_UINT8,False,12,40},{4,0,4,VT_UINT8,False,16,40}};
        LayoutElement auxiliaryLayout[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32},
            {3,1,4,VT_FLOAT32,False,0,64},{4,1,2,VT_FLOAT32,False,16,64},{5,1,3,VT_FLOAT32,False,24,64},{6,1,1,VT_FLOAT32,False,36,64},{7,1,3,VT_FLOAT32,False,40,64},{8,1,3,VT_FLOAT32,False,52,64}};
        ShaderResourceVariableDesc variables[]={{SHADER_TYPE_PIXEL,"DiffuseTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"ObjectSampler",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"CameraAlphaTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"CameraAlphaSampler",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_VERTEX,"SkinningPalette",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"SunShadowDepth",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        for(unsigned variant=0;variant<60;++variant) {
            const bool auxiliary=variant>=24;const bool skin=variant>=12&&!auxiliary;
            if(skin&&!gpuPrototype)continue;
            const auto cull=variant%3;
            GraphicsPipelineStateCreateInfo info;
            info.PSODesc.Name="Static object opaque diffuse"; info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            info.PSODesc.ResourceLayout.Variables=variables; info.PSODesc.ResourceLayout.NumVariables=variant>=36?6:(skin ? 5 : 4);
            auto& g=info.GraphicsPipeline;
            const auto& swap=s.backend.m_impl->swapChain->GetDesc();
            g.NumRenderTargets=1; g.RTVFormats[0]=swap.ColorBufferFormat; g.DSVFormat=swap.DepthBufferFormat;
            if(variant>=48){g.NumRenderTargets=2;g.RTVFormats[1]=TEX_FORMAT_RGBA8_UNORM;}
            g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            g.RasterizerDesc.CullMode=cull==0 ? CULL_MODE_NONE : CULL_MODE_BACK;
            g.RasterizerDesc.FrontCounterClockwise=variant>=36?cull!=2:cull==1;
            g.RasterizerDesc.DepthClipEnable=True;
            g.DepthStencilDesc.DepthEnable=True; g.DepthStencilDesc.DepthWriteEnable=variant%12<6;
            g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;
            auto& blend=g.BlendDesc.RenderTargets[0];
            blend.BlendEnable=(variant/3)%2!=0;
            blend.SrcBlend=blend.SrcBlendAlpha=BLEND_FACTOR_SRC_ALPHA;
            blend.DestBlend=blend.DestBlendAlpha=BLEND_FACTOR_INV_SRC_ALPHA;
            g.InputLayout.LayoutElements=auxiliary?auxiliaryLayout:(skin ? skinLayout : layout); g.InputLayout.NumElements=auxiliary?9:(skin ? 5 : 3);
            info.pVS=variant>=36?modernVegetationVS:(auxiliary?auxiliaryVS:(skin ? skinVS : vs)); info.pPS=variant>=48?ambientPS:variant>=36?modernVegetationPS:ps;
            auto& pipeline=s.pipelines[variant];
            device->CreateGraphicsPipelineState(info,&pipeline);
            if(!pipeline) return false;
            for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})
                if(auto* variable=pipeline->GetStaticVariableByName(stage,"ObjectConstants")) variable->Set(s.constants);
            if(variant>=36){++liveLightingPipelines;pipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL,"SceneLightingConstants")->Set(s.backend.m_impl->lightBuffer);}
            if(variant>=36)s.backend.m_impl->depthEffects.BindReceiver(pipeline);
        }
        return s.InitializePBR(gpuPrototype)&&s.InitializeShadows(gpuPrototype);
    } catch(...) { s.Fail("initialization exception", __LINE__); return false; }
}
StaticObjectGeometryPtr DiligentStaticObjectRenderer::UploadGeometry(const StaticObjectSource& data)
{ return CreateGeometry(data,false); }
// ZiiNAN: GPU skinning actor coverage
bool DiligentStaticObjectRenderer::PreparePrototype(StaticObjectGeometryPtr& geometry,
    const SkinningModelData& data, const std::vector<std::shared_ptr<const BoneRemap>>& remaps,
    const BonePalette& palette, const ActorModelSource* source)
{
    auto& s=*m_impl;
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !s.pipelines[12] ||
       (source && !source->indices32.empty()) ||
       !ValidPrototypeModel(data) || !ValidPrototypePalette(palette)) return false;
    auto mesh=std::dynamic_pointer_cast<Geometry>(geometry);
    if(mesh && (!mesh->skin || !mesh->pose || mesh->skin->meshes!=data.meshes || mesh->skin->remaps!=remaps ||
        mesh->pose->identity!=palette.identity || mesh->pose->skeleton!=palette.skeleton || mesh->counters!=s.counters)) mesh.reset();
    try {
        auto* device=s.backend.m_impl->device.RawPtr();
        if(!mesh) {
            AssetRuntime::AnimationStallAudit::WorkScope createAudit(AssetRuntime::AnimationStallAudit::Work::GPUCreate);
            std::erase_if(s.skinMeshes,[](const auto& entry){ return entry.expired(); });
            std::erase_if(s.skinPoses,[](const auto& entry){ return entry.expired(); });
            std::shared_ptr<SkinMeshBuffers> shared;
            for(const auto& weak:s.skinMeshes) if(auto candidate=weak.lock())
                if(candidate->meshes==data.meshes && candidate->remaps==remaps) { shared=std::move(candidate); break; }
            if(!shared) {
                std::vector<SkinningVertex> vertices; std::vector<uint16_t> indices;
                if(!BuildPrototypeVertices(data,remaps,palette,vertices,indices) || vertices.empty()) return false;
                if(source) {
                    if(vertices.size()!=source->deformVertexCount || source->vertexCount!=vertices.size()+source->rigidVertices.size()) return false;
                    indices=source->indices;
                    for(const auto& vertex:source->rigidVertices) for(float value:vertex) if(!std::isfinite(value)) return false;
                } else if(std::find(data.status.begin(),data.status.end(),SkinDataStatus::Rigid)!=data.status.end()) return false;
                if(indices.empty() || vertices.size()>std::numeric_limits<uint32_t>::max()/sizeof(SkinningVertex) ||
                    indices.size()>std::numeric_limits<uint32_t>::max()/sizeof(uint16_t)) return false;
                shared=std::make_shared<SkinMeshBuffers>();
                shared->deformCount=static_cast<uint32_t>(vertices.size());
                shared->vertexCount=source ? source->vertexCount : shared->deformCount;
                shared->meshes=data.meshes; shared->remaps=remaps;
                BufferDesc desc; desc.Name="Shared original PWNT"; desc.Size=vertices.size()*sizeof(SkinningVertex);
                desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_VERTEX_BUFFER;
                BufferData initial{vertices.data(),desc.Size}; device->CreateBuffer(desc,&initial,&shared->vertices);
                std::vector<AssetRuntime::MaterialVertex> materialVertices;
                if(source&&!source->materialVertices.empty())materialVertices=source->materialVertices;
                else {
                    materialVertices.resize(shared->vertexCount);
                    for(std::size_t i=0;i<vertices.size();++i)materialVertices[i].uv={vertices[i].uv[0],vertices[i].uv[1]};
                    if(source)for(std::size_t i=0;i<source->rigidVertices.size();++i)
                        materialVertices[vertices.size()+i].uv={source->rigidVertices[i][6],source->rigidVertices[i][7]};
                }
                if(materialVertices.size()!=shared->vertexCount)return false;
                desc.Name="Shared PBR tangent and UV stream";desc.Size=materialVertices.size()*sizeof(AssetRuntime::MaterialVertex);
                initial={materialVertices.data(),desc.Size};device->CreateBuffer(desc,&initial,&shared->materialVertices);
                if(!shared->materialVertices)return false;
                if(source && !source->rigidVertices.empty()) {
                    desc.Name="Shared rigid attachment PNT"; desc.Size=source->rigidVertices.size()*sizeof(StaticObjectVertex);
                    initial={source->rigidVertices.data(),desc.Size}; device->CreateBuffer(desc,&initial,&shared->rigidVertices);
                    if(!shared->rigidVertices) return false;
                }
                desc.Name="Shared original mesh-local indices"; desc.Size=indices.size()*sizeof(uint16_t); desc.BindFlags=BIND_INDEX_BUFFER;
                initial={indices.data(),desc.Size}; device->CreateBuffer(desc,&initial,&shared->indices);
                if(!shared->vertices || !shared->indices) return false;
                shared->validationIndices=std::move(indices); s.skinMeshes.emplace_back(shared);
            }
            std::shared_ptr<SkinPoseBuffer> pose;
            for(const auto& weak:s.skinPoses) if(auto candidate=weak.lock())
                if(candidate->identity==palette.identity && candidate->skeleton==palette.skeleton) { pose=std::move(candidate); break; }
            if(!pose) {
                pose=std::make_shared<SkinPoseBuffer>(); pose->identity=palette.identity; pose->skeleton=palette.skeleton;
                BufferDesc desc; desc.Name="Actor current composite palette"; desc.Size=gpuPrototypeBufferBones*sizeof(SkinningMatrix);
                desc.Usage=USAGE_DYNAMIC; desc.BindFlags=BIND_UNIFORM_BUFFER; desc.CPUAccessFlags=CPU_ACCESS_WRITE;
                device->CreateBuffer(desc,nullptr,&pose->buffer);
                if(!pose->buffer) return false;
                s.skinPoses.emplace_back(pose);
            }
            mesh=std::make_shared<Geometry>();
            mesh->skin=shared; ++livePrototypeGeometry;
            mesh->pose=pose; mesh->vertices=shared->vertices; mesh->indices=shared->indices;
            mesh->materialVertices=shared->materialVertices;
            mesh->vertexCount=shared->vertexCount; mesh->validationIndices=shared->validationIndices;
            mesh->counters=s.counters; ++s.counters->geometry;
        }
        if(mesh->pose->revision!=palette.revision || !mesh->pose->revision) {
            AssetRuntime::AnimationStallAudit::WorkScope uploadAudit(AssetRuntime::AnimationStallAudit::Work::GPUUpload);
            MapHelper<SkinningMatrix> mapped(s.backend.m_impl->context,mesh->pose->buffer,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!mapped) return false;
            memset(mapped,0,gpuPrototypeBufferBones*sizeof(SkinningMatrix));
            memcpy(mapped,palette.matrices.data(),palette.matrices.size()*sizeof(SkinningMatrix));
            mesh->pose->revision=palette.revision;
            prototypeBoneBytes+=gpuPrototypeBufferBones*sizeof(SkinningMatrix);
        }
        geometry=mesh;
        return true;
    } catch(...) { return false; }
}
// ZiiNAN: Reuse index validation/materials while keeping the static path unchanged.
StaticObjectGeometryPtr DiligentStaticObjectRenderer::UploadDynamicGeometry(const StaticObjectSource& data)
{ return CreateGeometry(data,true); }
StaticObjectGeometryPtr DiligentStaticObjectRenderer::CreateGeometry(const StaticObjectSource& data, bool dynamic)
{
    AssetRuntime::AnimationStallAudit::WorkScope createAudit(AssetRuntime::AnimationStallAudit::Work::GPUCreate);
    auto& s=*m_impl;
    const auto fail=[&](int line) -> StaticObjectGeometryPtr { s.Fail("geometry upload rejected source or GPU buffer", line); return {}; };
    const bool wide=!data.indices32.empty();
    const auto indexCount=wide ? data.indices32.size() : data.indices.size();
    const auto indexStride=wide ? sizeof(uint32_t) : sizeof(uint16_t);
    if(!s.backend.m_impl || data.vertices.empty() || !indexCount || (wide && !data.indices.empty()) ||
        data.vertices.size()>std::numeric_limits<uint32_t>::max()/32 ||
       indexCount>std::numeric_limits<uint32_t>::max()/indexStride) return fail(__LINE__);
    for(const auto& vertex:data.vertices) for(float v:vertex) if(!std::isfinite(v)) return fail(__LINE__);
    if(!data.vertexExtras.empty()) {
        if(dynamic||data.vertexExtras.size()!=data.vertices.size())return fail(__LINE__);
        for(const auto& e:data.vertexExtras){for(float f:e.color)if(!std::isfinite(f)||f<0||f>1)return fail(__LINE__);for(float f:e.uv1)if(!std::isfinite(f))return fail(__LINE__);for(float f:e.pivot)if(!std::isfinite(f))return fail(__LINE__);for(float f:e.cardPitchCos)if(!std::isfinite(f)||std::abs(f)>4)return fail(__LINE__);for(float f:e.cardPitchSin)if(!std::isfinite(f)||std::abs(f)>4)return fail(__LINE__);if(!std::isfinite(e.flexibility)||e.flexibility<0||e.flexibility>1)return fail(__LINE__);}
    }
    // Indices are mesh-local; exact base/range is validated at submission.
    for(auto index:data.indices) if(index>=data.vertices.size()) return fail(__LINE__);
    for(auto index:data.indices32) if(index>=data.vertices.size()) return fail(__LINE__);
    try {
        auto result=std::make_shared<Geometry>();
        BufferDesc desc;
        desc.Name="Original static PNT vertices"; desc.Size=data.vertices.size()*32;
        desc.Usage=dynamic ? USAGE_DYNAMIC : USAGE_IMMUTABLE; desc.BindFlags=BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags=dynamic ? CPU_ACCESS_WRITE : CPU_ACCESS_NONE;
        BufferData initial{data.vertices.data(),desc.Size};
        s.backend.m_impl->device->CreateBuffer(desc,dynamic ? nullptr : &initial,&result->vertices);
        auto materialVertices=data.materialVertices;
        if(materialVertices.empty()) {
            materialVertices.resize(data.vertices.size());
            for(std::size_t i=0;i<data.vertices.size();++i)materialVertices[i].uv={data.vertices[i][6],data.vertices[i][7]};
        }
        if(materialVertices.size()!=data.vertices.size())return fail(__LINE__);
        for(const auto& v:materialVertices){for(float f:v.tangent)if(!std::isfinite(f))return fail(__LINE__);for(float f:v.uv)if(!std::isfinite(f))return fail(__LINE__);}
        desc.Name="PBR tangent and UV stream";desc.Size=materialVertices.size()*sizeof(AssetRuntime::MaterialVertex);
        desc.Usage=USAGE_IMMUTABLE;desc.CPUAccessFlags=CPU_ACCESS_NONE;
        initial={materialVertices.data(),desc.Size};s.backend.m_impl->device->CreateBuffer(desc,&initial,&result->materialVertices);
        if(!result->materialVertices)return fail(__LINE__);
        if(!data.vertexExtras.empty()) {
            static_assert(sizeof(StaticObjectVertexExtras)==64);
            desc.Name="Shared mesh auxiliary vertex channels";desc.Size=data.vertexExtras.size()*sizeof(StaticObjectVertexExtras);
            initial={data.vertexExtras.data(),desc.Size};s.backend.m_impl->device->CreateBuffer(desc,&initial,&result->extras);if(!result->extras)return fail(__LINE__);
        }
        desc.Name=wide ? "Static uint32 indices" : "Original static uint16 indices";
        desc.Size=indexCount*indexStride; desc.BindFlags=BIND_INDEX_BUFFER;
        desc.Usage=USAGE_IMMUTABLE; desc.CPUAccessFlags=CPU_ACCESS_NONE;
        initial={wide ? static_cast<const void*>(data.indices32.data()) : static_cast<const void*>(data.indices.data()),desc.Size};
        s.backend.m_impl->device->CreateBuffer(desc,&initial,&result->indices);
        if(!result->vertices || !result->indices) return fail(__LINE__);
        result->vertexCount=static_cast<uint32_t>(data.vertices.size()); result->validationIndices=data.indices;
        if(!data.vertices.empty()){
            Graphics::Vector3 lo{data.vertices[0][0],data.vertices[0][1],data.vertices[0][2]},hi=lo;
            for(const auto& v:data.vertices)for(unsigned c=0;c<3;++c){lo[c]=std::min(lo[c],v[c]);hi[c]=std::max(hi[c],v[c]);}
            for(unsigned c=0;c<3;++c)result->boundsCenter[c]=(lo[c]+hi[c])*.5f;
            result->boundsRadius=std::hypot(hi[0]-lo[0],hi[1]-lo[1],hi[2]-lo[2])*.5f;
        }
        result->validationIndices32=data.indices32; result->indexType=wide ? VT_UINT32 : VT_UINT16;
        result->dynamic=dynamic;
        result->counters=s.counters; ++s.counters->geometry;
        return result;
    } catch(...) { return fail(__LINE__); }
}
// ZiiNAN: No skinning here; upload the already deformed position/normal/UV bytes.
bool DiligentStaticObjectRenderer::UpdateDynamicVertices(const StaticObjectGeometryPtr& geometry, const std::vector<StaticObjectVertex>& vertices)
{
    auto& s=*m_impl;
    auto mesh=std::dynamic_pointer_cast<Geometry>(geometry);
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !mesh || !mesh->dynamic ||
       mesh->counters!=s.counters || vertices.size()!=mesh->vertexCount) { s.Fail("dynamic mesh, owner, vertex count or frame mismatch", __LINE__); return false; }
    for(const auto& vertex:vertices) for(float value:vertex)
        if(!std::isfinite(value)) { s.Fail("non-finite dynamic vertex attribute", __LINE__); return false; }
    try {
        MapHelper<StaticObjectVertex> mapped(s.backend.m_impl->context,mesh->vertices,MAP_WRITE,MAP_FLAG_DISCARD);
        if(!mapped) { s.Fail("dynamic vertex buffer map failed", __LINE__); return false; }
        memcpy(mapped,vertices.data(),vertices.size()*sizeof(StaticObjectVertex));
        return true;
    } catch(...) { s.Fail("dynamic vertex upload exception", __LINE__); return false; }
}
TerrainTexturePtr DiligentStaticObjectRenderer::UploadTexture(const TerrainTextureData& data)
{
    auto& s=*m_impl;
    const auto fail=[&](int line) -> TerrainTexturePtr { s.Fail("texture upload rejected image, mip layout or GPU resource", line); return {}; };
    if(!s.backend.m_impl || !data.width || !data.height || data.width>8192 || data.height>8192 || data.mips.empty()) return fail(__LINE__);
    uint32_t maxMips=1;
    for(uint32_t dim=std::max(data.width,data.height);dim>1;dim>>=1) ++maxMips;
    if(data.mips.size()>maxMips) return fail(__LINE__);
    TEXTURE_FORMAT format=TEX_FORMAT_UNKNOWN; uint32_t block=0,pixelBytes=4;
    switch(data.format) {
    case TerrainTextureFormat::RGBA8: format=TEX_FORMAT_RGBA8_UNORM; break;
    case TerrainTextureFormat::BGRA8: format=TEX_FORMAT_BGRA8_UNORM; break;
    case TerrainTextureFormat::BGRX8: format=TEX_FORMAT_BGRX8_UNORM; break;
    case TerrainTextureFormat::BC1: format=TEX_FORMAT_BC1_UNORM; block=8; break;
    case TerrainTextureFormat::BC2: format=TEX_FORMAT_BC2_UNORM; block=16; break;
    case TerrainTextureFormat::BC3: format=TEX_FORMAT_BC3_UNORM; block=16; break;
    case TerrainTextureFormat::B5G5R5A1: format=TEX_FORMAT_B5G5R5A1_UNORM; pixelBytes=2; break;
    default: return fail(__LINE__);
    }
    try {
        std::vector<TextureSubResData> mips;
        uint32_t w=data.width,h=data.height;
        for(const auto& mip:data.mips) {
            const size_t row=block ? size_t((w+3)/4)*block : size_t(w)*pixelBytes;
            const size_t rows=block ? (h+3)/4 : h;
            if(!mip.data || mip.rowStride<row || mip.size<row || (rows-1)>(mip.size-row)/mip.rowStride) return fail(__LINE__);
            TextureSubResData sub; sub.pData=mip.data; sub.Stride=mip.rowStride; mips.push_back(sub);
            w=std::max(1u,w>>1); h=std::max(1u,h>>1);
        }
        auto result=std::make_shared<Texture>();
        TextureDesc desc;
        desc.Name="Original map object diffuse"; desc.Type=RESOURCE_DIM_TEX_2D;
        desc.Width=data.width; desc.Height=data.height; desc.MipLevels=static_cast<uint32_t>(mips.size());
        TEXTURE_FORMAT srgb=TEX_FORMAT_UNKNOWN,storage=format;
        switch(format) {
        case TEX_FORMAT_RGBA8_UNORM:storage=TEX_FORMAT_RGBA8_TYPELESS;srgb=TEX_FORMAT_RGBA8_UNORM_SRGB;break;
        case TEX_FORMAT_BGRA8_UNORM:storage=TEX_FORMAT_BGRA8_TYPELESS;srgb=TEX_FORMAT_BGRA8_UNORM_SRGB;break;
        case TEX_FORMAT_BGRX8_UNORM:storage=TEX_FORMAT_BGRX8_TYPELESS;srgb=TEX_FORMAT_BGRX8_UNORM_SRGB;break;
        case TEX_FORMAT_BC1_UNORM:storage=TEX_FORMAT_BC1_TYPELESS;srgb=TEX_FORMAT_BC1_UNORM_SRGB;break;
        case TEX_FORMAT_BC2_UNORM:storage=TEX_FORMAT_BC2_TYPELESS;srgb=TEX_FORMAT_BC2_UNORM_SRGB;break;
        case TEX_FORMAT_BC3_UNORM:storage=TEX_FORMAT_BC3_TYPELESS;srgb=TEX_FORMAT_BC3_UNORM_SRGB;break;
        default:break;
        }
        desc.Format=storage; desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_SHADER_RESOURCE;
        TextureData initial{mips.data(),desc.MipLevels};
        s.backend.m_impl->device->CreateTexture(desc,&initial,&result->texture);
        if(!result->texture) return fail(__LINE__);
        TextureViewDesc view;view.ViewType=TEXTURE_VIEW_SHADER_RESOURCE;view.Format=format;
        result->texture->CreateView(view,&result->linearView);
        if(!result->linearView)return fail(__LINE__);
        if(srgb!=TEX_FORMAT_UNKNOWN){view.Format=srgb;result->texture->CreateView(view,&result->srgbView);if(!result->srgbView)return fail(__LINE__);}
        result->counters=s.counters; ++s.counters->textures;
        return result;
    } catch(...) { return fail(__LINE__); }
}
void DiligentStaticObjectRenderer::Draw(const StaticObjectGeometryPtr& geometry,const TerrainTexturePtr& texture,const StaticObjectDraw& draw)
{
    auto& s=*m_impl;
    auto mesh=std::dynamic_pointer_cast<Geometry>(geometry);
    auto image=std::dynamic_pointer_cast<Texture>(texture);
    auto cameraImage=draw.vertexShadow ? std::dynamic_pointer_cast<Texture>(draw.vertexShadow) : draw.sphereMap ? std::dynamic_pointer_cast<Texture>(draw.sphereMap) :
        (draw.cameraAlpha ? std::dynamic_pointer_cast<Texture>(draw.cameraAlpha) : image);
    const auto cull=static_cast<uint32_t>(draw.cull);
    const auto materialVariant=cull+(draw.blend ? 3 : 0)+(draw.depthWrite ? 0 : 6);
    const bool rigid=mesh && mesh->skin && draw.baseVertex>=mesh->skin->deformCount;
    const bool skin=mesh && mesh->skin && !rigid;
    const bool auxiliary=mesh&&mesh->extras;
    const bool modernVegetation=auxiliary&&Graphics::GraphicsFeatures{GetGraphicsRuntimeConfig()}.UsePBR();
    const bool ambient=modernVegetation&&s.backend.m_impl&&s.backend.m_impl->depthEffects.ambientActive;
    const auto variant=materialVariant+(modernVegetation?(ambient?48:36):(auxiliary?24:(skin ? 12 : 0)));
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !mesh || !image || !cameraImage ||
       mesh->counters!=s.counters || image->counters!=s.counters || cameraImage->counters!=s.counters ||
       cull>=3 || !s.pipelines[variant] || draw.alphaReference>255 || static_cast<uint32_t>(draw.alphaTest)>2 ||
       static_cast<uint32_t>(draw.actorStage)>3 || (draw.cameraAlpha && draw.sphereMap) ||
       draw.cardMode>2 || (draw.vertexShadow&&(draw.cameraAlpha||draw.sphereMap)) ||
       (draw.actorStage==ActorMaterialStage::Specular && !draw.sphereMap) ||
       !draw.indexCount || draw.indexCount%3 || draw.firstIndex>mesh->IndexCount() ||
       draw.indexCount>mesh->IndexCount()-draw.firstIndex || !draw.vertexCount ||
       draw.baseVertex>mesh->vertexCount || draw.vertexCount>mesh->vertexCount-draw.baseVertex ||
       static_cast<uint32_t>(draw.fog)>3) {
        // Refine the existing failed guard only; successful draws do no diagnostic work.
        s.Fail(!s.backend.m_impl ? "missing mesh backend" : !s.backend.m_impl->inFrame ? "mesh submitted outside backend frame" :
            !mesh ? "missing mesh geometry" : !image ? "missing mesh diffuse texture" : !cameraImage ? "missing mesh camera texture" :
            mesh->counters!=s.counters ? "mesh geometry owner mismatch" : image->counters!=s.counters ? "mesh diffuse texture owner mismatch" :
            cameraImage->counters!=s.counters ? "mesh camera texture owner mismatch" : cull>=3 ? "invalid mesh cull mode" :
            !s.pipelines[variant] ? "missing mesh pipeline" : draw.alphaReference>255 ? "mesh alpha reference out of range" :
            static_cast<uint32_t>(draw.alphaTest)>2 ? "invalid mesh alpha test" : static_cast<uint32_t>(draw.actorStage)>3 ? "invalid actor material stage" :
            (draw.cameraAlpha && draw.sphereMap) ? "simultaneous actor camera alpha and sphere map" :
            (draw.actorStage==ActorMaterialStage::Specular && !draw.sphereMap) ? "specular actor without sphere map" :
            (!draw.indexCount || draw.indexCount%3) ? "invalid mesh triangle index count" :
            (draw.firstIndex>mesh->IndexCount() || draw.indexCount>mesh->IndexCount()-draw.firstIndex) ? "mesh index draw range exceeds geometry" :
            !draw.vertexCount ? "zero mesh draw vertices" :
            (draw.baseVertex>mesh->vertexCount || draw.vertexCount>mesh->vertexCount-draw.baseVertex) ? "mesh vertex draw range exceeds geometry" :
            "invalid mesh fog mode", __LINE__);
        return;
    }
    if((rigid && !mesh->skin->rigidVertices) ||
       (skin && draw.vertexCount>mesh->skin->deformCount-draw.baseVertex)) { s.Fail("rigid/deform vertex range or rigid buffer mismatch", __LINE__); return; }
    if(shadowPassIndex>=0){s.DrawShadow(mesh,image,draw,skin,rigid,auxiliary);return;}
    for(size_t i=draw.firstIndex;i<size_t(draw.firstIndex)+draw.indexCount;++i)
        if(mesh->IndexAt(i)>=draw.vertexCount) { s.Fail("mesh-local index exceeds draw vertex count", __LINE__); return; }
    if(draw.material && !auxiliary && Graphics::GraphicsFeatures{GetGraphicsRuntimeConfig()}.UsePBR()) {
        s.DrawPBR(mesh,cameraImage,draw,skin,rigid,materialVariant+(skin?12:0));return;
    }
    try {
        auto& b=*s.backend.m_impl;
        if(modernVegetation&&!b.SyncSceneLighting()){s.Fail("vegetation scene lighting upload",__LINE__);return;}
        if(!image->sampler || !(image->sampling==draw.sampling) || image->anisotropic!=draw.anisotropic || image->maxAnisotropy!=draw.maxAnisotropy) {
            image->sampler.Release();
            SamplerDesc sampler;
            sampler.MinFilter=draw.sampling.linearMin ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            sampler.MagFilter=draw.sampling.linearMag ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            sampler.MipFilter=draw.sampling.linearMip ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            if(draw.anisotropic) {
                sampler.MinFilter=sampler.MagFilter=sampler.MipFilter=FILTER_TYPE_ANISOTROPIC;
                sampler.MaxAnisotropy=draw.maxAnisotropy;
            }
            sampler.AddressU=draw.sampling.wrapU ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV=draw.sampling.wrapV ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
            if(!draw.sampling.useMips) sampler.MaxLOD=0;
            b.device->CreateSampler(sampler,&image->sampler);
            if(!image->sampler) { s.Fail("diffuse sampler creation failed", __LINE__); return; }
            image->sampling=draw.sampling;
            image->anisotropic=draw.anisotropic; image->maxAnisotropy=draw.maxAnisotropy;
            for(auto& binding:image->bindings) binding.Release();
        }
        if(!image->cameraSampler || image->cameraImage.lock()!=cameraImage ||
           !(image->cameraSampling==draw.cameraAlphaSampling) || image->cameraAnisotropic!=draw.cameraAlphaAnisotropic ||
           image->cameraMaxAnisotropy!=draw.cameraAlphaMaxAnisotropy) {
            SamplerDesc sampler;
            sampler.MinFilter=draw.cameraAlphaSampling.linearMin ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            sampler.MagFilter=draw.cameraAlphaSampling.linearMag ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            sampler.MipFilter=draw.cameraAlphaSampling.linearMip ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            if(draw.cameraAlphaAnisotropic) {
                sampler.MinFilter=sampler.MagFilter=sampler.MipFilter=FILTER_TYPE_ANISOTROPIC;
                sampler.MaxAnisotropy=draw.cameraAlphaMaxAnisotropy;
            }
            sampler.AddressU=draw.cameraAlphaSampling.wrapU ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV=draw.cameraAlphaSampling.wrapV ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
            if(!draw.cameraAlphaSampling.useMips) sampler.MaxLOD=0;
            image->cameraSampler.Release(); b.device->CreateSampler(sampler,&image->cameraSampler);
            if(!image->cameraSampler) { s.Fail("camera sampler creation failed", __LINE__); return; }
            image->cameraImage=cameraImage; image->cameraSampling=draw.cameraAlphaSampling;
            image->cameraAnisotropic=draw.cameraAlphaAnisotropic; image->cameraMaxAnisotropy=draw.cameraAlphaMaxAnisotropy;
            for(auto& binding:image->bindings) binding.Release();
        }
        // ZiiNAN: Diligent GPU skinning prototype
        RefCntAutoPtr<IShaderResourceBinding> skinBinding;
        auto& binding=skin ? skinBinding : image->bindings[materialVariant+(modernVegetation?(ambient?36:24):(auxiliary?12:0))];
        if(!binding) {
            s.pipelines[variant]->CreateShaderResourceBinding(&binding,true);
            if(!binding) { s.Fail("mesh shader resource binding creation failed", __LINE__); return; }
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"DiffuseTexture")->Set(modernVegetation&&image->srgbView?image->srgbView:image->linearView);
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"ObjectSampler")->Set(image->sampler);
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"CameraAlphaTexture")->Set(modernVegetation&&cameraImage->srgbView?cameraImage->srgbView:cameraImage->linearView);
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"CameraAlphaSampler")->Set(image->cameraSampler);
            if(skin) binding->GetVariableByName(SHADER_TYPE_VERTEX,"SkinningPalette")->Set(mesh->pose->buffer);
        }
        {
            MapHelper<Constants> mapped(b.context,s.constants,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!mapped) { s.Fail("mesh constants buffer map failed", __LINE__); return; }
            mapped->matrices=draw.matrices; mapped->normal=draw.normalTransform;
            const auto& extent=b.swapChain->GetDesc();
            const auto viewportWidth=draw.viewport[2] ? draw.viewport[2] : extent.Width;
            const auto viewportHeight=draw.viewport[3] ? draw.viewport[3] : extent.Height;
            for(size_t row=0;row<4;++row) {
                mapped->matrices.projection[row*4]+=draw.matrices.projection[row*4+3]/viewportWidth;
                mapped->matrices.projection[row*4+1]-=draw.matrices.projection[row*4+3]/viewportHeight;
            }
            mapped->ambient=draw.ambient; mapped->diffuse=draw.diffuse; mapped->direction=draw.lightDirection;
            mapped->fogColor=draw.fogColor; mapped->fogParameters=draw.fogParameters;
            mapped->modes={static_cast<uint32_t>(draw.fog),draw.rangeFog,draw.normalizeNormals,static_cast<uint32_t>(draw.actorStage)};
            mapped->textureFactor=draw.textureFactor;
            mapped->cameraAlphaTransform=draw.cameraAlphaTransform;
            mapped->pointPositionRange=draw.pointPositionRange; mapped->pointAttenuation=draw.pointAttenuation;
            mapped->pointAmbient=draw.pointAmbient; mapped->pointDiffuse=draw.pointDiffuse;
            mapped->spotPositionRange=draw.spotPositionRange; mapped->spotAttenuation=draw.spotAttenuation;
            mapped->spotAmbient=draw.spotAmbient; mapped->spotDiffuse=draw.spotDiffuse;
            mapped->spotDirection=draw.spotDirection; mapped->spotCone=draw.spotCone;
            mapped->cardRight=draw.cardRight;mapped->cardForward=draw.cardForward;mapped->cardUp=draw.cardUp;mapped->wind=draw.wind;
            mapped->cardPitch=draw.cardPitch;
            mapped->vertexModes={draw.cardMode,draw.vertexShadow?1u:0u,draw.cardFog?1u:0u,
                (draw.modulateCameraAlpha?1u:0u)|(modernVegetation&&image->srgbView?2u:0u)|(modernVegetation&&cameraImage->srgbView?4u:0u)};
            mapped->alphaModes={draw.factorAlphaOnly ? 4u : (draw.factorAlpha ? 3u : (draw.diffuseAlphaOnly ? 2u : uint32_t(draw.textureAlpha))),static_cast<uint32_t>(draw.alphaTest),draw.alphaReference,draw.cameraAlpha ? 1u : 0u};
        }
        b.depthEffects.BindTargets(b.swapChain,modernVegetation);
        if(modernVegetation)b.depthEffects.SetReceiver(binding);
        b.context->SetPipelineState(s.pipelines[variant]);
        const auto& extent=b.swapChain->GetDesc();
        Viewport viewport{float(draw.viewport[0]),float(draw.viewport[1]),float(draw.viewport[2] ? draw.viewport[2] : extent.Width),float(draw.viewport[3] ? draw.viewport[3] : extent.Height),0,1};
        b.context->SetViewports(1,&viewport,extent.Width,extent.Height);
        IBuffer* vertex=rigid ? mesh->skin->rigidVertices : mesh->vertices; Uint64 offset=0;
        if(auxiliary){IBuffer* buffers[]={vertex,mesh->extras};Uint64 offsets[]={0,0};b.context->SetVertexBuffers(0,2,buffers,offsets,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);}
        else b.context->SetVertexBuffers(0,1,&vertex,&offset,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        b.context->SetIndexBuffer(mesh->indices,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        b.context->CommitShaderResources(binding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs attributes{draw.indexCount,mesh->indexType,DRAW_FLAG_VERIFY_ALL};
        attributes.FirstIndexLocation=draw.firstIndex;
        attributes.BaseVertex=draw.baseVertex-(rigid ? mesh->skin->deformCount : 0);
        b.context->DrawIndexed(attributes); ++s.draws;
        if(modernVegetation)b.depthEffects.SetReceiver(binding,true);
        if(modernVegetation)++modernVegetationDraws;
    } catch(...) { s.Fail("mesh draw exception", __LINE__); }
}
void DiligentStaticObjectRenderer::ReleaseBindings()
{
    if(!m_impl->backend.m_impl) return;
    auto& b=*m_impl->backend.m_impl;
    b.context->InvalidateState();
    if(b.inFrame) {
        auto* target=b.swapChain->GetCurrentBackBufferRTV();
        b.context->SetRenderTargets(1,&target,b.swapChain->GetDepthBufferDSV(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
}
}
