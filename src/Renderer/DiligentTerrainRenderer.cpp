#include "DiligentTerrainRenderer.h"
#include "DiligentD3D11BackendInternal.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include <cstring>

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
constexpr char vertexShader[] = R"(
cbuffer TerrainCamera { row_major float4x4 World; row_major float4x4 View; row_major float4x4 Projection; };
float4 main(float3 position : ATTRIB0) : SV_POSITION
{
    return mul(mul(mul(float4(position, 1.0), World), View), Projection);
})";
constexpr char pixelShader[] = R"(
float4 main() : SV_TARGET { return float4(0.72, 0.82, 0.38, 1.0); }
)";
}
struct DiligentTerrainRenderer::Impl
{
    DiligentD3D11Backend& backend;
    RefCntAutoPtr<IBuffer> camera;
    RefCntAutoPtr<IPipelineState> pipelines[2];
    RefCntAutoPtr<IShaderResourceBinding> bindings[2];
    std::atomic<bool> failed{false};
    bool hasTerrain = false;
    uint32_t draws = 0;
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
        camera.Size = sizeof(TerrainMatrices);
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
        RefCntAutoPtr<IShader> vs, ps;
        device->CreateShader(shader, &vs);
        shader.Desc.Name = "Metin2 terrain constant PS";
        shader.Desc.ShaderType = SHADER_TYPE_PIXEL;
        shader.Source = pixelShader;
        device->CreateShader(shader, &ps);
        if (!vs || !ps) return false;
        // Consume the unchanged 24-byte source; normals stay in the buffer but are unused.
        LayoutElement layout{0, 0, 3, VT_FLOAT32, False, 0, 24};
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
            info.pPS = ps;
            device->CreateGraphicsPipelineState(info, &s.pipelines[strip]);
            if (!s.pipelines[strip]) return false;
            auto* variable = s.pipelines[strip]->GetStaticVariableByName(SHADER_TYPE_VERTEX, "TerrainCamera");
            if (!variable) return false;
            variable->Set(s.camera);
            s.pipelines[strip]->CreateShaderResourceBinding(&s.bindings[strip], true);
            if (!s.bindings[strip]) return false;
        }
        return true;
    }
    catch (...) { s.failed = true; return false; }
}
void DiligentTerrainRenderer::ResetFrame() { m_impl->hasTerrain = false; m_impl->draws = 0; }
bool DiligentTerrainRenderer::HasTerrain() const { return m_impl->hasTerrain; }
bool DiligentTerrainRenderer::Failed() const { return m_impl->failed; }
uint32_t DiligentTerrainRenderer::DrawCount() const { return m_impl->draws; }

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
void DiligentTerrainRenderer::BeginTerrain(const TerrainMatrices& matrices, bool statesMatch)
{
    auto& s = *m_impl;
    if (!statesMatch || !s.backend.m_impl || !s.backend.m_impl->inFrame || !s.camera)
    { s.failed = true; return; }
    try
    {
        MapHelper<TerrainMatrices> mapped(s.backend.m_impl->context, s.camera, MAP_WRITE, MAP_FLAG_DISCARD);
        if (!mapped) { s.failed = true; return; }
        *mapped = matrices;
        s.hasTerrain = true;
    }
    catch (...) { s.failed = true; }
}
void DiligentTerrainRenderer::DrawTerrain(const TerrainBufferPtr& vertices, const TerrainBufferPtr& indices,
                                         uint32_t count, bool strip)
{
    auto& s = *m_impl;
    const auto vb = std::dynamic_pointer_cast<GeometryBuffer>(vertices);
    const auto ib = std::dynamic_pointer_cast<GeometryBuffer>(indices);
    if (s.failed) return;
    if (!s.hasTerrain || !vb || !ib || vb->bind != BIND_VERTEX_BUFFER || ib->bind != BIND_INDEX_BUFFER ||
        count != ib->count || (strip ? count < 3 : count % 3 != 0))
    { s.failed = true; return; }
    try
    {
        auto* context = s.backend.m_impl->context.RawPtr();
        IBuffer* buffer = vb->buffer;
        Uint64 offset = 0;
        context->SetPipelineState(s.pipelines[strip]);
        context->SetVertexBuffers(0, 1, &buffer, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        context->SetIndexBuffer(ib->buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->CommitShaderResources(s.bindings[strip], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs draw{count, VT_UINT16, DRAW_FLAG_VERIFY_ALL};
        context->DrawIndexed(draw);
        ++s.draws;
    }
    catch (...) { s.failed = true; }
}
}
