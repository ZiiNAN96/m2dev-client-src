// ZiiNAN: Diligent SpeedTree rendering integration; translate only the native static-color/leaf-table pipeline.
#include "DiligentTreeRenderer.h"
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
#include <map>
#include <unordered_set>

namespace Renderer
{
using namespace Diligent;
namespace
{
struct Counts { uint32_t geometry=0,textures=0; };
struct Geometry final : TreeGeometry
{
    RefCntAutoPtr<IBuffer> vertices,indices;
    uint32_t vertexCount=0;
    std::vector<uint16_t> validationIndices;
    bool dynamic=false;
    std::shared_ptr<Counts> counts;
    ~Geometry() override { if(counts) --counts->geometry; }
};
struct Texture final : TerrainTexture
{
    RefCntAutoPtr<ITexture> image;
    std::shared_ptr<Counts> counts;
    ~Texture() override { if(counts) --counts->textures; }
};
struct Constants
{
    TerrainMatrices matrices;
    std::array<std::array<float,4>,96> native;
    std::array<float,16> textureTransform;
    std::array<float,4> fogColor,fogParameters;
    std::array<uint32_t,4> modes,alpha;
    std::array<float,4> pixelOffset;
};
static_assert(sizeof(Constants)%16==0);
constexpr char source[]=R"(
cbuffer TreeConstants {
 row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection;
 float4 Native[96]; row_major float4x4 TextureTransform;
 float4 FogColor; float4 FogParameters; uint4 Modes; uint4 Alpha; float4 PixelOffset;
};
Texture2D TreeTexture; SamplerState TreeSampler;
Texture2D SecondTexture; SamplerState SecondSampler;
struct Output { float4 position:SV_POSITION; float4 color:COLOR0; float2 uv:TEXCOORD0; float2 secondUV:TEXCOORD1; float fog:TEXCOORD2; };
Output VS(float3 position:ATTRIB0,float4 color:ATTRIB1,float2 uv:ATTRIB2,float2 shadowUV:ATTRIB3,float2 leaf:ATTRIB4) {
 Output o; float4 world=mul(float4(position,1),World);
 if(Modes.x==2) world=float4(position,1)+Native[(uint)leaf.x]*leaf.y+Native[52];
 float4 eye=mul(world,View); o.position=mul(eye,Projection);
 if(Modes.x==2) o.position=float4(dot(world,Native[0]),dot(world,Native[1]),dot(world,Native[2]),dot(world,Native[3]));
 o.color=color.bgra; o.uv=uv;
 o.secondUV=Modes.w!=0 ? mul(eye,TextureTransform).xy : shadowUV;
 // The original leaf VS1.1 exports oT0 only; fixed-function TCI is not applied to its missing oT1.
 if(Modes.x==2) o.secondUV=float2(0,0);
 float d=Alpha.z!=0 ? length(eye.xyz) : abs(eye.z); o.fog=1;
 if(Modes.y==1) o.fog=exp(-FogParameters.z*d);
 if(Modes.y==2) o.fog=exp(-pow(FogParameters.z*d,2));
 if(Modes.y==3) o.fog=(FogParameters.y-d)/(FogParameters.y-FogParameters.x);
 if(Modes.y==4) o.fog=(Native[85].y-o.position.z)*Native[85].z;
 o.fog=saturate(o.fog); o.position.xy+=PixelOffset.xy*o.position.w;
 return o;
}
float4 PS(Output i):SV_TARGET {
 float4 color=TreeTexture.Sample(TreeSampler,i.uv)*i.color;
 if(Modes.z==1) color.rgb*=SecondTexture.Sample(SecondSampler,i.secondUV).rgb;
 if(Modes.z==2) color.a*=SecondTexture.Sample(SecondSampler,i.secondUV).a;
 float testedAlpha=floor(saturate(color.a)*255+0.5);
 if(Alpha.x!=0 && testedAlpha<=float(Alpha.y)) discard;
 color.rgb=lerp(FogColor.rgb,color.rgb,i.fog); return color;
}
)";
uint32_t SamplerKey(const TreeSampler& s)
{
    const auto& p=s.sampling;
    return uint32_t(p.wrapU)|(uint32_t(p.wrapV)<<1)|(uint32_t(p.linearMin)<<2)|(uint32_t(p.linearMag)<<3)|
        (uint32_t(p.linearMip)<<4)|(uint32_t(p.useMips)<<5)|(uint32_t(s.anisotropic)<<6)|(s.maxAnisotropy<<7);
}
bool ValidVertices(const std::vector<TreeVertex>& vertices)
{
    for(const auto& v:vertices) {
        for(float f:v.position) if(!std::isfinite(f)) return false;
        for(float f:v.uv) if(!std::isfinite(f)) return false;
        for(float f:v.shadowUv) if(!std::isfinite(f)) return false;
        for(float f:v.leaf) if(!std::isfinite(f)) return false;
        if(v.leaf[0]<0 || v.leaf[0]>=96 || std::floor(v.leaf[0])!=v.leaf[0]) return false;
    }
    return true;
}
}
struct DiligentTreeRenderer::Impl
{
    DiligentD3D11Backend& backend;
    RefCntAutoPtr<IBuffer> constants;
    RefCntAutoPtr<IShader> vs,ps;
    struct Pipeline { RefCntAutoPtr<IPipelineState> state; RefCntAutoPtr<IShaderResourceBinding> bindings; };
    std::map<uint32_t,Pipeline> pipelines;
    std::map<uint32_t,RefCntAutoPtr<ISampler>> samplers;
    std::shared_ptr<Counts> counts=std::make_shared<Counts>();
    std::unordered_set<const void*> instances;
    std::array<uint32_t,4> draws{};
    uint64_t vertices=0,indices=0;
    bool failed=false;
    // Failure-only diagnostic: retain the original sticky failure state and draw behavior.
    void Fail(const char* reason, int line) noexcept
    {
        const bool first = !failed;
        failed = true;
        if (first) LogRendererFailure("DiligentTreeRenderer.cpp", this, reason, line);
    }
    explicit Impl(DiligentD3D11Backend& b):backend(b) {}
};
DiligentTreeRenderer::DiligentTreeRenderer(DiligentD3D11Backend& b):m_impl(std::make_unique<Impl>(b)) {}
DiligentTreeRenderer::~DiligentTreeRenderer()=default;
bool DiligentTreeRenderer::Failed() const { return m_impl->failed; }
void DiligentTreeRenderer::ReportFailure() { m_impl->Fail("bridge rejected tree source or legacy state", __LINE__); }
uint32_t DiligentTreeRenderer::VisibleInstances() const { return uint32_t(m_impl->instances.size()); }
uint32_t DiligentTreeRenderer::DrawCount(TreePart p) const { return m_impl->draws[uint32_t(p)]; }
uint64_t DiligentTreeRenderer::Vertices() const { return m_impl->vertices; }
uint64_t DiligentTreeRenderer::Indices() const { return m_impl->indices; }
uint32_t DiligentTreeRenderer::LiveGeometryCount() const { return m_impl->counts->geometry; }
uint32_t DiligentTreeRenderer::LiveTextureCount() const { return m_impl->counts->textures; }
void DiligentTreeRenderer::ResetFrame()
{
    auto& s=*m_impl; s.instances.clear(); s.draws.fill(0); s.vertices=s.indices=0;
    // Release last-frame SRB image references, not a global reset after each draw.
    for(auto& entry:s.pipelines) if(entry.second.bindings) for(auto name:{"TreeTexture","SecondTexture"})
        entry.second.bindings->GetVariableByName(SHADER_TYPE_PIXEL,name)->Set(nullptr);
}
bool DiligentTreeRenderer::Initialize()
{
    auto& s=*m_impl;
    if(!s.backend.m_impl) return false;
    try {
        auto* device=s.backend.m_impl->device.RawPtr();
        BufferDesc buffer; buffer.Name="Native SpeedTree transforms/leaf constants"; buffer.Size=sizeof(Constants);
        buffer.Usage=USAGE_DYNAMIC; buffer.BindFlags=BIND_UNIFORM_BUFFER; buffer.CPUAccessFlags=CPU_ACCESS_WRITE;
        device->CreateBuffer(buffer,nullptr,&s.constants);
        ShaderCreateInfo shader; shader.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL; shader.Source=source;
        shader.Desc.Name="Native SpeedTree static color and leaf placement"; shader.Desc.ShaderType=SHADER_TYPE_VERTEX; shader.EntryPoint="VS";
        device->CreateShader(shader,&s.vs);
        shader.Desc.Name="Native SpeedTree texture stages/alpha/fog"; shader.Desc.ShaderType=SHADER_TYPE_PIXEL; shader.EntryPoint="PS";
        device->CreateShader(shader,&s.ps);
        return s.constants && s.vs && s.ps;
    } catch(...) { s.Fail("initialization exception", __LINE__); return false; }
}
TreeGeometryPtr DiligentTreeRenderer::UploadGeometry(const TreeSource& data,bool dynamic)
{
    auto& s=*m_impl;
    const auto fail=[&](int line) -> TreeGeometryPtr { s.Fail("geometry upload rejected source or GPU buffer", line); return {}; };
    if(!s.backend.m_impl || data.vertices.empty() || data.vertices.size()>UINT32_MAX/sizeof(TreeVertex) ||
       data.indices.size()>UINT32_MAX/2 || !ValidVertices(data.vertices)) return fail(__LINE__);
    for(auto index:data.indices) if(index>=data.vertices.size()) return fail(__LINE__);
    try {
        auto mesh=std::make_shared<Geometry>();
        BufferDesc desc; desc.Name="Original SpeedTree vertices"; desc.Size=data.vertices.size()*sizeof(TreeVertex);
        desc.Usage=dynamic ? USAGE_DYNAMIC : USAGE_IMMUTABLE; desc.BindFlags=BIND_VERTEX_BUFFER;
        desc.CPUAccessFlags=dynamic ? CPU_ACCESS_WRITE : CPU_ACCESS_NONE;
        BufferData initial{data.vertices.data(),desc.Size};
        s.backend.m_impl->device->CreateBuffer(desc,dynamic ? nullptr : &initial,&mesh->vertices);
        if(!data.indices.empty()) {
            desc.Name="Original SpeedTree uint16 strips"; desc.Size=data.indices.size()*2;
            desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_INDEX_BUFFER; desc.CPUAccessFlags=CPU_ACCESS_NONE;
            initial={data.indices.data(),desc.Size}; s.backend.m_impl->device->CreateBuffer(desc,&initial,&mesh->indices);
            if(!mesh->indices) return fail(__LINE__);
        }
        if(!mesh->vertices) return fail(__LINE__);
        mesh->vertexCount=uint32_t(data.vertices.size()); mesh->validationIndices=data.indices; mesh->dynamic=dynamic;
        mesh->counts=s.counts; ++s.counts->geometry; return mesh;
    } catch(...) { return fail(__LINE__); }
}
bool DiligentTreeRenderer::UpdateVertices(const TreeGeometryPtr& geometry,const std::vector<TreeVertex>& vertices)
{
    auto& s=*m_impl; auto mesh=std::dynamic_pointer_cast<Geometry>(geometry);
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !mesh || !mesh->dynamic || mesh->counts!=s.counts ||
       vertices.size()!=mesh->vertexCount || !ValidVertices(vertices)) { s.Fail("tree dynamic geometry, owner, frame or vertex validation failed", __LINE__); return false; }
    try {
        MapHelper<TreeVertex> mapped(s.backend.m_impl->context,mesh->vertices,MAP_WRITE,MAP_FLAG_DISCARD);
        if(!mapped) { s.Fail("tree vertex buffer map failed", __LINE__); return false; }
        memcpy(mapped,vertices.data(),vertices.size()*sizeof(TreeVertex)); return true;
    } catch(...) { s.Fail("tree vertex upload exception", __LINE__); return false; }
}
TerrainTexturePtr DiligentTreeRenderer::UploadTexture(const TerrainTextureData& data)
{
    auto& s=*m_impl;
    const auto fail=[&](int line) -> TerrainTexturePtr { s.Fail("texture upload rejected image, mip layout or GPU resource", line); return {}; };
    if(!s.backend.m_impl || !data.width || !data.height || data.width>8192 || data.height>8192 || data.mips.empty()) return fail(__LINE__);
    uint32_t maximum=1; for(auto d=std::max(data.width,data.height);d>1;d>>=1) ++maximum;
    if(data.mips.size()>maximum) return fail(__LINE__);
    TEXTURE_FORMAT format=TEX_FORMAT_UNKNOWN; uint32_t block=0,bytes=4;
    switch(data.format) {
    case TerrainTextureFormat::RGBA8: format=TEX_FORMAT_RGBA8_UNORM; break;
    case TerrainTextureFormat::BGRA8: format=TEX_FORMAT_BGRA8_UNORM; break;
    case TerrainTextureFormat::BGRX8: format=TEX_FORMAT_BGRX8_UNORM; break;
    case TerrainTextureFormat::BC1: format=TEX_FORMAT_BC1_UNORM; block=8; break;
    case TerrainTextureFormat::BC2: format=TEX_FORMAT_BC2_UNORM; block=16; break;
    case TerrainTextureFormat::BC3: format=TEX_FORMAT_BC3_UNORM; block=16; break;
    case TerrainTextureFormat::B5G5R5A1: format=TEX_FORMAT_B5G5R5A1_UNORM; bytes=2; break;
    default: return fail(__LINE__);
    }
    try {
        std::vector<TextureSubResData> mips; auto w=data.width,h=data.height;
        for(const auto& mip:data.mips) {
            const size_t row=block ? size_t((w+3)/4)*block : size_t(w)*bytes, rows=block ? (h+3)/4 : h;
            if(!mip.data || mip.rowStride<row || mip.size<row || (rows-1)>(mip.size-row)/mip.rowStride) return fail(__LINE__);
            TextureSubResData sub; sub.pData=mip.data; sub.Stride=mip.rowStride; mips.push_back(sub);
            w=std::max(1u,w>>1); h=std::max(1u,h>>1);
        }
        auto image=std::make_shared<Texture>();
        TextureDesc desc; desc.Name="Original SpeedTree DDS"; desc.Type=RESOURCE_DIM_TEX_2D;
        desc.Width=data.width; desc.Height=data.height; desc.MipLevels=uint32_t(mips.size());
        desc.Format=format; desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_SHADER_RESOURCE;
        TextureData initial{mips.data(),desc.MipLevels}; s.backend.m_impl->device->CreateTexture(desc,&initial,&image->image);
        if(!image->image) return fail(__LINE__);
        image->counts=s.counts; ++s.counts->textures; return image;
    } catch(...) { return fail(__LINE__); }
}
void DiligentTreeRenderer::Draw(const void* instance,const TreeGeometryPtr& geometry,const TerrainTexturePtr& texture,
                               const TerrainTexturePtr& second,const TreeDraw& draw)
{
    auto& s=*m_impl; auto mesh=std::dynamic_pointer_cast<Geometry>(geometry);
    auto image=std::dynamic_pointer_cast<Texture>(texture), other=std::dynamic_pointer_cast<Texture>(second ? second : texture);
    if(!s.backend.m_impl || !s.backend.m_impl->inFrame || !instance || !mesh || !image || !other ||
       mesh->counts!=s.counts || image->counts!=s.counts || other->counts!=s.counts ||
       draw.cull>2 || draw.depthFunction<1 || draw.depthFunction>8 || draw.alphaReference>255 || draw.fog>4 ||
       draw.stage1>2 || (draw.stage1 && !second) || uint32_t(draw.part)>3 || !draw.count ||
       (draw.strip && draw.count<3) || (!draw.strip && draw.count%3)) { s.Fail("tree geometry, texture, owner or draw state validation failed", __LINE__); return; }
    const auto available=mesh->indices ? mesh->validationIndices.size() : mesh->vertexCount;
    if(draw.first>available || draw.count>available-draw.first) { s.Fail("tree draw range exceeds geometry", __LINE__); return; }
    for(const auto& sample:draw.samplers) if(sample.maxAnisotropy<1 || sample.maxAnisotropy>16) { s.Fail("tree sampler anisotropy out of range", __LINE__); return; }
    try {
        auto& b=*s.backend.m_impl;
        const uint32_t key=draw.cull|(uint32_t(draw.blend)<<2)|(uint32_t(draw.depthWrite)<<3)|
            (uint32_t(draw.depthTest)<<4)|(draw.depthFunction<<5)|(uint32_t(draw.strip)<<9);
        auto& pipeline=s.pipelines[key];
        if(!pipeline.state) {
            GraphicsPipelineStateCreateInfo info;
            info.PSODesc.Name="Native SpeedTree material state"; info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            ShaderResourceVariableDesc variables[]={{SHADER_TYPE_PIXEL,"TreeTexture",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_PIXEL,"TreeSampler",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_PIXEL,"SecondTexture",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
                {SHADER_TYPE_PIXEL,"SecondSampler",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
            info.PSODesc.ResourceLayout.Variables=variables; info.PSODesc.ResourceLayout.NumVariables=4;
            auto& g=info.GraphicsPipeline; const auto& swap=b.swapChain->GetDesc();
            g.NumRenderTargets=1; g.RTVFormats[0]=swap.ColorBufferFormat; g.DSVFormat=swap.DepthBufferFormat;
            g.PrimitiveTopology=draw.strip ? PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            g.RasterizerDesc.CullMode=draw.cull==0 ? CULL_MODE_NONE : CULL_MODE_BACK;
            g.RasterizerDesc.FrontCounterClockwise=draw.cull==1; g.RasterizerDesc.DepthClipEnable=True;
            g.DepthStencilDesc.DepthEnable=draw.depthTest; g.DepthStencilDesc.DepthWriteEnable=draw.depthWrite;
            g.DepthStencilDesc.DepthFunc=COMPARISON_FUNCTION(draw.depthFunction);
            auto& blend=g.BlendDesc.RenderTargets[0]; blend.BlendEnable=draw.blend;
            blend.SrcBlend=blend.SrcBlendAlpha=BLEND_FACTOR_SRC_ALPHA; blend.DestBlend=blend.DestBlendAlpha=BLEND_FACTOR_INV_SRC_ALPHA;
            LayoutElement layout[]={{0,0,3,VT_FLOAT32,False,0,40},{1,0,4,VT_UINT8,True,12,40},
                {2,0,2,VT_FLOAT32,False,16,40},{3,0,2,VT_FLOAT32,False,24,40},{4,0,2,VT_FLOAT32,False,32,40}};
            g.InputLayout.LayoutElements=layout; g.InputLayout.NumElements=5; info.pVS=s.vs; info.pPS=s.ps;
            b.device->CreateGraphicsPipelineState(info,&pipeline.state);
            if(!pipeline.state) { s.Fail("tree pipeline creation failed", __LINE__); return; }
            for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})
                if(auto* variable=pipeline.state->GetStaticVariableByName(stage,"TreeConstants")) variable->Set(s.constants);
            pipeline.state->CreateShaderResourceBinding(&pipeline.bindings,true);
            if(!pipeline.bindings) { s.Fail("tree shader resource binding creation failed", __LINE__); return; }
        }
        RefCntAutoPtr<ISampler> bound[2];
        for(unsigned i=0;i<2;++i) {
            const auto& sample=draw.samplers[i]; auto& sampler=s.samplers[SamplerKey(sample)];
            if(!sampler) {
                SamplerDesc desc;
                desc.MinFilter=sample.sampling.linearMin ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
                desc.MagFilter=sample.sampling.linearMag ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
                desc.MipFilter=sample.sampling.linearMip ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
                if(sample.anisotropic) { desc.MinFilter=desc.MagFilter=desc.MipFilter=FILTER_TYPE_ANISOTROPIC; desc.MaxAnisotropy=sample.maxAnisotropy; }
                desc.AddressU=sample.sampling.wrapU ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
                desc.AddressV=sample.sampling.wrapV ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
                if(!sample.sampling.useMips) desc.MaxLOD=0;
                b.device->CreateSampler(desc,&sampler);
                if(!sampler) { s.Fail("tree sampler creation failed", __LINE__); return; }
            }
            bound[i]=sampler;
        }
        // Every draw defines both SRVs, both samplers, the full PSO and all constants.
        auto& binding=pipeline.bindings;
        binding->GetVariableByName(SHADER_TYPE_PIXEL,"TreeTexture")->Set(image->image->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        binding->GetVariableByName(SHADER_TYPE_PIXEL,"SecondTexture")->Set(other->image->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        binding->GetVariableByName(SHADER_TYPE_PIXEL,"TreeSampler")->Set(bound[0]);
        binding->GetVariableByName(SHADER_TYPE_PIXEL,"SecondSampler")->Set(bound[1]);
        {
            MapHelper<Constants> mapped(b.context,s.constants,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!mapped) { s.Fail("tree constants buffer map failed", __LINE__); return; }
            mapped->matrices=draw.matrices; mapped->native=draw.legacyConstants; mapped->textureTransform=draw.textureTransform;
            mapped->fogColor=draw.fogColor; mapped->fogParameters=draw.fogParameters;
            mapped->modes={uint32_t(draw.part),draw.fog,draw.stage1,uint32_t(draw.cameraCoordinates)};
            mapped->alpha={uint32_t(draw.alphaTest),draw.alphaReference,uint32_t(draw.rangeFog),0};
            const auto& extent=b.swapChain->GetDesc(); mapped->pixelOffset={1.0f/extent.Width,-1.0f/extent.Height,0,0};
        }
        b.context->SetPipelineState(pipeline.state);
        IBuffer* vb=mesh->vertices; Uint64 offset=0;
        b.context->SetVertexBuffers(0,1,&vb,&offset,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        b.context->SetIndexBuffer(mesh->indices,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        b.context->CommitShaderResources(binding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        if(mesh->indices) {
            DrawIndexedAttribs attributes{draw.count,VT_UINT16,DRAW_FLAG_VERIFY_ALL}; attributes.FirstIndexLocation=draw.first;
            b.context->DrawIndexed(attributes); s.indices+=draw.count;
        } else { DrawAttribs attributes{draw.count,DRAW_FLAG_VERIFY_ALL}; attributes.StartVertexLocation=draw.first; b.context->Draw(attributes); }
        s.vertices+=mesh->indices ? mesh->vertexCount : draw.count;
        s.instances.insert(instance); ++s.draws[uint32_t(draw.part)];
    } catch(...) { s.Fail("tree draw exception", __LINE__); }
}
}
