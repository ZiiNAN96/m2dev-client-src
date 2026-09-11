#include "DiligentTerrainRenderer.h"
#include "DiligentD3D11BackendInternal.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Texture.h"
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
struct TextureCounters { uint32_t live = 0, uploads = 0; std::array<uint32_t,3> lastSize{}; };
struct TextureResource final : TerrainTexture
{
    RefCntAutoPtr<ITexture> texture;
    RefCntAutoPtr<IShaderResourceBinding> bindings[2];
    std::shared_ptr<TextureCounters> counters;
    ~TextureResource() override { if (counters) --counters->live; }
};
struct TerrainConstants { TerrainMatrices matrices; std::array<float,16> textureTransform{}; };
constexpr char vertexShader[] = R"(
cbuffer TerrainCamera {
    row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection;
    row_major float4x4 TextureTransform;
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
float4 main() : SV_TARGET { return float4(0.72, 0.82, 0.38, 1.0); }
)";
constexpr char texturePixelShader[] = R"(
Texture2D TerrainTexture;
SamplerState TerrainSampler;
float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return float4(TerrainTexture.Sample(TerrainSampler, uv).rgb, 1.0);
})";
}
struct DiligentTerrainRenderer::Impl
{
    DiligentD3D11Backend& backend;
    RefCntAutoPtr<IBuffer> camera;
    RefCntAutoPtr<IPipelineState> pipelines[2];
    RefCntAutoPtr<IPipelineState> texturedPipelines[2];
    RefCntAutoPtr<IShaderResourceBinding> bindings[2];
    std::atomic<bool> failed{false};
    bool hasTerrain = false;
    uint32_t draws = 0;
    uint32_t texturedDraws = 0;
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
            if (!textured)
            {
                pipeline->CreateShaderResourceBinding(&s.bindings[strip], true);
                if (!s.bindings[strip]) return false;
            }
        }
        return true;
    }
    catch (...) { s.failed = true; return false; }
}
void DiligentTerrainRenderer::ResetFrame() { m_impl->hasTerrain = false; m_impl->draws = m_impl->texturedDraws = 0; }
bool DiligentTerrainRenderer::HasTerrain() const { return m_impl->hasTerrain; }
bool DiligentTerrainRenderer::Failed() const { return m_impl->failed; }
uint32_t DiligentTerrainRenderer::DrawCount() const { return m_impl->draws; }
uint32_t DiligentTerrainRenderer::TexturedDrawCount() const { return m_impl->texturedDraws; }
uint32_t DiligentTerrainRenderer::LiveTextureCount() const { return m_impl->textureCounters->live; }
uint32_t DiligentTerrainRenderer::TextureUploadCount() const { return m_impl->textureCounters->uploads; }
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
            const size_t rowBytes = blockSize ? size_t((width+3)/4)*blockSize : size_t(width)*4;
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
        desc.Name="Metin2 single terrain texture";
        desc.Type=RESOURCE_DIM_TEX_2D; desc.Width=data.width; desc.Height=data.height;
        desc.MipLevels=static_cast<uint32_t>(mips.size()); desc.Format=format;
        desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_SHADER_RESOURCE;
        TextureData initial{mips.data(),static_cast<uint32_t>(mips.size())};
        s.backend.m_impl->device->CreateTexture(desc,&initial,&resource->texture);
        if (!resource->texture) return fail();
        auto* view=resource->texture->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        if (!view) return fail();
        for (int strip=0;strip<2;++strip)
        {
            s.texturedPipelines[strip]->CreateShaderResourceBinding(&resource->bindings[strip],true);
            if (!resource->bindings[strip]) return fail();
            auto* variable=resource->bindings[strip]->GetVariableByName(SHADER_TYPE_PIXEL,"TerrainTexture");
            if (!variable) return fail();
            variable->Set(view);
        }
        resource->counters=s.textureCounters;
        ++s.textureCounters->live; ++s.textureCounters->uploads;
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
        mapped->matrices = matrices;
        mapped->textureTransform = textureTransform ? *textureTransform : std::array<float,16>{};
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
}
