#pragma once
#include "Renderer/GpuSkinningShader.h"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/ShaderResourceBinding.h"
#include "Graphics/GraphicsEngine/interface/Query.h"
#include <d3d11.h>
#include "Graphics/GraphicsEngineD3D11/interface/RenderDeviceD3D11.h"
#include <d3d11sdklayers.h>
#include <wrl/client.h>

namespace Renderer
{
class BackendTestAccess
{
public:
    // ZiiNAN: GPU skinning parity validation
    static void ValidateUploadedPalette(DiligentD3D11Backend& backend, const BonePalette& palette)
    {
        using namespace Diligent;
        RefCntAutoPtr<IRenderDeviceD3D11> native(backend.m_impl->device,IID_RenderDeviceD3D11);
        auto* device=native->GetD3D11Device();
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;device->GetImmediateContext(&context);
        Microsoft::WRL::ComPtr<ID3D11Buffer> paletteBuffer;
        for(UINT slot=0;slot<D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT;++slot) {
            Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;context->VSGetConstantBuffers(slot,1,&buffer);
            if(buffer) { D3D11_BUFFER_DESC desc{};buffer->GetDesc(&desc);
                if(desc.ByteWidth==gpuPrototypeBufferBones*sizeof(SkinningMatrix)) {
                    Check(!paletteBuffer,"Exactly one bound vertex-shader bone palette");paletteBuffer=buffer;
                }
            }
        }
        Check(bool(paletteBuffer),"Production draw actually binds a bone constant buffer");
        D3D11_BUFFER_DESC desc{};paletteBuffer->GetDesc(&desc);
        desc.Usage=D3D11_USAGE_STAGING;desc.BindFlags=0;desc.CPUAccessFlags=D3D11_CPU_ACCESS_READ;desc.MiscFlags=0;
        Microsoft::WRL::ComPtr<ID3D11Buffer> staging;
        Check(SUCCEEDED(device->CreateBuffer(&desc,nullptr,&staging)),"Production palette staging buffer");
        context->CopyResource(staging.Get(),paletteBuffer.Get());D3D11_MAPPED_SUBRESOURCE mapped{};
        Check(SUCCEEDED(context->Map(staging.Get(),0,D3D11_MAP_READ,0,&mapped)),"Production palette readback");
        std::array<SkinningMatrix,gpuPrototypeBufferBones> actual{};
        memcpy(actual.data(),mapped.pData,sizeof(actual));context->Unmap(staging.Get(),0);
        std::array<SkinningMatrix,gpuPrototypeBufferBones> expected{};
        std::copy(palette.matrices.begin(),palette.matrices.end(),expected.begin());
        Check(actual==expected,"Uploaded production matrices/order and zero tail exactly match current native palette");
    }
    static Diligent::RefCntAutoPtr<Diligent::IQuery> BeginTiming(DiligentD3D11Backend& backend)
    {
        using namespace Diligent;
        RefCntAutoPtr<IQuery> result;
        if(backend.m_impl->device->GetDeviceInfo().Features.DurationQueries) {
            QueryDesc desc;desc.Name="B3 isolated actor draw duration";desc.Type=QUERY_TYPE_DURATION;
            backend.m_impl->device->CreateQuery(desc,&result);
            Check(bool(result),"GPU duration query"); backend.m_impl->context->BeginQuery(result);
        }
        return result;
    }
    static void EndTiming(DiligentD3D11Backend& backend,Diligent::IQuery* query)
    { if(query) backend.m_impl->context->EndQuery(query); }
    static void Idle(DiligentD3D11Backend& backend) { backend.m_impl->context->WaitForIdle(); }
    static void Validate(DiligentD3D11Backend& backend)
    {
        using namespace Diligent;
        RefCntAutoPtr<IRenderDeviceD3D11> native(backend.m_impl->device,IID_RenderDeviceD3D11);
        Check(SUCCEEDED(native->GetD3D11Device()->GetDeviceRemovedReason()),"Native D3D11 device healthy");
        Microsoft::WRL::ComPtr<ID3D11InfoQueue> queue;
        const auto status=native->GetD3D11Device()->QueryInterface(IID_PPV_ARGS(&queue));
#ifdef _DEBUG
        Check(SUCCEEDED(status) && queue,"Debug D3D11 validation layer active");
#endif
        size_t warnings=0;
        if(SUCCEEDED(status)) for(UINT64 i=0;i<queue->GetNumStoredMessages();++i) {
            SIZE_T bytes=0;queue->GetMessage(i,nullptr,&bytes);std::vector<char> memory(bytes);
            auto* message=reinterpret_cast<D3D11_MESSAGE*>(memory.data());queue->GetMessage(i,message,&bytes);
            if(message->Severity<=D3D11_MESSAGE_SEVERITY_WARNING) { ++warnings;std::cerr<<"D3D11: "<<message->pDescription<<'\n'; }
        }
        Check(warnings==0,"No native D3D11 warnings/errors");
        std::cout<<"D3D11 validation="<<(SUCCEEDED(status)?"enabled":"Release no debug layer")<<" warnings="<<warnings<<'\n';
    }
    // ZiiNAN: Diligent GPU skinning prototype
    static std::vector<std::array<float,4>> Skin(DiligentD3D11Backend& backend,
        const std::vector<SkinningVertex>& vertices, const BonePalette& palette)
    {
        using namespace Diligent;
        auto& s=*backend.m_impl;
        Check(ValidPrototypePalette(palette),"GPU numeric palette bounds");
        const std::string source=std::string(gpuSkinningShader)+R"(
ByteAddressBuffer InputVertices;
RWStructuredBuffer<float4> OutputVertices;
uint4 Bytes(uint value) { return uint4(value&255,(value>>8)&255,(value>>16)&255,value>>24); }
[numthreads(64,1,1)] void CS(uint3 id:SV_DispatchThreadID) {
 uint size; InputVertices.GetDimensions(size); uint offset=id.x*40;
 if(offset>=size) return;
 float3 p,n;
 SkinVertex(asfloat(InputVertices.Load3(offset)),asfloat(InputVertices.Load3(offset+20)),
     Bytes(InputVertices.Load(offset+12)),Bytes(InputVertices.Load(offset+16)),p,n);
 OutputVertices[id.x*2]=float4(p,1); OutputVertices[id.x*2+1]=float4(n,0);
})";
        RefCntAutoPtr<IShader> shader;
        ShaderCreateInfo ci; ci.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL; ci.Source=source.c_str();
        ci.EntryPoint="CS"; ci.Desc.Name="B3 exact skin function numeric probe"; ci.Desc.ShaderType=SHADER_TYPE_COMPUTE;
        s.device->CreateShader(ci,&shader); Check(bool(shader),"numeric GPU shader");
        ComputePipelineStateCreateInfo pi; pi.PSODesc.Name="B3 numeric probe";
        pi.PSODesc.PipelineType=PIPELINE_TYPE_COMPUTE; pi.pCS=shader;
        RefCntAutoPtr<IPipelineState> pso; s.device->CreateComputePipelineState(pi,&pso); Check(bool(pso),"numeric GPU PSO");
        RefCntAutoPtr<IBuffer> input,output,bones,staging;
        BufferDesc desc; desc.Name="B3 numeric original skin input"; desc.Size=vertices.size()*40;
        desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_SHADER_RESOURCE; desc.Mode=BUFFER_MODE_RAW;
        BufferData initial{vertices.data(),desc.Size}; s.device->CreateBuffer(desc,&initial,&input);
        std::array<SkinningMatrix,gpuPrototypeBufferBones> matrices{};
        std::copy(palette.matrices.begin(),palette.matrices.end(),matrices.begin());
        desc={}; desc.Name="B3 numeric palette"; desc.Size=sizeof(matrices); desc.Usage=USAGE_IMMUTABLE; desc.BindFlags=BIND_UNIFORM_BUFFER;
        initial={matrices.data(),desc.Size}; s.device->CreateBuffer(desc,&initial,&bones);
        desc={}; desc.Name="B3 numeric output"; desc.Size=vertices.size()*32; desc.Usage=USAGE_DEFAULT;
        desc.BindFlags=BIND_UNORDERED_ACCESS; desc.Mode=BUFFER_MODE_STRUCTURED; desc.ElementByteStride=16;
        s.device->CreateBuffer(desc,nullptr,&output);
        desc={}; desc.Name="B3 numeric readback"; desc.Size=vertices.size()*32; desc.Usage=USAGE_STAGING; desc.CPUAccessFlags=CPU_ACCESS_READ;
        s.device->CreateBuffer(desc,nullptr,&staging);
        Check(input && bones && output && staging,"numeric GPU buffers");
        pso->GetStaticVariableByName(SHADER_TYPE_COMPUTE,"InputVertices")->Set(input->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
        pso->GetStaticVariableByName(SHADER_TYPE_COMPUTE,"OutputVertices")->Set(output->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
        pso->GetStaticVariableByName(SHADER_TYPE_COMPUTE,"SkinningPalette")->Set(bones);
        RefCntAutoPtr<IShaderResourceBinding> binding; pso->CreateShaderResourceBinding(&binding,true);
        s.context->SetPipelineState(pso); s.context->CommitShaderResources(binding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DispatchComputeAttribs dispatch; dispatch.ThreadGroupCountX=static_cast<Uint32>((vertices.size()+63)/64);
        s.context->DispatchCompute(dispatch);
        s.context->CopyBuffer(output,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,staging,0,desc.Size,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        s.context->WaitForIdle(); void* mapped{};
        s.context->MapBuffer(staging,MAP_READ,MAP_FLAG_NONE,mapped); Check(mapped!=nullptr,"numeric GPU map");
        std::vector<std::array<float,4>> result(vertices.size()*2); memcpy(result.data(),mapped,static_cast<size_t>(desc.Size));
        s.context->UnmapBuffer(staging,MAP_READ); s.context->InvalidateState(); return result;
    }
};
}
