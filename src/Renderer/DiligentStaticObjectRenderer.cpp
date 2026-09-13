#include "DiligentStaticObjectRenderer.h"
#include "DiligentD3D11BackendInternal.h"
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
struct Geometry final : StaticObjectGeometry
{
    RefCntAutoPtr<IBuffer> vertices, indices;
    std::vector<uint16_t> validationIndices;
    uint32_t vertexCount=0;
    bool dynamic=false; // ZiiNAN: Only actor VBs use discard updates.
    std::shared_ptr<Counters> counters;
    ~Geometry() override { if(counters) --counters->geometry; }
};
struct Texture final : TerrainTexture
{
    RefCntAutoPtr<ITexture> texture;
    RefCntAutoPtr<ISampler> sampler;
    RefCntAutoPtr<IShaderResourceBinding> bindings[12];
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
};
static_assert(sizeof(Constants)%16==0 && sizeof(StaticObjectVertex)==32);
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
};
Texture2D DiffuseTexture;
SamplerState ObjectSampler;
Texture2D CameraAlphaTexture;
SamplerState CameraAlphaSampler;
struct Output { float4 position:SV_POSITION; float2 uv:TEXCOORD0; float4 diffuse:COLOR0; float fog:TEXCOORD1; float2 cameraUV:TEXCOORD2; };
Output VS(float3 position:ATTRIB0, float3 normal:ATTRIB1, float2 uv:ATTRIB2) {
 Output o;
 float4 eye=mul(mul(float4(position,1),World),View);
 o.position=mul(eye,Projection); o.uv=uv;
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
float4 PS(Output i):SV_TARGET {
 float4 color=DiffuseTexture.Sample(ObjectSampler,i.uv);
 color.rgb*=i.diffuse.rgb;
 if(AlphaModes.x==0) color.a*=i.diffuse.a;
 if(AlphaModes.x==2) color.a=i.diffuse.a;
 // ZiiNAN: Exact legacy factor/fade and stage-1 actor operations, before fog.
 if(AlphaModes.x==3) color.a*=TextureFactor.a;
 if(AlphaModes.x==4) color.a=TextureFactor.a;
 if(Modes.w==1) color.rgb=saturate(color.rgb+TextureFactor.rgb);
 if(Modes.w==2) color.rgb*=TextureFactor.rgb;
 if(Modes.w==3) color.rgb=saturate(color.rgb+color.a*CameraAlphaTexture.Sample(CameraAlphaSampler,i.cameraUV).rgb);
 if(AlphaModes.w!=0) color.a=CameraAlphaTexture.Sample(CameraAlphaSampler,i.cameraUV).a;
 // ZiiNAN: Native alpha test compares the 8-bit stage result, including filtered/factor alpha.
 float testedAlpha=floor(saturate(color.a)*255+0.5);
 if(AlphaModes.y==1 && testedAlpha<float(AlphaModes.z)) discard;
 if(AlphaModes.y==2 && testedAlpha<=float(AlphaModes.z)) discard;
 color.rgb=lerp(FogColor.rgb,color.rgb,i.fog);
 return color;
}
)";
}
struct DiligentStaticObjectRenderer::Impl
{
    DiligentD3D11Backend& backend;
    RefCntAutoPtr<IBuffer> constants;
    RefCntAutoPtr<IPipelineState> pipelines[12];
    std::shared_ptr<Counters> counters=std::make_shared<Counters>();
    uint32_t draws=0;
    bool failed=false;
    explicit Impl(DiligentD3D11Backend& b):backend(b) {}
};
DiligentStaticObjectRenderer::DiligentStaticObjectRenderer(DiligentD3D11Backend& b):m_impl(std::make_unique<Impl>(b)) {}
DiligentStaticObjectRenderer::~DiligentStaticObjectRenderer() = default;
void DiligentStaticObjectRenderer::ResetFrame() { m_impl->draws=0; }
bool DiligentStaticObjectRenderer::Failed() const { return m_impl->failed; }
uint32_t DiligentStaticObjectRenderer::DrawCount() const { return m_impl->draws; }
uint32_t DiligentStaticObjectRenderer::LiveGeometryCount() const { return m_impl->counters->geometry; }
uint32_t DiligentStaticObjectRenderer::LiveTextureCount() const { return m_impl->counters->textures; }
bool DiligentStaticObjectRenderer::Initialize()
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
        LayoutElement layout[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32}};
        ShaderResourceVariableDesc variables[]={{SHADER_TYPE_PIXEL,"DiffuseTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"ObjectSampler",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"CameraAlphaTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"CameraAlphaSampler",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
        for(unsigned variant=0;variant<12;++variant) {
            const auto cull=variant%3;
            GraphicsPipelineStateCreateInfo info;
            info.PSODesc.Name="Static object opaque diffuse"; info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            info.PSODesc.ResourceLayout.Variables=variables; info.PSODesc.ResourceLayout.NumVariables=4;
            auto& g=info.GraphicsPipeline;
            const auto& swap=s.backend.m_impl->swapChain->GetDesc();
            g.NumRenderTargets=1; g.RTVFormats[0]=swap.ColorBufferFormat; g.DSVFormat=swap.DepthBufferFormat;
            g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            g.RasterizerDesc.CullMode=cull==0 ? CULL_MODE_NONE : CULL_MODE_BACK;
            g.RasterizerDesc.FrontCounterClockwise=cull==1;
            g.RasterizerDesc.DepthClipEnable=True;
            g.DepthStencilDesc.DepthEnable=True; g.DepthStencilDesc.DepthWriteEnable=variant<6;
            g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;
            auto& blend=g.BlendDesc.RenderTargets[0];
            blend.BlendEnable=(variant/3)%2!=0;
            blend.SrcBlend=blend.SrcBlendAlpha=BLEND_FACTOR_SRC_ALPHA;
            blend.DestBlend=blend.DestBlendAlpha=BLEND_FACTOR_INV_SRC_ALPHA;
            g.InputLayout.LayoutElements=layout; g.InputLayout.NumElements=3;
            info.pVS=vs; info.pPS=ps;
            auto& pipeline=s.pipelines[variant];
            device->CreateGraphicsPipelineState(info,&pipeline);
            if(!pipeline) return false;
            for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})
                if(auto* variable=pipeline->GetStaticVariableByName(stage,"ObjectConstants")) variable->Set(s.constants);
        }
        return true;
    } catch(...) { s.failed=true; return false; }
}
StaticObjectGeometryPtr DiligentStaticObjectRenderer::UploadGeometry(const StaticObjectSource& data)
{ return CreateGeometry(data,false); }
// ZiiNAN: Reuse index validation/materials while keeping the static path unchanged.
StaticObjectGeometryPtr DiligentStaticObjectRenderer::UploadDynamicGeometry(const StaticObjectSource& data)
{ return CreateGeometry(data,true); }
StaticObjectGeometryPtr DiligentStaticObjectRenderer::CreateGeometry(const StaticObjectSource& data, bool dynamic)
{
    auto& s=*m_impl;
    const auto fail=[&]() -> StaticObjectGeometryPtr { s.failed=true; return {}; };
    if(!s.backend.m_impl || data.vertices.empty() || data.indices.empty() ||
       data.vertices.size()>std::numeric_limits<uint32_t>::max()/32 ||
       data.indices.size()>std::numeric_limits<uint32_t>::max()/2) return fail();
    for(const auto& vertex:data.vertices) for(float v:vertex) if(!std::isfinite(v)) return fail();
    // Indices are mesh-local; exact base/range is validated at submission.
    for(auto index:data.indices) if(index>=data.vertices.size()) return fail();
    try {
        auto result=std::make_shared<Geometry>();
        BufferDesc desc;
        desc.Name="Original static PNT vertices"; desc.Size=data.vertices.size()*32;
        desc.Usage=dynamic ? USAGE_DYNAMIC : USAGE_IMMUTABLE; desc.BindFlags=BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags=dynamic ? CPU_ACCESS_WRITE : CPU_ACCESS_NONE;
        BufferData initial{data.vertices.data(),desc.Size};
        s.backend.m_impl->device->CreateBuffer(desc,dynamic ? nullptr : &initial,&result->vertices);
        desc.Name="Original static uint16 indices"; desc.Size=data.indices.size()*2; desc.BindFlags=BIND_INDEX_BUFFER;
        desc.Usage=USAGE_IMMUTABLE; desc.CPUAccessFlags=CPU_ACCESS_NONE;
        initial={data.indices.data(),desc.Size};
        s.backend.m_impl->device->CreateBuffer(desc,&initial,&result->indices);
        if(!result->vertices || !result->indices) return fail();
        result->vertexCount=static_cast<uint32_t>(data.vertices.size()); result->validationIndices=data.indices;
        result->dynamic=dynamic;
        result->counters=s.counters; ++s.counters->geometry;
        return result;
    } catch(...) { return fail(); }
}
// ZiiNAN: No skinning here; upload the already deformed position/normal/UV bytes.
bool DiligentStaticObjectRenderer::UpdateDynamicVertices(const StaticObjectGeometryPtr& geometry, const std::vector<StaticObjectVertex>& vertices)
{
    auto& s=*m_impl;
    auto mesh=std::dynamic_pointer_cast<Geometry>(geometry);
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !mesh || !mesh->dynamic ||
       mesh->counters!=s.counters || vertices.size()!=mesh->vertexCount) { s.failed=true; return false; }
    for(const auto& vertex:vertices) for(float value:vertex)
        if(!std::isfinite(value)) { s.failed=true; return false; }
    try {
        MapHelper<StaticObjectVertex> mapped(s.backend.m_impl->context,mesh->vertices,MAP_WRITE,MAP_FLAG_DISCARD);
        if(!mapped) { s.failed=true; return false; }
        memcpy(mapped,vertices.data(),vertices.size()*sizeof(StaticObjectVertex));
        return true;
    } catch(...) { s.failed=true; return false; }
}
TerrainTexturePtr DiligentStaticObjectRenderer::UploadTexture(const TerrainTextureData& data)
{
    auto& s=*m_impl;
    const auto fail=[&]() -> TerrainTexturePtr { s.failed=true; return {}; };
    if(!s.backend.m_impl || !data.width || !data.height || data.width>8192 || data.height>8192 || data.mips.empty()) return fail();
    uint32_t maxMips=1;
    for(uint32_t dim=std::max(data.width,data.height);dim>1;dim>>=1) ++maxMips;
    if(data.mips.size()>maxMips) return fail();
    TEXTURE_FORMAT format=TEX_FORMAT_UNKNOWN; uint32_t block=0,pixelBytes=4;
    switch(data.format) {
    case TerrainTextureFormat::RGBA8: format=TEX_FORMAT_RGBA8_UNORM; break;
    case TerrainTextureFormat::BGRA8: format=TEX_FORMAT_BGRA8_UNORM; break;
    case TerrainTextureFormat::BGRX8: format=TEX_FORMAT_BGRX8_UNORM; break;
    case TerrainTextureFormat::BC1: format=TEX_FORMAT_BC1_UNORM; block=8; break;
    case TerrainTextureFormat::BC2: format=TEX_FORMAT_BC2_UNORM; block=16; break;
    case TerrainTextureFormat::BC3: format=TEX_FORMAT_BC3_UNORM; block=16; break;
    case TerrainTextureFormat::B5G5R5A1: format=TEX_FORMAT_B5G5R5A1_UNORM; pixelBytes=2; break;
    default: return fail();
    }
    try {
        std::vector<TextureSubResData> mips;
        uint32_t w=data.width,h=data.height;
        for(const auto& mip:data.mips) {
            const size_t row=block ? size_t((w+3)/4)*block : size_t(w)*pixelBytes;
            const size_t rows=block ? (h+3)/4 : h;
            if(!mip.data || mip.rowStride<row || mip.size<row || (rows-1)>(mip.size-row)/mip.rowStride) return fail();
            TextureSubResData sub; sub.pData=mip.data; sub.Stride=mip.rowStride; mips.push_back(sub);
            w=std::max(1u,w>>1); h=std::max(1u,h>>1);
        }
        auto result=std::make_shared<Texture>();
        TextureDesc desc;
        desc.Name="Original map object diffuse"; desc.Type=RESOURCE_DIM_TEX_2D;
        desc.Width=data.width; desc.Height=data.height; desc.MipLevels=static_cast<uint32_t>(mips.size());
        desc.Format=format; desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_SHADER_RESOURCE;
        TextureData initial{mips.data(),desc.MipLevels};
        s.backend.m_impl->device->CreateTexture(desc,&initial,&result->texture);
        if(!result->texture) return fail();
        result->counters=s.counters; ++s.counters->textures;
        return result;
    } catch(...) { return fail(); }
}
void DiligentStaticObjectRenderer::Draw(const StaticObjectGeometryPtr& geometry,const TerrainTexturePtr& texture,const StaticObjectDraw& draw)
{
    auto& s=*m_impl;
    auto mesh=std::dynamic_pointer_cast<Geometry>(geometry);
    auto image=std::dynamic_pointer_cast<Texture>(texture);
    auto cameraImage=draw.sphereMap ? std::dynamic_pointer_cast<Texture>(draw.sphereMap) :
        (draw.cameraAlpha ? std::dynamic_pointer_cast<Texture>(draw.cameraAlpha) : image);
    const auto cull=static_cast<uint32_t>(draw.cull);
    const auto variant=cull+(draw.blend ? 3 : 0)+(draw.depthWrite ? 0 : 6);
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !mesh || !image || !cameraImage ||
       mesh->counters!=s.counters || image->counters!=s.counters || cameraImage->counters!=s.counters ||
       cull>=3 || !s.pipelines[variant] || draw.alphaReference>255 || static_cast<uint32_t>(draw.alphaTest)>2 ||
       static_cast<uint32_t>(draw.actorStage)>3 || (draw.cameraAlpha && draw.sphereMap) ||
       (draw.actorStage==ActorMaterialStage::Specular && !draw.sphereMap) ||
       !draw.indexCount || draw.indexCount%3 || draw.firstIndex>mesh->validationIndices.size() ||
       draw.indexCount>mesh->validationIndices.size()-draw.firstIndex || !draw.vertexCount ||
       draw.baseVertex>mesh->vertexCount || draw.vertexCount>mesh->vertexCount-draw.baseVertex ||
       static_cast<uint32_t>(draw.fog)>3) { s.failed=true; return; }
    for(size_t i=draw.firstIndex;i<size_t(draw.firstIndex)+draw.indexCount;++i)
        if(mesh->validationIndices[i]>=draw.vertexCount) { s.failed=true; return; }
    try {
        auto& b=*s.backend.m_impl;
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
            if(!image->sampler) { s.failed=true; return; }
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
            if(!image->cameraSampler) { s.failed=true; return; }
            image->cameraImage=cameraImage; image->cameraSampling=draw.cameraAlphaSampling;
            image->cameraAnisotropic=draw.cameraAlphaAnisotropic; image->cameraMaxAnisotropy=draw.cameraAlphaMaxAnisotropy;
            for(auto& binding:image->bindings) binding.Release();
        }
        auto& binding=image->bindings[variant];
        if(!binding) {
            s.pipelines[variant]->CreateShaderResourceBinding(&binding,true);
            if(!binding) { s.failed=true; return; }
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"DiffuseTexture")->Set(image->texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"ObjectSampler")->Set(image->sampler);
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"CameraAlphaTexture")->Set(cameraImage->texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            binding->GetVariableByName(SHADER_TYPE_PIXEL,"CameraAlphaSampler")->Set(image->cameraSampler);
        }
        {
            MapHelper<Constants> mapped(b.context,s.constants,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!mapped) { s.failed=true; return; }
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
            mapped->alphaModes={draw.factorAlphaOnly ? 4u : (draw.factorAlpha ? 3u : (draw.diffuseAlphaOnly ? 2u : uint32_t(draw.textureAlpha))),static_cast<uint32_t>(draw.alphaTest),draw.alphaReference,draw.cameraAlpha ? 1u : 0u};
        }
        b.context->SetPipelineState(s.pipelines[variant]);
        const auto& extent=b.swapChain->GetDesc();
        Viewport viewport{float(draw.viewport[0]),float(draw.viewport[1]),float(draw.viewport[2] ? draw.viewport[2] : extent.Width),float(draw.viewport[3] ? draw.viewport[3] : extent.Height),0,1};
        b.context->SetViewports(1,&viewport,extent.Width,extent.Height);
        IBuffer* vertex=mesh->vertices; Uint64 offset=0;
        b.context->SetVertexBuffers(0,1,&vertex,&offset,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        b.context->SetIndexBuffer(mesh->indices,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        b.context->CommitShaderResources(binding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs attributes{draw.indexCount,VT_UINT16,DRAW_FLAG_VERIFY_ALL};
        attributes.FirstIndexLocation=draw.firstIndex; attributes.BaseVertex=draw.baseVertex;
        b.context->DrawIndexed(attributes); ++s.draws;
    } catch(...) { s.failed=true; }
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
