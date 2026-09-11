#include "DiligentTerrainRenderer.h"
#include "DiligentD3D11BackendInternal.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/Sampler.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include <cstring>
#include <algorithm>

namespace Renderer
{
using namespace Diligent;
namespace
{
struct GeometryBuffer final : TerrainBuffer
{
    RefCntAutoPtr<IBuffer> buffer;
    uint32_t count = 0;
    BIND_FLAGS bind = BIND_NONE;
};
struct TextureCounters { uint32_t live = 0, uploads = 0, alpha = 0, materials = 0; std::array<uint32_t,3> lastSize{}; };
struct TextureResource final : TerrainTexture
{
    RefCntAutoPtr<ITexture> texture;
    RefCntAutoPtr<IShaderResourceBinding> bindings[2];
    std::shared_ptr<TextureCounters> counters;
    bool isAlpha = false;
    ~TextureResource() override { if (counters) { --counters->live; if(isAlpha) --counters->alpha; } }
};
struct SplatMaterial final : TerrainSplatMaterial
{
    std::shared_ptr<TextureResource> color, alpha;
    RefCntAutoPtr<IShaderResourceBinding> bindings[4];
    RefCntAutoPtr<ISampler> colorSampler, alphaSampler;
    TerrainSampling colorSampling{}, alphaSampling{};
    std::shared_ptr<TextureCounters> counters;
    ~SplatMaterial() override { if(counters) --counters->materials; }
};
struct TerrainConstants
{
    TerrainMatrices matrices;
    std::array<float,16> textureTransform{};
    std::array<float,4> solidColor{0.72f,0.82f,0.38f,1};
};
struct SplatConstants
{
    TerrainMatrices matrices;
    std::array<float,16> colorTransform, alphaTransform;
    std::array<float,4> textureFactor, fogColor, fogParameters;
    std::array<uint32_t,4> modes;
};
static_assert(sizeof(SplatConstants)%16==0 && sizeof(TerrainSplatVertex)==36);
constexpr char vertexShader[] = R"(
cbuffer TerrainCamera {
    row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection;
    row_major float4x4 TextureTransform;
    float4 SolidColor;
};
struct VSOutput { float4 position : SV_POSITION; float2 uv : TEXCOORD0; };
VSOutput main(float3 position : ATTRIB0)
{
    VSOutput result;
    float4 world = mul(float4(position, 1.0), World);
    result.position = mul(mul(world, View), Projection);
    result.uv = mul(world, TextureTransform).xy;
    return result;
})";
constexpr char pixelShader[] = R"(
cbuffer TerrainCamera {
    row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection;
    row_major float4x4 TextureTransform; float4 SolidColor;
};
float4 main() : SV_TARGET { return SolidColor; }
)";
constexpr char texturePixelShader[] = R"(
Texture2D TerrainTexture;
SamplerState TerrainSampler;
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return float4(TerrainTexture.Sample(TerrainSampler, uv).rgb, 1.0);
})";
constexpr char splatShader[] = R"(
cbuffer TerrainSplat {
    row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection;
    row_major float4x4 ColorTransform; row_major float4x4 AlphaTransform;
    float4 TextureFactor; float4 FogColor; float4 FogParameters; uint4 Modes;
};
Texture2D ColorTexture; Texture2D AlphaTexture;
SamplerState ColorSampler; SamplerState AlphaSampler;
struct Output {
    float4 position : SV_POSITION; float2 colorUV : TEXCOORD0; float2 alphaUV : TEXCOORD1;
    float4 diffuse : COLOR0; noperspective float fog : TEXCOORD2;
};
Output vs_main(float3 position : ATTRIB0, float4 diffuse : ATTRIB1, float fog : ATTRIB2,
               float2 colorUV : ATTRIB3, float2 alphaUV : ATTRIB4)
{
    Output result;
    float4 camera = mul(mul(float4(position,1),World),View);
    result.position = mul(camera,Projection);
    result.colorUV = (Modes.x & 8) ? colorUV : mul(camera,ColorTransform).xy;
    result.alphaUV = (Modes.x & 8) ? alphaUV : mul(camera,AlphaTransform).xy;
    result.diffuse = diffuse;
    float d = FogParameters.w != 0 ? length(camera.xyz) : abs(camera.z);
    result.fog = 1;
    if (Modes.z == 1) result.fog = saturate(exp(-d*FogParameters.z));
    if (Modes.z == 2) result.fog = saturate(exp(-pow(d*FogParameters.z,2)));
    if (Modes.z == 3) result.fog = saturate((FogParameters.y-d)/(FogParameters.y-FogParameters.x));
    if (Modes.z == 4) result.fog = fog;
    return result;
}
float4 ps_main(Output input) : SV_TARGET
{
    float4 color = ColorTexture.Sample(ColorSampler,input.colorUV);
    float alpha = color.a;
    if (Modes.y == 1) alpha = AlphaTexture.Sample(AlphaSampler,input.alphaUV).r;
    if (Modes.y == 2) alpha = input.diffuse.a;
    if (Modes.y == 3) alpha *= input.diffuse.a;
    if (Modes.w != 0 && alpha <= float(Modes.w-1)/255.0) discard;
    if ((Modes.x & 7) == 1) color.rgb *= input.diffuse.rgb;
    if ((Modes.x & 7) == 2) color.rgb = lerp(TextureFactor.rgb,color.rgb,input.diffuse.a);
    color.rgb = lerp(FogColor.rgb,color.rgb,input.fog);
    return float4(color.rgb,alpha);
}
)";
}
struct DiligentTerrainRenderer::Impl
{
    DiligentD3D11Backend& backend;
    RefCntAutoPtr<IBuffer> camera;
    RefCntAutoPtr<IPipelineState> pipelines[2];
    RefCntAutoPtr<IPipelineState> texturedPipelines[2];
    RefCntAutoPtr<IShaderResourceBinding> bindings[2];
    RefCntAutoPtr<IPipelineState> splatPipelines[4]; // blend*2 + strip
    RefCntAutoPtr<IBuffer> splatConstants, whiteVertices, dynamicVertices;
    TerrainMatrices matrices{};
    bool vertexAttributes = false;
    std::atomic<bool> failed{false};
    bool hasTerrain = false;
    uint32_t draws = 0;
    uint32_t texturedDraws = 0;
    uint32_t splatDraws = 0;
    std::shared_ptr<TextureCounters> textureCounters = std::make_shared<TextureCounters>();
    explicit Impl(DiligentD3D11Backend& value) : backend(value) {}
};
DiligentTerrainRenderer::DiligentTerrainRenderer(DiligentD3D11Backend& backend) : m_impl(std::make_unique<Impl>(backend)) {}
DiligentTerrainRenderer::~DiligentTerrainRenderer() = default;
bool DiligentTerrainRenderer::Initialize()
{
    auto& s = *m_impl;
    if (!s.backend.m_impl)
        return false;
    try
    {
        auto* device = s.backend.m_impl->device.RawPtr();
        BufferDesc camera;
        camera.Name = "Metin2 terrain camera (row major)";
        camera.Size = sizeof(TerrainConstants);
        camera.Usage = USAGE_DYNAMIC;
        camera.BindFlags = BIND_UNIFORM_BUFFER;
        camera.CPUAccessFlags = CPU_ACCESS_WRITE;
        device->CreateBuffer(camera, nullptr, &s.camera);
        if (!s.camera) return false;
        ShaderCreateInfo shader;
        shader.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        shader.EntryPoint = "main";
        shader.Desc.Name = "Metin2 terrain VS";
        shader.Desc.ShaderType = SHADER_TYPE_VERTEX;
        shader.Source = vertexShader;
        RefCntAutoPtr<IShader> vs, ps, texturedPS;
        device->CreateShader(shader, &vs);
        shader.Desc.Name = "Metin2 terrain constant PS";
        shader.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shader.Source = pixelShader;
        device->CreateShader(shader, &ps);
        shader.Desc.Name = "Metin2 single terrain texture PS";
        shader.Source = texturePixelShader;
        device->CreateShader(shader, &texturedPS);
        if (!vs || !ps || !texturedPS) return false;
        // Consume the unchanged 24-byte source; normals stay in the buffer but are unused.
        LayoutElement layout{0, 0, 3, VT_FLOAT32, False, 0, 24};
        for (int textured = 0; textured < 2; ++textured)
        for (int strip = 0; strip < 2; ++strip)
        {
            GraphicsPipelineStateCreateInfo info;
            info.PSODesc.Name = strip ? "Terrain LOD0 strip" : "Terrain LOD1/2 list";
            info.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;
            auto& g = info.GraphicsPipeline;
            const auto& swap = s.backend.m_impl->swapChain->GetDesc();
            g.NumRenderTargets = 1;
            g.RTVFormats[0] = swap.ColorBufferFormat;
            g.DSVFormat = swap.DepthBufferFormat;
            g.PrimitiveTopology = strip ? PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            g.RasterizerDesc.CullMode = CULL_MODE_BACK;
            g.RasterizerDesc.FrontCounterClockwise = True; // D3D9 D3DCULL_CW.
            g.RasterizerDesc.DepthClipEnable = True;
            g.DepthStencilDesc.DepthEnable = True;
            g.DepthStencilDesc.DepthWriteEnable = True;
            g.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;
            g.InputLayout.LayoutElements = &layout;
            g.InputLayout.NumElements = 1;
            info.pVS = vs;
            info.pPS = textured ? texturedPS : ps;
            ShaderResourceVariableDesc textureVariable{SHADER_TYPE_PIXEL, "TerrainTexture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE};
            SamplerDesc sampler;
            sampler.MinFilter = sampler.MagFilter = sampler.MipFilter = FILTER_TYPE_LINEAR;
            sampler.AddressU = sampler.AddressV = sampler.AddressW = TEXTURE_ADDRESS_WRAP;
            sampler.MaxAnisotropy = 1;
            ImmutableSamplerDesc immutableSampler{SHADER_TYPE_PIXEL, "TerrainSampler", sampler};
            if (textured)
            {
                info.PSODesc.ResourceLayout.Variables = &textureVariable;
                info.PSODesc.ResourceLayout.NumVariables = 1;
                info.PSODesc.ResourceLayout.ImmutableSamplers = &immutableSampler;
                info.PSODesc.ResourceLayout.NumImmutableSamplers = 1;
            }
            auto& pipeline = textured ? s.texturedPipelines[strip] : s.pipelines[strip];
            device->CreateGraphicsPipelineState(info, &pipeline);
            if (!pipeline) return false;
            auto* variable = pipeline->GetStaticVariableByName(SHADER_TYPE_VERTEX, "TerrainCamera");
            if (!variable) return false;
            variable->Set(s.camera);
            if (auto* pixelCamera=pipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL,"TerrainCamera"))
                pixelCamera->Set(s.camera);
            if (!textured)
            {
                pipeline->CreateShaderResourceBinding(&s.bindings[strip], true);
                if (!s.bindings[strip]) return false;
            }
        }
        BufferDesc constants;
        constants.Name="Metin2 original splat constants";
        constants.Size=sizeof(SplatConstants); constants.Usage=USAGE_DYNAMIC;
        constants.BindFlags=BIND_UNIFORM_BUFFER; constants.CPUAccessFlags=CPU_ACCESS_WRITE;
        device->CreateBuffer(constants,nullptr,&s.splatConstants);
        BufferDesc attributes;
        attributes.Name="Metin2 existing STP attributes"; attributes.Size=289*sizeof(TerrainSplatVertex);
        attributes.Usage=USAGE_DYNAMIC; attributes.BindFlags=BIND_VERTEX_BUFFER; attributes.CPUAccessFlags=CPU_ACCESS_WRITE;
        device->CreateBuffer(attributes,nullptr,&s.dynamicVertices);
        std::array<TerrainSplatVertex,289> white{};
        attributes.Name="Metin2 HTP default diffuse"; attributes.Usage=USAGE_IMMUTABLE; attributes.CPUAccessFlags=CPU_ACCESS_NONE;
        BufferData whiteData{white.data(),sizeof(white)};
        device->CreateBuffer(attributes,&whiteData,&s.whiteVertices);
        if(!s.splatConstants || !s.dynamicVertices || !s.whiteVertices) return false;
        shader.Source=splatShader; shader.EntryPoint="vs_main"; shader.Desc.ShaderType=SHADER_TYPE_VERTEX;
        shader.Desc.Name="Metin2 original splat VS";
        RefCntAutoPtr<IShader> splatVS,splatPS;
        device->CreateShader(shader,&splatVS);
        shader.EntryPoint="ps_main"; shader.Desc.ShaderType=SHADER_TYPE_PIXEL; shader.Desc.Name="Metin2 original splat PS";
        device->CreateShader(shader,&splatPS);
        if(!splatVS || !splatPS) return false;
        const LayoutElement splatLayout[]{
            {0,0,3,VT_FLOAT32,False,0,24}, {1,1,4,VT_FLOAT32,False,0,36},
            {2,1,1,VT_FLOAT32,False,16,36}, {3,1,2,VT_FLOAT32,False,20,36}, {4,1,2,VT_FLOAT32,False,28,36}};
        const ShaderResourceVariableDesc resources[]{
            {SHADER_TYPE_PIXEL,"ColorTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"AlphaTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"ColorSampler",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
            {SHADER_TYPE_PIXEL,"AlphaSampler",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
        for(int blend=0;blend<2;++blend) for(int strip=0;strip<2;++strip)
        {
            GraphicsPipelineStateCreateInfo info;
            info.PSODesc.Name="Metin2 original splat pass"; info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            info.PSODesc.ResourceLayout.Variables=resources; info.PSODesc.ResourceLayout.NumVariables=4;
            auto& g=info.GraphicsPipeline;
            const auto& swap=s.backend.m_impl->swapChain->GetDesc();
            g.NumRenderTargets=1; g.RTVFormats[0]=swap.ColorBufferFormat; g.DSVFormat=swap.DepthBufferFormat;
            g.PrimitiveTopology=strip ? PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            g.RasterizerDesc.CullMode=CULL_MODE_BACK; g.RasterizerDesc.FrontCounterClockwise=True;
            g.DepthStencilDesc.DepthEnable=True; g.DepthStencilDesc.DepthWriteEnable=True;
            g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;
            auto& target=g.BlendDesc.RenderTargets[0]; target.BlendEnable=blend!=0;
            target.SrcBlend=target.SrcBlendAlpha=BLEND_FACTOR_SRC_ALPHA;
            target.DestBlend=target.DestBlendAlpha=BLEND_FACTOR_INV_SRC_ALPHA;
            target.BlendOp=target.BlendOpAlpha=BLEND_OPERATION_ADD;
            g.InputLayout.LayoutElements=splatLayout; g.InputLayout.NumElements=5;
            info.pVS=splatVS; info.pPS=splatPS;
            auto& pipeline=s.splatPipelines[blend*2+strip];
            device->CreateGraphicsPipelineState(info,&pipeline);
            if(!pipeline) return false;
            for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})
            {
                auto* variable=pipeline->GetStaticVariableByName(stage,"TerrainSplat");
                if(!variable) return false;
                variable->Set(s.splatConstants);
            }
        }
        return true;
    }
    catch (...) { s.failed = true; return false; }
}
void DiligentTerrainRenderer::ResetFrame() { m_impl->hasTerrain = false; m_impl->draws = m_impl->texturedDraws = m_impl->splatDraws = 0; }
bool DiligentTerrainRenderer::HasTerrain() const { return m_impl->hasTerrain; }
bool DiligentTerrainRenderer::Failed() const { return m_impl->failed; }
uint32_t DiligentTerrainRenderer::DrawCount() const { return m_impl->draws; }
uint32_t DiligentTerrainRenderer::TexturedDrawCount() const { return m_impl->texturedDraws; }
uint32_t DiligentTerrainRenderer::LiveTextureCount() const { return m_impl->textureCounters->live; }
uint32_t DiligentTerrainRenderer::TextureUploadCount() const { return m_impl->textureCounters->uploads; }
uint32_t DiligentTerrainRenderer::LiveAlphaCount() const { return m_impl->textureCounters->alpha; }
uint32_t DiligentTerrainRenderer::LiveMaterialCount() const { return m_impl->textureCounters->materials; }
uint32_t DiligentTerrainRenderer::SplatDrawCount() const { return m_impl->splatDraws; }
std::array<uint32_t,3> DiligentTerrainRenderer::LastTextureSize() const { return m_impl->textureCounters->lastSize; }

TerrainBufferPtr DiligentTerrainRenderer::UploadVertices(const void* data, uint32_t count, uint32_t stride)
{
    auto& s = *m_impl;
    if (!data || count != 289 || stride != 24 || !s.backend.m_impl) { s.failed = true; return {}; }
    try
    {
        auto result = std::make_shared<GeometryBuffer>();
        result->count = count;
        result->bind = BIND_VERTEX_BUFFER;
        BufferDesc desc;
        desc.Name = "Metin2 terrain patch";
        desc.Size = uint64_t(count) * stride;
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = result->bind;
        BufferData initial{data, desc.Size};
        s.backend.m_impl->device->CreateBuffer(desc, &initial, &result->buffer);
        if (result->buffer) return result;
    }
    catch (...) {}
    s.failed = true;
    return {};
}
TerrainBufferPtr DiligentTerrainRenderer::UploadIndices(const uint16_t* data, uint32_t count)
{
    auto& s = *m_impl;
    if (!data || !count || !s.backend.m_impl) { s.failed = true; return {}; }
    for (uint32_t i = 0; i < count; ++i)
        if (data[i] >= 289) { s.failed = true; return {}; }
    try
    {
        auto result = std::make_shared<GeometryBuffer>();
        result->count = count;
        result->bind = BIND_INDEX_BUFFER;
        BufferDesc desc;
        desc.Name = "Metin2 terrain shared LOD indices";
        desc.Size = uint64_t(count) * sizeof(uint16_t);
        desc.Usage = USAGE_IMMUTABLE;
        desc.BindFlags = result->bind;
        BufferData initial{data, desc.Size};
        s.backend.m_impl->device->CreateBuffer(desc, &initial, &result->buffer);
        if (result->buffer) return result;
    }
    catch (...) {}
    s.failed = true;
    return {};
}
TerrainTexturePtr DiligentTerrainRenderer::UploadTexture(const TerrainTextureData& data)
{
    auto& s = *m_impl;
    const auto fail = [&]() -> TerrainTexturePtr { s.failed = true; return {}; };
    if (!s.backend.m_impl || !s.texturedPipelines[0] || !s.texturedPipelines[1] ||
        !data.width || !data.height || data.width > 8192 || data.height > 8192 || data.mips.empty()) return fail();
    uint32_t maxMips = 1;
    for (uint32_t size=std::max(data.width,data.height); size>1; size>>=1) ++maxMips;
    if (data.mips.size() > maxMips) return fail();
    TEXTURE_FORMAT format = TEX_FORMAT_UNKNOWN;
    uint32_t blockSize = 0;
    switch (data.format)
    {
    case TerrainTextureFormat::Alpha8: format = TEX_FORMAT_R8_UNORM; break;
    case TerrainTextureFormat::RGBA8: format = TEX_FORMAT_RGBA8_UNORM; break;
    case TerrainTextureFormat::BGRA8: format = TEX_FORMAT_BGRA8_UNORM; break;
    case TerrainTextureFormat::BGRX8: format = TEX_FORMAT_BGRX8_UNORM; break;
    case TerrainTextureFormat::BC1: format = TEX_FORMAT_BC1_UNORM; blockSize=8; break;
    case TerrainTextureFormat::BC2: format = TEX_FORMAT_BC2_UNORM; blockSize=16; break;
    case TerrainTextureFormat::BC3: format = TEX_FORMAT_BC3_UNORM; blockSize=16; break;
    default: return fail();
    }
    try
    {
        std::vector<TextureSubResData> mips;
        uint32_t width=data.width, height=data.height;
        for (const auto& mip : data.mips)
        {
            const size_t rowBytes = blockSize ? size_t((width+3)/4)*blockSize : size_t(width)*(data.format==TerrainTextureFormat::Alpha8 ? 1 : 4);
            const size_t rows = blockSize ? (height+3)/4 : height;
            // Validate without overflowing user-supplied stride/size arithmetic.
            if (!mip.data || mip.rowStride < rowBytes || mip.size < rowBytes ||
                (rows-1) > (mip.size-rowBytes)/mip.rowStride) return fail();
            TextureSubResData subresource;
            subresource.pData = mip.data;
            subresource.Stride = mip.rowStride;
            mips.push_back(subresource);
            width=std::max(1u,width>>1); height=std::max(1u,height>>1);
        }
        auto resource=std::make_shared<TextureResource>();
        TextureDesc desc;
        desc.Name="Metin2 terrain texture";
        desc.Type=RESOURCE_DIM_TEX_2D; desc.Width=data.width; desc.Height=data.height;
        desc.MipLevels=static_cast<uint32_t>(mips.size()); desc.Format=format;
        desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_SHADER_RESOURCE;
        TextureData initial{mips.data(),static_cast<uint32_t>(mips.size())};
        s.backend.m_impl->device->CreateTexture(desc,&initial,&resource->texture);
        if (!resource->texture) return fail();
        auto* view=resource->texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        if (!view) return fail();
        resource->isAlpha=data.format==TerrainTextureFormat::Alpha8;
        for (int strip=0;!resource->isAlpha && strip<2;++strip)
        {
            s.texturedPipelines[strip]->CreateShaderResourceBinding(&resource->bindings[strip],true);
            if (!resource->bindings[strip]) return fail();
            auto* variable=resource->bindings[strip]->GetVariableByName(SHADER_TYPE_PIXEL,"TerrainTexture");
            if (!variable) return fail();
            variable->Set(view);
        }
        resource->counters=s.textureCounters;
        ++s.textureCounters->live; ++s.textureCounters->uploads;
        if(resource->isAlpha) ++s.textureCounters->alpha;
        s.textureCounters->lastSize={data.width,data.height,static_cast<uint32_t>(mips.size())};
        return resource;
    }
    catch (...) { return fail(); }
}
void DiligentTerrainRenderer::ReleaseTexture(TerrainTexturePtr& texture)
{
    if (!texture) return;
    auto& s=*m_impl;
    if (s.backend.m_impl)
    {
        auto& backend=*s.backend.m_impl;
        // D3D11 InvalidateState also unbinds committed SRVs. The context must not
        // keep a previous map alive after its texture/SRB owner has been released.
        backend.context->InvalidateState();
        if (backend.inFrame)
        {
            auto* target=backend.swapChain->GetCurrentBackBufferRTV();
            backend.context->SetRenderTargets(1,&target,backend.swapChain->GetDepthBufferDSV(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }
    texture.reset();
}
void DiligentTerrainRenderer::BeginTerrain(const TerrainMatrices& matrices, bool statesMatch,
                                          const std::array<float,16>* textureTransform)
{
    auto& s = *m_impl;
    if (!statesMatch || !s.backend.m_impl || !s.backend.m_impl->inFrame || !s.camera)
    { s.failed = true; return; }
    try
    {
        MapHelper<TerrainConstants> mapped(s.backend.m_impl->context, s.camera, MAP_WRITE, MAP_FLAG_DISCARD);
        if (!mapped) { s.failed = true; return; }
        s.matrices = matrices;
        // Preserve D3D9's integer pixel centers on the D3D11 half-integer raster.
        // Shift clip XY by (+1/width, -1/height)*clipW; UV/world/view data stay exact.
        const auto& extent=s.backend.m_impl->swapChain->GetDesc();
        for(size_t row=0;row<4;++row)
        {
            s.matrices.projection[row*4]+=matrices.projection[row*4+3]/extent.Width;
            s.matrices.projection[row*4+1]-=matrices.projection[row*4+3]/extent.Height;
        }
        mapped->matrices = s.matrices;
        mapped->textureTransform = textureTransform ? *textureTransform : std::array<float,16>{};
        mapped->solidColor = {0.72f,0.82f,0.38f,1};
        s.hasTerrain = true;
    }
    catch (...) { s.failed = true; }
}
void DiligentTerrainRenderer::DrawTerrain(const TerrainBufferPtr& vertices, const TerrainBufferPtr& indices,
                                         uint32_t count, bool strip, const TerrainTexturePtr& texture)
{
    auto& s = *m_impl;
    const auto vb = std::dynamic_pointer_cast<GeometryBuffer>(vertices);
    const auto ib = std::dynamic_pointer_cast<GeometryBuffer>(indices);
    const auto material = std::dynamic_pointer_cast<TextureResource>(texture);
    if (s.failed) return;
    if (!s.hasTerrain || !vb || !ib || vb->bind != BIND_VERTEX_BUFFER || ib->bind != BIND_INDEX_BUFFER ||
        count != ib->count || (strip ? count < 3 : count % 3 != 0) || (texture && !material))
    { s.failed = true; return; }
    try
    {
        auto* context = s.backend.m_impl->context.RawPtr();
        IBuffer* buffer = vb->buffer;
        Uint64 offset = 0;
        context->SetPipelineState(material ? s.texturedPipelines[strip] : s.pipelines[strip]);
        context->SetVertexBuffers(0, 1, &buffer, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        context->SetIndexBuffer(ib->buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->CommitShaderResources(material ? material->bindings[strip] : s.bindings[strip], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs draw{count, VT_UINT16, DRAW_FLAG_VERIFY_ALL};
        context->DrawIndexed(draw);
        ++s.draws;
        if (material) ++s.texturedDraws;
    }
    catch (...) { s.failed = true; }
}

TerrainSplatMaterialPtr DiligentTerrainRenderer::CreateSplatMaterial(const TerrainTexturePtr& color, const TerrainTexturePtr& alpha)
{
    auto& s=*m_impl;
    auto material=std::make_shared<SplatMaterial>();
    material->color=std::dynamic_pointer_cast<TextureResource>(color);
    material->alpha=std::dynamic_pointer_cast<TextureResource>(alpha);
    if(!material->color || !material->alpha || material->color->isAlpha || !material->alpha->isAlpha ||
       material->color->counters!=s.textureCounters || material->alpha->counters!=s.textureCounters)
    { s.failed=true; return {}; }
    try
    {
        for(int index=0;index<4;++index)
        {
            if(!s.splatPipelines[index]) { s.failed=true; return {}; }
            auto& binding=material->bindings[index];
            s.splatPipelines[index]->CreateShaderResourceBinding(&binding,true);
            if(!binding) { s.failed=true; return {}; }
            auto* colorVariable=binding->GetVariableByName(SHADER_TYPE_PIXEL,"ColorTexture");
            auto* alphaVariable=binding->GetVariableByName(SHADER_TYPE_PIXEL,"AlphaTexture");
            if(!colorVariable || !alphaVariable) { s.failed=true; return {}; }
            colorVariable->Set(material->color->texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
            alphaVariable->Set(material->alpha->texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        }
        material->counters=s.textureCounters;
        ++s.textureCounters->materials;
        return material;
    }
    catch(...) { s.failed=true; return {}; }
}

void DiligentTerrainRenderer::ReleaseSplatMaterial(TerrainSplatMaterialPtr& material)
{
    if(!material) return;
    if(m_impl->backend.m_impl)
    {
        auto& backend=*m_impl->backend.m_impl;
        backend.context->InvalidateState();
        if(backend.inFrame)
        {
            auto* target=backend.swapChain->GetCurrentBackBufferRTV();
            backend.context->SetRenderTargets(1,&target,backend.swapChain->GetDepthBufferDSV(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }
    }
    material.reset();
}

void DiligentTerrainRenderer::SetSplatVertices(const TerrainSplatVertex* vertices, uint32_t count)
{
    auto& s=*m_impl;
    s.vertexAttributes=vertices!=nullptr;
    if(!vertices && !count) return;
    if(!vertices || count!=289 || !s.backend.m_impl || !s.dynamicVertices) { s.failed=true; return; }
    try
    {
        MapHelper<TerrainSplatVertex> mapped(s.backend.m_impl->context,s.dynamicVertices,MAP_WRITE,MAP_FLAG_DISCARD);
        if(!mapped) { s.failed=true; return; }
        memcpy(mapped,vertices,count*sizeof(*vertices));
    }
    catch(...) { s.failed=true; }
}

void DiligentTerrainRenderer::DrawSplat(const TerrainBufferPtr& vertices, const TerrainBufferPtr& indices,
    uint32_t count, bool strip, const TerrainSplatMaterialPtr& handle, const TerrainSplatParameters& params)
{
    auto& s=*m_impl;
    if(s.failed) return;
    auto material=std::dynamic_pointer_cast<SplatMaterial>(handle);
    auto vb=std::dynamic_pointer_cast<GeometryBuffer>(vertices), ib=std::dynamic_pointer_cast<GeometryBuffer>(indices);
    if(!s.hasTerrain || !s.backend.m_impl || !s.backend.m_impl->inFrame || !material ||
       material->counters!=s.textureCounters || !vb || !ib || vb->bind!=BIND_VERTEX_BUFFER ||
       ib->bind!=BIND_INDEX_BUFFER || count!=ib->count || (strip ? count<3 : count%3!=0) ||
       params.alphaReference < -1 || params.alphaReference>255 || params.vertexUV!=s.vertexAttributes ||
       (params.fog==TerrainFog::Linear && params.fogStart>=params.fogEnd))
    { s.failed=true; return; }
    try
    {
        const auto setSampler=[&](const TerrainSampling& sampling, TerrainSampling& previous,
                                   RefCntAutoPtr<ISampler>& sampler, const char* name)
        {
            if(sampler && sampling==previous) return true;
            SamplerDesc desc;
            desc.MinFilter=sampling.linearMin ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            desc.MagFilter=sampling.linearMag ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            desc.MipFilter=sampling.linearMip ? FILTER_TYPE_LINEAR : FILTER_TYPE_POINT;
            desc.AddressU=sampling.wrapU ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
            desc.AddressV=sampling.wrapV ? TEXTURE_ADDRESS_WRAP : TEXTURE_ADDRESS_CLAMP;
            desc.AddressW=TEXTURE_ADDRESS_CLAMP; desc.MaxAnisotropy=1;
            if(!sampling.useMips) desc.MaxLOD=0;
            sampler.Release(); s.backend.m_impl->device->CreateSampler(desc,&sampler);
            if(!sampler) return false;
            for(auto& binding:material->bindings)
            {
                auto* variable=binding->GetVariableByName(SHADER_TYPE_PIXEL,name);
                if(!variable) return false;
                variable->Set(sampler);
            }
            previous=sampling;
            return true;
        };
        if(!setSampler(params.colorSampling,material->colorSampling,material->colorSampler,"ColorSampler") ||
           !setSampler(params.alphaSampling,material->alphaSampling,material->alphaSampler,"AlphaSampler"))
        { s.failed=true; return; }
        auto* context=s.backend.m_impl->context.RawPtr();
        {
            MapHelper<SplatConstants> constants(context,s.splatConstants,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!constants) { s.failed=true; return; }
            constants->matrices=s.matrices;
            constants->colorTransform=params.colorTransform; constants->alphaTransform=params.alphaTransform;
            constants->textureFactor=params.textureFactor; constants->fogColor=params.fogColor;
            constants->fogParameters={params.fogStart,params.fogEnd,params.fogDensity,params.rangeFog ? 1.0f : 0.0f};
            constants->modes={uint32_t(params.colorOp)|(params.vertexUV ? 8u : 0u),uint32_t(params.alphaOp),
                               uint32_t(params.fog),uint32_t(params.alphaReference+1)};
        }
        const int pipeline=(params.blend ? 2 : 0)+(strip ? 1 : 0);
        context->SetPipelineState(s.splatPipelines[pipeline]);
        IBuffer* buffers[]{vb->buffer,s.vertexAttributes ? s.dynamicVertices.RawPtr() : s.whiteVertices.RawPtr()};
        Uint64 offsets[]{0,0};
        context->SetVertexBuffers(0,2,buffers,offsets,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        context->SetIndexBuffer(ib->buffer,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->CommitShaderResources(material->bindings[pipeline],RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->DrawIndexed(DrawIndexedAttribs{count,VT_UINT16,DRAW_FLAG_VERIFY_ALL});
        ++s.draws; ++s.texturedDraws; ++s.splatDraws;
    }
    catch(...) { s.failed=true; }
}

void DiligentTerrainRenderer::DrawTerrainSolid(const TerrainBufferPtr& vertices, const TerrainBufferPtr& indices,
    uint32_t count, bool strip, const std::array<float,4>& color)
{
    auto& s=*m_impl;
    if(!s.hasTerrain || !s.backend.m_impl || !s.backend.m_impl->inFrame) { s.failed=true; return; }
    try
    {
        {
            MapHelper<TerrainConstants> constants(s.backend.m_impl->context,s.camera,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!constants) { s.failed=true; return; }
            constants->matrices=s.matrices; constants->textureTransform={}; constants->solidColor=color;
        }
        DrawTerrain(vertices,indices,count,strip);
    }
    catch(...) { s.failed=true; }
}
}
