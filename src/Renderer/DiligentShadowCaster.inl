// Same immutable geometry, native card deformation and current GPU palette as the main pass.
bool DiligentStaticObjectRenderer::Impl::InitializeShadows(bool gpu){
    auto& b=*backend.m_impl;
    BufferDesc buffer;buffer.Name="G34 caster alpha material";buffer.Size=48;buffer.Usage=USAGE_DYNAMIC;buffer.BindFlags=BIND_UNIFORM_BUFFER;buffer.CPUAccessFlags=CPU_ACCESS_WRITE;
    b.device->CreateBuffer(buffer,nullptr,&shadowMaterial);if(!shadowMaterial)return false;++liveShadowBuffers;
    SamplerDesc sampler;sampler.MinFilter=sampler.MagFilter=sampler.MipFilter=FILTER_TYPE_LINEAR;sampler.AddressU=sampler.AddressV=TEXTURE_ADDRESS_WRAP;
    b.device->CreateSampler(sampler,&shadowSampler);if(!shadowSampler)return false;
    const std::string source=std::string("#define SHADOW_CASTER\n")+shaderSource+gpuSkinningShader+R"(
Output ShadowRigidVS(float3 p:ATTRIB0,float3 n:ATTRIB1,float2 oldUV:ATTRIB2,float2 uv:ATTRIB6){return VS(p,n,uv);}
Output ShadowSkinVS(float3 p:ATTRIB0,float3 n:ATTRIB1,float2 oldUV:ATTRIB2,uint4 w:ATTRIB3,uint4 bones:ATTRIB4,float2 uv:ATTRIB6){float3 pos,normal;SkinVertex(p,n,w,bones,pos,normal);return VS(pos,normal,uv);}
)";
    ShaderCreateInfo shader;shader.SourceLanguage=SHADER_SOURCE_LANGUAGE_HLSL;shader.Source=source.c_str();shader.Desc.Name="G34 alpha masked caster";shader.Desc.ShaderType=SHADER_TYPE_PIXEL;shader.EntryPoint="PS";
    RefCntAutoPtr<IShader> ps;std::array<RefCntAutoPtr<IShader>,3> vs;b.device->CreateShader(shader,&ps);if(!ps)return false;
    const char* entries[]{"ShadowRigidVS","ShadowSkinVS","AuxiliaryVS"};
    for(unsigned i=0;i<3;++i){if(i==1&&!gpu)continue;shader.Desc.ShaderType=SHADER_TYPE_VERTEX;shader.EntryPoint=entries[i];b.device->CreateShader(shader,&vs[i]);if(!vs[i])return false;}
    LayoutElement rigid[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32},{6,1,2,VT_FLOAT32,False,16,24}};
    LayoutElement skin[]={{0,0,3,VT_FLOAT32,False,0,40},{1,0,3,VT_FLOAT32,False,20,40},{2,0,2,VT_FLOAT32,False,32,40},{3,0,4,VT_UINT8,False,12,40},{4,0,4,VT_UINT8,False,16,40},{6,1,2,VT_FLOAT32,False,16,24}};
    LayoutElement auxiliary[]={{0,0,3,VT_FLOAT32,False,0,32},{1,0,3,VT_FLOAT32,False,12,32},{2,0,2,VT_FLOAT32,False,24,32},{3,1,4,VT_FLOAT32,False,0,64},{4,1,2,VT_FLOAT32,False,16,64},{5,1,3,VT_FLOAT32,False,24,64},{6,1,1,VT_FLOAT32,False,36,64},{7,1,3,VT_FLOAT32,False,40,64},{8,1,3,VT_FLOAT32,False,52,64}};
    const ShaderResourceVariableDesc variables[]={{SHADER_TYPE_PIXEL,"DiffuseTexture",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{SHADER_TYPE_PIXEL,"CameraAlphaTexture",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},{SHADER_TYPE_VERTEX,"SkinningPalette",SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC}};
    for(unsigned group=0;group<3;++group)for(unsigned cull=0;cull<3;++cull){if(!vs[group])continue;
        unsigned index=group*3+cull;GraphicsPipelineStateCreateInfo info;info.PSODesc.Name="G34 shared geometry caster";info.PSODesc.PipelineType=PIPELINE_TYPE_GRAPHICS;
        info.PSODesc.ResourceLayout.Variables=variables;info.PSODesc.ResourceLayout.NumVariables=group==1?3:2;
        auto& g=info.GraphicsPipeline;g.NumRenderTargets=0;g.DSVFormat=TEX_FORMAT_D32_FLOAT;g.PrimitiveTopology=PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        g.RasterizerDesc.CullMode=cull?CULL_MODE_BACK:CULL_MODE_NONE;g.RasterizerDesc.FrontCounterClockwise=cull!=2;
        g.RasterizerDesc.DepthClipEnable=True;g.RasterizerDesc.DepthBias=Graphics::ShadowQualityConfig::RasterDepthBias;g.RasterizerDesc.SlopeScaledDepthBias=Graphics::ShadowQualityConfig{}.slopeBias;
        g.DepthStencilDesc.DepthEnable=g.DepthStencilDesc.DepthWriteEnable=True;g.DepthStencilDesc.DepthFunc=COMPARISON_FUNC_LESS_EQUAL;
        g.InputLayout.LayoutElements=group==2?auxiliary:group==1?skin:rigid;g.InputLayout.NumElements=group==2?9:group==1?6:4;
        info.pVS=vs[group];info.pPS=ps;b.device->CreateGraphicsPipelineState(info,&shadowPipelines[index]);auto* p=shadowPipelines[index].RawPtr();if(!p)return false;++liveShadowPipelines;
        for(auto stage:{SHADER_TYPE_VERTEX,SHADER_TYPE_PIXEL})if(auto* v=p->GetStaticVariableByName(stage,"ObjectConstants"))v->Set(constants);
        if(auto* v=p->GetStaticVariableByName(SHADER_TYPE_PIXEL,"ShadowMaterial"))v->Set(shadowMaterial);
        for(auto name:{"ObjectSampler","CameraAlphaSampler"})if(auto* v=p->GetStaticVariableByName(SHADER_TYPE_PIXEL,name))v->Set(shadowSampler);
        p->CreateShaderResourceBinding(&shadowBindings[index],true);if(!shadowBindings[index])return false;
    }return true;
}
void DiligentStaticObjectRenderer::Impl::DrawShadow(const std::shared_ptr<Geometry>& mesh,const std::shared_ptr<Texture>& image,const StaticObjectDraw& draw,bool skin,bool rigid,bool auxiliary){
    if(!draw.depthWrite||draw.blend)return;
    // Actor bounds are culled before submission. Rigid world meshes also reject per cascade here.
    if(!mesh->skin&&mesh->boundsRadius>0){
        const auto center=Graphics::TransformPoint(mesh->boundsCenter,draw.matrices.world);float scale=0;
        for(unsigned row=0;row<3;++row)scale=std::max(scale,std::hypot(draw.matrices.world[row*4],draw.matrices.world[row*4+1],draw.matrices.world[row*4+2]));
        if(!ShadowVisible(center,(mesh->boundsRadius+(auxiliary?100.f:0.f))*scale)){++shadowCulled;return;}
    }
    auto& b=*backend.m_impl;unsigned variant=(auxiliary?6:skin?3:0)+unsigned(draw.cull);auto* pipeline=shadowPipelines[variant].RawPtr();auto* binding=shadowBindings[variant].RawPtr();
    if(!pipeline||!binding){Fail("missing G34 caster pipeline",__LINE__);return;}
    auto baseImage=image;std::array<float,12> material{1,1,1,1,1,0,0,0,0,1,0,0};
    if(draw.material&&!auxiliary){const auto& p=draw.material->parameters;if(auto texture=std::dynamic_pointer_cast<Texture>(draw.material->maps[0]))baseImage=texture;
        std::copy(p.baseColor.begin(),p.baseColor.end(),material.begin());for(unsigned r=0;r<2;++r)for(unsigned c=0;c<3;++c)material[4+r*4+c]=p.maps[0].uvTransform[r*3+c];}
    {MapHelper<std::array<float,12>> mapped(b.context,shadowMaterial,MAP_WRITE,MAP_FLAG_DISCARD);if(!mapped)return;*mapped=material;}
    {MapHelper<Constants> mapped(b.context,constants,MAP_WRITE,MAP_FLAG_DISCARD);if(!mapped)return;*mapped={};
        mapped->matrices={draw.matrices.world,activeShadowCascade.view,activeShadowCascade.projection};mapped->normal=draw.normalTransform;
        mapped->ambient=draw.ambient;mapped->textureFactor=draw.textureFactor;
        mapped->cardRight=draw.cardRight;mapped->cardForward=draw.cardForward;mapped->cardUp=draw.cardUp;mapped->wind=draw.wind;mapped->cardPitch=draw.cardPitch;
        mapped->vertexModes={draw.cardMode,0,0,0};
        unsigned alpha=draw.factorAlphaOnly?4u:draw.factorAlpha?3u:draw.diffuseAlphaOnly?2u:unsigned(draw.textureAlpha);
        if(draw.material&&draw.material->explicitRenderState)alpha=draw.material->parameters.alpha==AssetRuntime::AlphaMode::Opaque?4u:1u;
        mapped->alphaModes={alpha,unsigned(draw.alphaTest),draw.alphaReference,0};
    }
    if(auto* v=binding->GetVariableByName(SHADER_TYPE_PIXEL,"DiffuseTexture"))v->Set(baseImage->linearView);
    if(auto* v=binding->GetVariableByName(SHADER_TYPE_PIXEL,"CameraAlphaTexture"))v->Set(baseImage->linearView);
    if(skin)binding->GetVariableByName(SHADER_TYPE_VERTEX,"SkinningPalette")->Set(mesh->pose->buffer);
    b.depthEffects.BindTargets(b.swapChain,false);b.context->SetPipelineState(pipeline);
    IBuffer* vertices[]{rigid?mesh->skin->rigidVertices.RawPtr():mesh->vertices.RawPtr(),auxiliary?mesh->extras.RawPtr():mesh->materialVertices.RawPtr()};
    Uint64 offsets[]{0,rigid?Uint64(mesh->skin->deformCount)*sizeof(AssetRuntime::MaterialVertex):0};
    b.context->SetVertexBuffers(0,2,vertices,offsets,RESOURCE_STATE_TRANSITION_MODE_TRANSITION,SET_VERTEX_BUFFERS_FLAG_RESET);
    b.context->SetIndexBuffer(mesh->indices,0,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);b.context->CommitShaderResources(binding,RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    DrawIndexedAttribs attributes{draw.indexCount,mesh->indexType,DRAW_FLAG_VERIFY_ALL};attributes.FirstIndexLocation=draw.firstIndex;attributes.BaseVertex=draw.baseVertex-(rigid?mesh->skin->deformCount:0);
    b.context->DrawIndexed(attributes);RecordShadowCaster(mesh.get(),draw.matrices.world);
    for(auto name:{"DiffuseTexture","CameraAlphaTexture"})if(auto* v=binding->GetVariableByName(SHADER_TYPE_PIXEL,name))v->Set(nullptr);
    if(skin)binding->GetVariableByName(SHADER_TYPE_VERTEX,"SkinningPalette")->Set(nullptr);
}
