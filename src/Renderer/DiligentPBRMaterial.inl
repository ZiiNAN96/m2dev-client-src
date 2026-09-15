// Implementation detail of DiligentStaticObjectRenderer.cpp; no loader-specific paths.
bool DiligentStaticObjectRenderer::Impl::InitializePBR(bool gpu)
{
    auto* device=backend.m_impl->device.RawPtr();
    BufferDesc buffer;buffer.Name="PBR material/transform constants";buffer.Size=sizeof(PBRConstants);
    buffer.Usage=USAGE_DYNAMIC;buffer.BindFlags=BIND_UNIFORM_BUFFER;buffer.CPUAccessFlags=CPU_ACCESS_WRITE;
    device->CreateBuffer(buffer,nullptr,&pbrConstants);if(!pbrConstants)return false;
    SamplerDesc sampler;sampler.MinFilter=sampler.MagFilter=sampler.MipFilter=FILTER_TYPE_LINEAR;
    sampler.AddressU=sampler.AddressV=TEXTURE_ADDRESS_WRAP;
    device->CreateSampler(sampler,&pbrSampler);if(!pbrSampler)return false;
    const std::string litSource=std::string(sceneLightingShader)+pbrShader;
    ShaderCreateInfo shader;shader.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL;shader.Source=litSource.c_str();
    shader.Desc.Name="G1 PBR rigid";shader.Desc.ShaderType=SHADER_TYPE_VERTEX;shader.EntryPoint="PBRVS";
    RefCntAutoPtr<IShader> vs,skinVS,ps;device->CreateShader(shader,&vs);
    shader.Desc.Name="G1 metallic roughness GGX";shader.Desc.ShaderType=SHADER_TYPE_PIXEL;shader.EntryPoint="PBRPS";
    device->CreateShader(shader,&ps);if(!vs||!ps)return false;
    const std::string skinned=litSource+gpuSkinningShader+R"(
POutput PBRSkinVS(float3 position:ATTRIB0,float3 normal:ATTRIB1,float2 oldUV:ATTRIB2,
 uint4 weights:ATTRIB3,uint4 indices:ATTRIB4,float4 tangent:ATTRIB5,float2 uv:ATTRIB6) {
 float3 p,n,unused,t;SkinVertex(position,normal,weights,indices,p,n);
 SkinVertex(float3(0,0,0),tangent.xyz,weights,indices,unused,t);
 float3x3 skinMatrix=0;
 [unroll]for(uint k=0;k<4;++k)if(weights[k]!=0)skinMatrix+=float(weights[k])*(1.0/255.0)*(float3x3)Bones[indices[k]];
 n=LightingTransformNormal(normal,skinMatrix);
 return PBRVS(p,n,oldUV,float4(t,tangent.w*(determinant(skinMatrix)<0?-1:1)),uv);
})";
    if(gpu){shader.Source=skinned.c_str();shader.Desc.Name="G1 PBR GPU skinned";shader.Desc.ShaderType=SHADER_TYPE_VERTEX;shader.EntryPoint="PBRSkinVS";device->CreateShader(shader,&skinVS);if(!skinVS)return false;}
    LayoutElement rigidLayout[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32},
        {5,1,4,VT_FLOAT32,False,0,24},{6,1,2,VT_FLOAT32,False,16,24}};
    LayoutElement skinLayout[]={{0,0,3,VT_FLOAT32,False,0,40},{1,0,3,VT_FLOAT32,False,20,40},{2,0,2,VT_FLOAT32,False,32,40},
        {3,0,4,VT_UINT8,False,12,40},{4,0,4,VT_UINT8,False,16,40},{5,1,4,VT_FLOAT32,False,0,24},{6,1,2,VT_FLOAT32,False,16,24}};
    ShaderResourceVariableDesc variables[]={{SHADER_TYPE_PIXEL,"PBaseTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL,"PNormalTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL,"PMetalRoughTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL,"POcclusionTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL,"PEmissiveTexture",SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE},
        {SHADER_TYPE_PIXEL,"PCameraTexture",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_PIXEL,"PCameraSampler",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_VERTEX,"SkinningPalette",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
    for(unsigned variant=0;variant<24;++variant) {
        const bool skin=variant>=12;if(skin&&!gpu)continue;
        GraphicsPipelineStateCreateInfo info;info.PSODesc.Name="G1 PBR material";info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        info.PSODesc.ResourceLayout.Variables=variables;info.PSODesc.ResourceLayout.NumVariables=skin?8:7;
        auto& g=info.GraphicsPipeline;const auto& swap=backend.m_impl->swapChain->GetDesc();
        g.NumRenderTargets=1;g.RTVFormats[0]=swap.ColorBufferFormat;g.DSVFormat=swap.DepthBufferFormat;
        g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        g.RasterizerDesc.CullMode=variant%3==0?CULL_MODE_NONE:CULL_MODE_BACK;
        // Disabling culling must not reverse the definition of a front face.
        // The normal asset triangle convention is the same as CullClockwise.
        g.RasterizerDesc.FrontCounterClockwise=variant%3!=2;g.RasterizerDesc.DepthClipEnable=True;
        g.DepthStencilDesc.DepthEnable=True;g.DepthStencilDesc.DepthWriteEnable=variant%12<6;
        g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;
        auto& blend=g.BlendDesc.RenderTargets[0];blend.BlendEnable=(variant/3)%2!=0;
        blend.SrcBlend=blend.SrcBlendAlpha=BLEND_FACTOR_SRC_ALPHA;blend.DestBlend=blend.DestBlendAlpha=BLEND_FACTOR_INV_SRC_ALPHA;
        g.InputLayout.LayoutElements=skin?skinLayout:rigidLayout;g.InputLayout.NumElements=skin?7:5;
        info.pVS=skin?skinVS:vs;info.pPS=ps;
        auto& pipeline=pbrPipelines[variant];device->CreateGraphicsPipelineState(info,&pipeline);if(!pipeline)return false;
        ++livePBRPipelines;
        for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})if(auto* v=pipeline->GetStaticVariableByName(stage,"PBRConstants"))v->Set(pbrConstants);
        if(auto* v=pipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL,"SceneLightingConstants"))v->Set(backend.m_impl->lightBuffer);
        if(auto* v=pipeline->GetStaticVariableByName(SHADER_TYPE_PIXEL,"PMaterialSampler"))v->Set(pbrSampler);
    }
    return true;
}

void DiligentStaticObjectRenderer::Impl::DrawPBR(const std::shared_ptr<Geometry>& mesh,
    const std::shared_ptr<Texture>& camera,const StaticObjectDraw& draw,bool skin,bool rigid,unsigned variant)
{
    try {
        auto& b=*backend.m_impl;const auto& runtime=*draw.material;const auto& p=runtime.parameters;
        if(!b.SyncSceneLighting()){Fail("scene lighting upload",__LINE__);return;}
        if(!pbrPipelines[variant]||!mesh->materialVertices){Fail("missing PBR pipeline or material vertex stream",__LINE__);return;}
        auto& owned=pbrBindings[draw.material.get()];
        if(!owned || owned->material.lock()!=draw.material){
            owned=std::make_unique<PBRBinding>();owned->material=draw.material;
            for(unsigned slot=0;slot<owned->images.size();++slot) {
                auto& image=owned->images[slot];image=std::dynamic_pointer_cast<Texture>(runtime.maps[slot]);
                if(image){if(image->counters!=counters){Fail("PBR texture owner mismatch",__LINE__);return;}owned->maps|=1u<<slot;if(image->srgbView)owned->srgb|=1u<<slot;}
            }
        }
        auto& binding=owned->bindings[variant];
        const auto maps=owned->maps,srgb=owned->srgb;const auto& images=owned->images;
        if(!images[0]){Fail("PBR base color missing",__LINE__);return;}
        if(!binding) {
            pbrPipelines[variant]->CreateShaderResourceBinding(&binding,true);
            if(!binding){Fail("PBR binding creation",__LINE__);return;}
            const char* names[]={"PBaseTexture","PNormalTexture","PMetalRoughTexture","POcclusionTexture","PEmissiveTexture"};
            for(unsigned slot=0;slot<images.size();++slot) {
                auto image=images[slot]?images[slot]:images[0];
                const bool color=AssetRuntime::MapColorSpace(static_cast<AssetRuntime::MaterialMap>(slot))==AssetRuntime::ColorSpace::SRGB;
                binding->GetVariableByName(SHADER_TYPE_PIXEL,names[slot])->Set(color&&image->srgbView?image->srgbView:image->linearView);
            }
            if(skin)owned->palettes[variant]=binding->GetVariableByName(SHADER_TYPE_VERTEX,"SkinningPalette");
            owned->cameraVariables[variant]=binding->GetVariableByName(SHADER_TYPE_PIXEL,"PCameraTexture");
            owned->cameraSamplers[variant]=binding->GetVariableByName(SHADER_TYPE_PIXEL,"PCameraSampler");
        }
        if(skin)owned->palettes[variant]->Set(mesh->pose->buffer);
        owned->cameraVariables[variant]->Set(camera->linearView);
        if(draw.cameraAlpha) {
            if(!camera->cameraSampler || !(camera->cameraSampling==draw.cameraAlphaSampling) ||
               camera->cameraAnisotropic!=draw.cameraAlphaAnisotropic || camera->cameraMaxAnisotropy!=draw.cameraAlphaMaxAnisotropy) {
                SamplerDesc sampler;
                sampler.MinFilter=draw.cameraAlphaSampling.linearMin?FILTER_TYPE_LINEAR:FILTER_TYPE_POINT;
                sampler.MagFilter=draw.cameraAlphaSampling.linearMag?FILTER_TYPE_LINEAR:FILTER_TYPE_POINT;
                sampler.MipFilter=draw.cameraAlphaSampling.linearMip?FILTER_TYPE_LINEAR:FILTER_TYPE_POINT;
                if(draw.cameraAlphaAnisotropic){sampler.MinFilter=sampler.MagFilter=sampler.MipFilter=FILTER_TYPE_ANISOTROPIC;sampler.MaxAnisotropy=draw.cameraAlphaMaxAnisotropy;}
                sampler.AddressU=draw.cameraAlphaSampling.wrapU?TEXTURE_ADDRESS_WRAP:TEXTURE_ADDRESS_CLAMP;
                sampler.AddressV=draw.cameraAlphaSampling.wrapV?TEXTURE_ADDRESS_WRAP:TEXTURE_ADDRESS_CLAMP;
                if(!draw.cameraAlphaSampling.useMips)sampler.MaxLOD=0;
                camera->cameraSampler.Release();b.device->CreateSampler(sampler,&camera->cameraSampler);
                if(!camera->cameraSampler){Fail("PBR camera mask sampler",__LINE__);return;}
                camera->cameraSampling=draw.cameraAlphaSampling;camera->cameraAnisotropic=draw.cameraAlphaAnisotropic;camera->cameraMaxAnisotropy=draw.cameraAlphaMaxAnisotropy;
            }
        }
        owned->cameraSamplers[variant]->Set(draw.cameraAlpha?camera->cameraSampler:pbrSampler);
        {
            MapHelper<PBRConstants> mapped(b.context,pbrConstants,MAP_WRITE,MAP_FLAG_DISCARD);
            if(!mapped){Fail("PBR constants upload",__LINE__);return;}
            mapped->matrices=draw.matrices;mapped->normal=draw.normalTransform;
            const auto& size=b.swapChain->GetDesc();const auto width=draw.viewport[2]?draw.viewport[2]:size.Width,height=draw.viewport[3]?draw.viewport[3]:size.Height;
            for(unsigned row=0;row<4;++row){mapped->matrices.projection[row*4]+=draw.matrices.projection[row*4+3]/width;mapped->matrices.projection[row*4+1]-=draw.matrices.projection[row*4+3]/height;}
            mapped->fogColor=draw.fogColor;mapped->fogParameters=draw.fogParameters;
            mapped->baseColor=p.baseColor;mapped->emissive={p.emissive[0],p.emissive[1],p.emissive[2],0};
            if(!runtime.authored && draw.actorStage==ActorMaterialStage::Modulate)
                for(unsigned c=0;c<3;++c)mapped->baseColor[c]*=draw.textureFactor[c];
            mapped->factors={p.roughness,p.metallic,p.normalScale,p.occlusionStrength};
            for(unsigned slot=0;slot<images.size();++slot)for(unsigned row=0;row<2;++row)
                mapped->uvRows[slot*2+row]={p.maps[slot].uvTransform[row*3],p.maps[slot].uvTransform[row*3+1],p.maps[slot].uvTransform[row*3+2],0};
            mapped->flags={maps,srgb,unsigned(draw.fog)+(draw.rangeFog?256u:0u),unsigned(materialDebugView)};
            auto alpha=draw.factorAlphaOnly?4u:draw.factorAlpha?3u:draw.diffuseAlphaOnly?2u:unsigned(draw.textureAlpha);
            // Authored glTF base alpha already includes its factor. Preserve OPAQUE alpha=1.
            if(runtime.explicitRenderState)alpha=p.alpha==AssetRuntime::AlphaMode::Opaque?4u:1u;
            mapped->alpha={alpha,unsigned(draw.alphaTest),draw.alphaReference,draw.cameraAlpha?1u:0u};
            mapped->fade={draw.ambient[3],draw.textureFactor[3],0,0};mapped->camera=draw.cameraAlphaTransform;
        }
        b.context->SetPipelineState(pbrPipelines[variant]);const auto& size=b.swapChain->GetDesc();
        Viewport viewport{float(draw.viewport[0]),float(draw.viewport[1]),float(draw.viewport[2]?draw.viewport[2]:size.Width),float(draw.viewport[3]?draw.viewport[3]:size.Height),0,1};
        b.context->SetViewports(1,&viewport,size.Width,size.Height);
        IBuffer* buffers[]={rigid?mesh->skin->rigidVertices.RawPtr():mesh->vertices.RawPtr(),mesh->materialVertices.RawPtr()};
        Uint64 offsets[]={0,rigid?Uint64(mesh->skin->deformCount)*sizeof(AssetRuntime::MaterialVertex):0};
        b.context->SetVertexBuffers(0,2,buffers,offsets,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
        b.context->SetIndexBuffer(mesh->indices,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        b.context->CommitShaderResources(binding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        DrawIndexedAttribs attributes{draw.indexCount,mesh->indexType,DRAW_FLAG_VERIFY_ALL};
        attributes.FirstIndexLocation=draw.firstIndex;attributes.BaseVertex=draw.baseVertex-(rigid?mesh->skin->deformCount:0);
        b.context->DrawIndexed(attributes);++draws;++pbrDraws;
        pbrTextureSamples+=1+bool(maps&2)+bool(maps&4)+bool(maps&8)+bool(maps&16)+bool(draw.cameraAlpha);
    }catch(...){Fail("PBR draw exception",__LINE__);}
}
