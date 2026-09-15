#include "DiligentShadowAmbient.h"
#include "SceneLightingRuntime.h"
#include "ShadowAmbientShader.h"
#include "SkinningBenchmark.h"
#include "Graphics/GraphicsEngine/interface/Shader.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"
#include <chrono>
#include <algorithm>

namespace Renderer {
using namespace Diligent;
namespace {
struct ShadowConstants {
    std::array<Graphics::Matrix4,3> matrices;
    Graphics::Matrix4 view;
    std::array<float,4> splits,settings,bias,texels;
};
struct AOConstants {
    Graphics::Matrix4 inverse;
    std::array<float,4> size,parameters,fade;
    std::array<unsigned,4> modes;
};
void DynamicBuffer(IRenderDevice* d,const char* name,Uint64 size,RefCntAutoPtr<IBuffer>& result){
    BufferDesc desc;desc.Name=name;desc.Size=size;desc.Usage=USAGE_DYNAMIC;desc.BindFlags=BIND_UNIFORM_BUFFER;desc.CPUAccessFlags=CPU_ACCESS_WRITE;d->CreateBuffer(desc,nullptr,&result);
}
}
bool DiligentShadowAmbient::Initialize(IRenderDevice* d,IDeviceContext* c){
    device=d;context=c;DynamicBuffer(d,"G34 shared shadow constants",sizeof(ShadowConstants),shadowConstants);
    if(!shadowConstants)return false;++liveShadowBuffers;
    TextureDesc desc;desc.Name="G34 neutral depth fallback";desc.Type=RESOURCE_DIM_TEX_2D_ARRAY;desc.Width=desc.Height=1;desc.ArraySize=1;
    desc.Format=TEX_FORMAT_D32_FLOAT;desc.BindFlags=BIND_DEPTH_STENCIL|BIND_SHADER_RESOURCE;
    d->CreateTexture(desc,nullptr,&fallback);if(!fallback)return false;++liveShadowMaps;liveShadowViews+=2;
    c->SetRenderTargets(0,nullptr,fallback->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    c->ClearDepthStencil(fallback->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL),CLEAR_DEPTH_FLAG,1,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    c->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    SamplerDesc sampler;sampler.MinFilter=sampler.MagFilter=FILTER_TYPE_COMPARISON_LINEAR;sampler.MipFilter=FILTER_TYPE_COMPARISON_POINT;
    sampler.ComparisonFunc=COMPARISON_FUNC_LESS_EQUAL;sampler.AddressU=sampler.AddressV=sampler.AddressW=TEXTURE_ADDRESS_CLAMP;
    d->CreateSampler(sampler,&comparisonSampler);UploadShadows();return comparisonSampler!=nullptr;
}
void DiligentShadowAmbient::UploadShadows(){
    if(!shadowConstants)return;MapHelper<ShadowConstants> data(context,shadowConstants,MAP_WRITE,MAP_FLAG_DISCARD);if(!data)return;
    *data={};data->view=cameraView;
    for(unsigned i=0;i<3;++i){data->matrices[i]=cascades.cascade[i].worldToClip;data->splits[i]=cascades.cascade[i].farDistance;data->texels[i]=cascades.cascade[i].texelSize;}
    data->settings={float(cascades.count),1.f/shadowConfig.resolution,float(shadowConfig.filterRadius),float(cascadeDebugView)};
    data->bias={shadowConfig.depthBias,shadowConfig.slopeBias,shadowConfig.normalOffset,0};
}
void DiligentShadowAmbient::ReleaseShadows(){
    for(auto& view:cascadeDSV)if(view){view.Release();--liveShadowViews;}
    if(shadow){shadow.Release();--liveShadowMaps;--liveShadowViews;}shadowMemory=0;
}
void DiligentShadowAmbient::ReleaseAO(){
    ambientActive=false;aoBinding.Release();compositeBinding.Release();
    if(depthSRV){depthSRV.Release();--liveAOViews;}
    const auto release=[](RefCntAutoPtr<ITexture>& texture,unsigned views){if(texture){liveAOViews-=views;texture.Release();--liveAOTargets;}};
    release(ambient,2);release(depthCopy,0);release(colorCopy,1);release(rawAO,2);aoMemory=0;
}
void DiligentShadowAmbient::Reset(){shadowPassIndex=-1;cascades={};shadowCullingCascades={};shadowDraws=shadowCasters=shadowCulled=0;shadowCpuUs=aoCpuUs=0;frameShadowCasters.clear();ReleaseShadows();ReleaseAO();UploadShadows();}
void DiligentShadowAmbient::Shutdown(){
    if(context)CollectTimings();for(auto& t:timings){t.query.Release();t.serial=0;}
    frameShadowCasters.clear();frameShadowCasters.rehash(0);
    ReleaseAO();ReleaseShadows();
    if(fallback){fallback.Release();--liveShadowMaps;liveShadowViews-=2;}
    if(shadowConstants){shadowConstants.Release();--liveShadowBuffers;}
    if(aoConstants){aoConstants.Release();--liveAOBuffers;}
    if(aoPipeline){aoPipeline.Release();--liveAOPipelines;}if(compositePipeline){compositePipeline.Release();--liveAOPipelines;}
    comparisonSampler.Release();shadowPassIndex=-1;device=nullptr;context=nullptr;
}
unsigned DiligentShadowAmbient::Begin(const Matrix& view,const Matrix& projection,const Graphics::GraphicsRuntimeConfig& config,unsigned width,unsigned height){
    CollectTimings();frameShadowCasters.clear();
    shadowDraws=shadowCasters=shadowCulled=0;shadowCpuUs=aoCpuUs=0;
    if(config.style!=Graphics::GraphicsStyle::Modern){Reset();return 0;}
    cameraView=view;cameraProjection=projection;shadowConfig=Graphics::ValidateShadowConfig(config.shadowConfig);aoConfig=Graphics::ValidateAmbientConfig(config.ambientConfig);
    cascades=Graphics::BuildCascades(view,projection,sceneLighting.Get().sun.direction,shadowConfig);
    if(cascades.count){
        if(shadow&&(shadow->GetDesc().Width!=shadowConfig.resolution||shadow->GetDesc().ArraySize!=cascades.count))ReleaseShadows();
        if(!shadow){TextureDesc desc;desc.Name="G34 cascaded sun depth";desc.Type=RESOURCE_DIM_TEX_2D_ARRAY;desc.Width=desc.Height=shadowConfig.resolution;desc.ArraySize=cascades.count;
            desc.Format=TEX_FORMAT_D32_FLOAT;desc.BindFlags=BIND_DEPTH_STENCIL|BIND_SHADER_RESOURCE;device->CreateTexture(desc,nullptr,&shadow);
            if(shadow){++liveShadowMaps;++liveShadowViews;
                for(unsigned i=0;i<cascades.count;++i){TextureViewDesc v;v.Name="G34 cascade DSV";v.ViewType=TEXTURE_VIEW_DEPTH_STENCIL;v.TextureDim=RESOURCE_DIM_TEX_2D_ARRAY;v.Format=TEX_FORMAT_D32_FLOAT;v.FirstArraySlice=i;v.NumArraySlices=1;
                    shadow->CreateView(v,&cascadeDSV[i]);if(cascadeDSV[i])++liveShadowViews;else{ReleaseShadows();break;}}
            }
        }
        if(!shadow)cascades.count=0;else shadowMemory=std::uint64_t(shadowConfig.resolution)*shadowConfig.resolution*cascades.count*4;
    }else ReleaseShadows();
    if(aoConfig.quality&&width&&height){ambientActive=CreateAO(width,height);if(ambientActive){const float clear[]{0,0,0,0};
        auto* target=ambient->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        context->ClearRenderTarget(target,clear,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);}}
    else ReleaseAO();shadowCullingCascades=cascades;UploadShadows();return cascades.count;
}
bool DiligentShadowAmbient::BeginCascade(unsigned i){
    if(i>=cascades.count||!cascadeDSV[i])return false;shadowPassIndex=int(i);activeShadowCascade=cascades.cascade[i];
    context->SetRenderTargets(0,nullptr,cascadeDSV[i],RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    context->ClearDepthStencil(cascadeDSV[i],CLEAR_DEPTH_FLAG,1,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Viewport viewport{0,0,float(shadowConfig.resolution),float(shadowConfig.resolution),0,1};context->SetViewports(1,&viewport,shadowConfig.resolution,shadowConfig.resolution);return true;
}
void DiligentShadowAmbient::EndShadows(ISwapChain* swap){shadowPassIndex=-1;BindTargets(swap,false);UploadShadows();}
void DiligentShadowAmbient::BindTargets(ISwapChain* swap,bool modern){
    if(shadowPassIndex>=0){context->SetRenderTargets(0,nullptr,cascadeDSV[shadowPassIndex],RESOURCE_STATE_TRANSITION_MODE_TRANSITION);return;}
    ITextureView* targets[]{swap->GetCurrentBackBufferRTV(),ambientActive?ambient->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET):nullptr};
    context->SetRenderTargets(modern&&ambientActive?2:1,targets,swap->GetDepthBufferDSV(),RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    const auto& s=swap->GetDesc();Viewport viewport{0,0,float(s.Width),float(s.Height),0,1};context->SetViewports(1,&viewport,s.Width,s.Height);
}
void DiligentShadowAmbient::BindReceiver(IPipelineState* pipeline){
    if(auto* v=pipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL,"ShadowSceneConstants"))v->Set(shadowConstants);
    if(auto* v=pipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL,"SunShadowSampler"))v->Set(comparisonSampler);
}
void DiligentShadowAmbient::SetReceiver(IShaderResourceBinding* binding,bool release){
    if(auto* v=binding->GetVariableByName(SHADER_TYPE_PIXEL,"SunShadowDepth"))v->Set((!release&&shadow?shadow:fallback)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}
bool DiligentShadowAmbient::CreateAO(unsigned width,unsigned height){
    const unsigned w=(width+aoConfig.divisor-1)/aoConfig.divisor,h=(height+aoConfig.divisor-1)/aoConfig.divisor;
    if(ambient&&ambient->GetDesc().Width==width&&ambient->GetDesc().Height==height&&rawAO&&rawAO->GetDesc().Width==w&&rawAO->GetDesc().Height==h)return true;
    ReleaseAO();
    const auto texture=[&](const char* name,unsigned tw,unsigned th,TEXTURE_FORMAT format,BIND_FLAGS flags,RefCntAutoPtr<ITexture>& t){
        TextureDesc d;d.Name=name;d.Type=RESOURCE_DIM_TEX_2D;d.Width=tw;d.Height=th;d.Format=format;d.BindFlags=flags;device->CreateTexture(d,nullptr,&t);
        if(t){++liveAOTargets;liveAOViews+=unsigned(bool(flags&BIND_SHADER_RESOURCE))+unsigned(bool(flags&BIND_RENDER_TARGET));}return bool(t);
    };
    if(!texture("G34 LDR ambient contribution",width,height,TEX_FORMAT_RGBA8_UNORM,BIND_RENDER_TARGET|BIND_SHADER_RESOURCE,ambient)||
       !texture("G34 scene LDR copy",width,height,TEX_FORMAT_RGBA8_UNORM,BIND_SHADER_RESOURCE,colorCopy)||
       !texture("G34 ambient horizon visibility",w,h,TEX_FORMAT_R8_UNORM,BIND_RENDER_TARGET|BIND_SHADER_RESOURCE,rawAO)){ReleaseAO();return false;}
    // Copying depth is not a geometry prepass. The source remains the shared scene D24 depth.
    // A shader-resource bind is required even though the view has an explicit typed format.
    TextureDesc dd;dd.Name="G34 sampled scene depth";dd.Type=RESOURCE_DIM_TEX_2D;dd.Width=width;dd.Height=height;dd.Format=TEX_FORMAT_R24G8_TYPELESS;dd.BindFlags=BIND_SHADER_RESOURCE;
    device->CreateTexture(dd,nullptr,&depthCopy);if(!depthCopy){ReleaseAO();return false;}++liveAOTargets;
    TextureViewDesc sv;sv.ViewType=TEXTURE_VIEW_SHADER_RESOURCE;sv.Format=TEX_FORMAT_R24_UNORM_X8_TYPELESS;depthCopy->CreateView(sv,&depthSRV);
    if(!depthSRV){ReleaseAO();return false;}++liveAOViews;
    if(!aoConstants){DynamicBuffer(device,"G34 ambient projection and radius",sizeof(AOConstants),aoConstants);if(!aoConstants){ReleaseAO();return false;}++liveAOBuffers;}
    if(!aoPipeline||!compositePipeline){
        ShaderCreateInfo shader;shader.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL;shader.Source=ambientDepthShader;
        shader.Desc.Name="G34 fullscreen triangle";shader.Desc.ShaderType=SHADER_TYPE_VERTEX;shader.EntryPoint="FullVS";
        RefCntAutoPtr<IShader> vs,ps,composite;device->CreateShader(shader,&vs);
        shader.Desc.ShaderType=SHADER_TYPE_PIXEL;shader.Desc.Name="G34 horizon ambient depth";shader.EntryPoint="AmbientPS";device->CreateShader(shader,&ps);
        shader.Desc.Name="G34 bilateral ambient-only composite";shader.EntryPoint="CompositePS";device->CreateShader(shader,&composite);
        if(!vs||!ps||!composite){ReleaseAO();return false;}
        const ShaderResourceVariableDesc variables[]={{SHADER_TYPE_PIXEL,"SceneDepth",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{SHADER_TYPE_PIXEL,"SceneColor",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            {SHADER_TYPE_PIXEL,"AmbientDelta",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{SHADER_TYPE_PIXEL,"AmbientRaw",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
        for(unsigned i=0;i<2;++i){auto& pipeline=i?compositePipeline:aoPipeline;if(pipeline)continue;
            GraphicsPipelineStateCreateInfo info;info.PSODesc.Name=i?"G34 ambient composite":"G34 horizon AO";info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
            info.PSODesc.ResourceLayout.Variables=variables;info.PSODesc.ResourceLayout.NumVariables=i?4:1;
            auto& g=info.GraphicsPipeline;g.NumRenderTargets=1;g.RTVFormats[0]=i?TEX_FORMAT_RGBA8_UNORM:TEX_FORMAT_R8_UNORM;
            g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;g.RasterizerDesc.CullMode=CULL_MODE_NONE;g.DepthStencilDesc.DepthEnable=False;
            info.pVS=vs;info.pPS=i?composite:ps;device->CreateGraphicsPipelineState(info,&pipeline);if(!pipeline){ReleaseAO();return false;}++liveAOPipelines;
            pipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL,"AmbientConstants")->Set(aoConstants);
        }
    }
    aoPipeline->CreateShaderResourceBinding(&aoBinding,true);compositePipeline->CreateShaderResourceBinding(&compositeBinding,true);
    if(!aoBinding||!compositeBinding){ReleaseAO();return false;}
    aoMemory=std::uint64_t(width)*height*12+std::uint64_t(w)*h;return true;
}
void DiligentShadowAmbient::Composite(ISwapChain* swap,ITextureView* depth){
    if(!ambientActive)return;const auto start=std::chrono::steady_clock::now();ambientActive=false;
    if(!depth){BindTargets(swap,false);return;}
    Graphics::Matrix4 corrected=cameraProjection;const auto& size=swap->GetDesc();
    for(unsigned row=0;row<4;++row){corrected[row*4]+=cameraProjection[row*4+3]/size.Width;corrected[row*4+1]-=cameraProjection[row*4+3]/size.Height;}
    Graphics::Matrix4 inverse;if(!Graphics::Inverse(corrected,inverse)){BindTargets(swap,false);return;}
    Timing* active=nullptr;
    if(skinningBenchmarkEnabled){auto& slot=timings[timingIndex++%timings.size()];
        if(!slot.query){QueryDesc q;q.Name="G34 AO copy, horizon and bilateral composite";q.Type=QUERY_TYPE_DURATION;device->CreateQuery(q,&slot.query);}
        if(slot.query&&!slot.serial){slot.serial=skinningBenchmarkCurrent.serial;active=&slot;context->BeginQuery(slot.query);}}
    context->SetRenderTargets(0,nullptr,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    CopyTextureAttribs copy;copy.SrcTextureTransitionMode=copy.DstTextureTransitionMode=RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    copy.pSrcTexture=depth->GetTexture();copy.pDstTexture=depthCopy;context->CopyTexture(copy);
    copy.pSrcTexture=swap->GetCurrentBackBufferRTV()->GetTexture();copy.pDstTexture=colorCopy;context->CopyTexture(copy);
    {MapHelper<AOConstants> data(context,aoConstants,MAP_WRITE,MAP_FLAG_DISCARD);if(!data){if(active)context->EndQuery(active->query);BindTargets(swap,false);return;}
        data->inverse=inverse;data->size={float(size.Width),float(size.Height),float(rawAO->GetDesc().Width),float(rawAO->GetDesc().Height)};
        data->parameters={aoConfig.radius,aoConfig.intensity,std::abs(cameraProjection[5])*size.Height*.5f,0};
        data->fade={aoConfig.fadeStart,aoConfig.fadeEnd,0,0};data->modes={aoConfig.directions,aoConfig.steps,ambientDebugView,0};}
    auto* target=rawAO->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    Viewport viewport{0,0,float(rawAO->GetDesc().Width),float(rawAO->GetDesc().Height),0,1};context->SetViewports(1,&viewport,rawAO->GetDesc().Width,rawAO->GetDesc().Height);
    context->SetPipelineState(aoPipeline);aoBinding->GetVariableByName(SHADER_TYPE_PIXEL,"SceneDepth")->Set(depthSRV);
    context->CommitShaderResources(aoBinding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->Draw(DrawAttribs{3,DRAW_FLAG_VERIFY_ALL});
    target=swap->GetCurrentBackBufferRTV();context->SetRenderTargets(1,&target,nullptr,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    viewport.Width=float(size.Width);viewport.Height=float(size.Height);context->SetViewports(1,&viewport,size.Width,size.Height);
    context->SetPipelineState(compositePipeline);
    compositeBinding->GetVariableByName(SHADER_TYPE_PIXEL,"SceneDepth")->Set(depthSRV);
    compositeBinding->GetVariableByName(SHADER_TYPE_PIXEL,"SceneColor")->Set(colorCopy->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    compositeBinding->GetVariableByName(SHADER_TYPE_PIXEL,"AmbientDelta")->Set(ambient->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    compositeBinding->GetVariableByName(SHADER_TYPE_PIXEL,"AmbientRaw")->Set(rawAO->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    context->CommitShaderResources(compositeBinding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);context->Draw(DrawAttribs{3,DRAW_FLAG_VERIFY_ALL});
    context->InvalidateState();BindTargets(swap,false);
    if(active)context->EndQuery(active->query);
    aoCpuUs=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
}
void DiligentShadowAmbient::CollectTimings(){
    for(auto& slot:timings)if(slot.query&&slot.serial){QueryDataDuration data;
        if(slot.query->GetData(&data,sizeof(data),true)){if(data.Frequency){lastAOGpuUs=double(data.Duration)*1e6/double(data.Frequency);
            if(ambientBenchmarkGpuTimes.size()<skinningBenchmarkLimit)ambientBenchmarkGpuTimes[slot.serial]=lastAOGpuUs;}slot.serial=0;}}
}
}
