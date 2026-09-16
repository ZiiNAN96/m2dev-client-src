#include "VegetationRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <chrono>

namespace Vegetation {
std::shared_ptr<const RenderAsset> Prepare(AssetPtr asset,::Renderer::IStaticObjectRenderer&renderer,const TextureResolver&texture,std::string&error,bool optional){
    try{
        if(!asset)throw std::runtime_error("missing vegetation asset");auto result=std::make_shared<RenderAsset>();result->asset=asset;
        const auto model=asset->geometry.Model(0);result->textures.resize(model.Get()->materials.size());result->materials.resize(model.Get()->materials.size());
        result->instanceBuffers.resize(asset->metadata.parts.size()*8); // color/shadow, LOD fade side, transform winding
        for(const auto&part:asset->metadata.parts){const auto&mesh=model.Get()->meshes[part.mesh];::Renderer::StaticObjectSource source;source.vertices.resize(mesh.vertexCount);source.indices32.resize(mesh.indexCount);
            if(asset->geometry.Get()->CopyVertices(0,part.mesh,AssetRuntime::VertexLayout::PositionNormalUV,std::as_writable_bytes(std::span(source.vertices)))!=AssetRuntime::AssetError::None||
               asset->geometry.Get()->CopyIndices(0,part.mesh,AssetRuntime::IndexWidth::UInt32,std::as_writable_bytes(std::span(source.indices32)))!=AssetRuntime::AssetError::None)throw std::runtime_error("vegetation mesh upload data unavailable");
            if(mesh.vertexExtras.size()!=mesh.vertexCount)throw std::runtime_error("vegetation GLB is missing auxiliary channels");source.vertexExtras.resize(mesh.vertexCount);
            for(std::size_t v=0;v<source.vertexExtras.size();++v){const auto&e=mesh.vertexExtras[v];source.vertexExtras[v]={e.color,e.uv1,e.pivot,e.flexibility,e.cardPitchCos,e.cardPitchSin};}
            if(asset->metadata.version==2)source.tangents=mesh.tangents;
            auto geometry=renderer.UploadGeometry(source);if(!geometry)throw std::runtime_error("vegetation GPU upload failed");result->geometry.push_back(geometry);++liveGeometry;++statistics.uploads;
            if(mesh.materialBindings.size()!=1)throw std::runtime_error("vegetation part needs one material");const auto material=mesh.materialBindings[0];auto&image=result->textures.at(material);
            if(!image){image=texture(model.Get()->materials.at(material).textures[0]);if(!image)throw std::runtime_error("vegetation texture missing: "+model.Get()->materials.at(material).textures[0]);}
            if(asset->metadata.version==2&&!result->materials[material]) {
                const auto& input=model.Get()->materials[material];auto bound=std::make_shared<::Renderer::MaterialRuntimeData>();
                bound->baseColor=input.baseColorFactor;bound->model=input.model;bound->roughness=input.roughness;bound->metallic=input.metallic;
                bound->normalScale=input.normalScale;bound->occlusionStrength=input.occlusionStrength;bound->emissive=input.emissiveColor;bound->textures[0]=image;
                for(unsigned channel=1;channel<bound->textures.size();++channel)if(!input.materialTextures[channel].id.empty()) {
                    bound->textures[channel]=texture(input.materialTextures[channel].id);if(!bound->textures[channel])throw std::runtime_error("missing modern vegetation material texture");
                }
                bound->roughnessChannel=input.materialTextures[2].channel;bound->metallicChannel=input.materialTextures[3].channel;bound->occlusionChannel=input.materialTextures[4].channel;
                result->materials[material]=bound;
            }
        }
        if(!asset->metadata.shadowTexture.empty()){result->shadow=texture(asset->metadata.shadowTexture);if(!result->shadow)throw std::runtime_error("vegetation shadow texture missing");}
        return result;
    }catch(const std::exception&e){error=e.what();if(!optional)++statistics.failures;return {};}
}
namespace {
::Renderer::StaticObjectDraw PartDraw(const Instance&instance,const RenderAsset&asset,const RenderContext&context,unsigned slot,int index){
    const auto model=instance.asset->geometry.Model(0);
        const auto&part=instance.asset->metadata.parts[std::size_t(index)];const auto&mesh=model.Get()->meshes[std::size_t(index)];
        auto draw=context.state;draw.matrices={instance.transform,context.view,context.projection};draw.normalTransform=Identity;
        draw.cull=part.kind==PartKind::Branch ? ::Renderer::StaticObjectCull::Clockwise : ::Renderer::StaticObjectCull::None;
        draw.alphaTest=::Renderer::StaticObjectAlphaTest::Greater;draw.alphaReference=static_cast<unsigned>(std::clamp(instance.lod.alpha[slot],0.f,255.f));
        draw.vertexCount=mesh.vertexCount;draw.indexCount=mesh.indexCount;draw.firstIndex=draw.baseVertex=0;
        draw.cardMode=part.kind==PartKind::Leaf?1:(part.kind==PartKind::Billboard?2:0);
        draw.cardFog=part.kind==PartKind::Leaf&&draw.fog!=::Renderer::TerrainFog::None;
        // Legacy leaf cards use linear shader fog even when the world uses density
        // fog. Preserve the accepted 0 .. 2.3/density range independently of
        // the ordinary FogStart/FogEnd states, which are stale in density mode.
        if(draw.cardFog&&draw.fog==::Renderer::TerrainFog::Exp){
            const float density=draw.fogParameters[2];
            if(density>0){draw.fogParameters[0]=0;draw.fogParameters[1]=2.3f/density;}
            else draw.cardFog=false; // Zero density means no attenuation.
        }
        draw.modulateCameraAlpha=bool(draw.cameraAlpha);
        const auto&v=context.view;
        draw.cardRight={v[0],v[4],v[8],0};draw.cardForward={-v[2],-v[6],-v[10],0};draw.cardUp={v[1],v[5],v[9],0};
        if(draw.cardMode){const float len=std::hypot(draw.cardRight[0],draw.cardRight[1]);if(len>1e-6f){draw.cardRight={draw.cardRight[0]/len,draw.cardRight[1]/len,0,0};draw.cardForward={-draw.cardRight[1],draw.cardRight[0],0,0};}else{draw.cardRight={1,0,0,0};draw.cardForward={0,1,0,0};}draw.cardUp={0,0,1,0};}
        if(draw.cardMode==1){const float sine=std::clamp(v[10],-1.f,1.f);draw.cardPitch={std::sqrt(std::max(0.f,1-sine*sine))-1,sine,0,0};}
        const auto&w=instance.asset->metadata.wind;
        const float amplitude=part.kind==PartKind::Leaf?w.leafAmplitude:part.kind==PartKind::Branch?w.branchAmplitude:part.kind==PartKind::Frond?w.frondAmplitude:0;
        draw.wind={context.time+instance.phase,amplitude*w.strength*context.windStrength,w.frequency,0};
        if(!draw.cameraAlpha&&(part.kind==PartKind::Branch||part.kind==PartKind::Frond))draw.vertexShadow=asset.shadow;
        if(context.modern) {
            const auto&m=instance.asset->metadata;draw.modernVegetation=m.version==2;
            const float len=std::hypot(context.windDirection[0],context.windDirection[1]);
            draw.worldWind={len>1e-6f?context.windDirection[0]/len:0,len>1e-6f?context.windDirection[1]/len:0,0,m.bounds.max[2]-m.bounds.min[2]};
            if(m.version==2&&part.kind==PartKind::Billboard)draw.worldWind[2]=8;
            draw.branchWind={part.kind==PartKind::Billboard?0:w.branchAmplitude*w.strength*context.windStrength,0,m.bounds.min[2],0};
            draw.wind[0]=context.time;draw.wind[1]*=context.quality.windDetail;
            if(part.kind!=PartKind::Branch)draw.foliage={m.foliage.transmissionColor[0],m.foliage.transmissionColor[1],m.foliage.transmissionColor[2],m.foliage.transmissionStrength*context.quality.transmission};
            if(m.version==2) {
                draw.material=asset.materials[mesh.materialBindings[0]];
                const auto&material=model.Get()->materials[mesh.materialBindings[0]];
                draw.alphaReference=static_cast<unsigned>(std::clamp(material.alphaCutoff,0.f,1.f)*255);
                draw.alphaTest=material.alphaTest ? ::Renderer::StaticObjectAlphaTest::GreaterEqual : ::Renderer::StaticObjectAlphaTest::Disabled;
                // Authored cards are fixed geometry; only the far card faces the camera.
                if(part.kind==PartKind::Leaf)draw.cardMode=0;
            }
        }
        return draw;
}
}
bool Draw(Instance&instance,const RenderAsset&asset,::Renderer::IStaticObjectRenderer&renderer,const RenderContext&context){
    if(instance.asset!=asset.asset)return false;
    const auto previous=instance.lod.meshes;
    if(!instance.Update(context.camera,{},context.distanceScale)){++statistics.culled;return false;}
    if(previous!=instance.lod.meshes)++statistics.lodChanges;
    const auto model=instance.asset->geometry.Model(0);
    for(unsigned slot=0;slot<5;++slot){const int index=instance.lod.meshes[slot];if(index<0)continue;
        const auto&part=instance.asset->metadata.parts[std::size_t(index)];const auto&mesh=model.Get()->meshes[std::size_t(index)];
        auto draw=PartDraw(instance,asset,context,slot,index);
        renderer.Draw(asset.geometry[std::size_t(index)],asset.textures[mesh.materialBindings[0]],draw);++statistics.parts[unsigned(part.kind)];++statistics.submitted;
    }return true;
}
namespace {
template<class GetInstance>
bool DrawBatchImpl(std::size_t count,GetInstance getInstance,const RenderAsset&asset,::Renderer::IStaticObjectRenderer&renderer,const RenderContext&context) {
    const auto start=std::chrono::steady_clock::now();
    const auto planes=FrustumPlanes(context.view,context.projection);
    std::vector<std::vector<::Renderer::StaticObjectInstance>> groups(asset.geometry.size()*4);
    // Camera blockers are queued as transparent draws. Their singleton data must
    // survive subsequent blockers of the same asset until the queue is drained.
    std::vector<::Renderer::StaticObjectInstanceBufferPtr> transparentBuffers(context.state.blend?groups.size():0);
    struct Example {Matrix transform;LodState lod;float phase;};
    std::vector<Example> examples(groups.size());std::vector<unsigned> slots(groups.size());
    const auto&m=asset.asset->metadata;
    for(std::size_t i=0;i<count;++i) {
        auto*instance=getInstance(i);
        if(!instance||instance->asset!=asset.asset)return false;
        const auto previous=instance->lod.meshes;
        const auto planeSpan=context.shadowPass?std::span<const std::array<float,4>>{}:std::span<const std::array<float,4>>{planes};
        if(!instance->Update(context.camera,planeSpan,context.distanceScale)){++statistics.culled;continue;}
        float distance=0;for(unsigned k=0;k<3;++k){const float delta=context.camera[k]-instance->transform[12+k];distance+=delta*delta;}distance=std::sqrt(distance);
        if(m.plantKind==PlantKind::Grass&&distance>context.quality.grassDistance){++statistics.culled;continue;}
        const auto selected=SelectModernLOD(m,distance/context.distanceScale);instance->lod=selected.first;
        if(previous!=instance->lod.meshes)++statistics.lodChanges;++statistics.visible;
        const float grassFade=m.plantKind==PlantKind::Grass?std::clamp((distance/context.quality.grassDistance-.6f)*2.5f,0.f,1.f):0;
        const auto&t=instance->transform;
        const unsigned mirrored=t[0]*(t[5]*t[10]-t[6]*t[9])-t[1]*(t[4]*t[10]-t[6]*t[8])+t[2]*(t[4]*t[9]-t[5]*t[8])<0?1u:0u;
        for(unsigned pass=0;pass<2;++pass) {
            if(pass&&selected.transition<=0)continue;
            const auto&lod=pass?selected.second:selected.first;
            for(unsigned slot=0;slot<5;++slot)if(lod.meshes[slot]>=0) {
                const auto part=unsigned(lod.meshes[slot]),group=part*4+pass*2+mirrored;
                auto alpha=lod.alpha[slot];
                if(m.version==2){const auto&model=*asset.asset->geometry.Model(0).Get();alpha=model.materials[model.meshes[part].materialBindings[0]].alphaCutoff*255;}
                groups[group].push_back({instance->transform,{instance->phase,pass?-selected.transition:selected.transition,alpha,1-grassFade}});
                examples[group]={instance->transform,instance->lod,instance->phase};slots[group]=slot;
            }
        }
    }
    for(unsigned group=0;group<groups.size();++group) {
        auto&buffer=context.state.blend?transparentBuffers[group]:asset.instanceBuffers[group+(context.shadowPass?groups.size():0)];
        if(groups[group].empty()){buffer.reset();continue;}
        if(!renderer.UpdateInstances(buffer,groups[group])){++statistics.failures;return false;}
        const auto&data=examples[group];Instance example(asset.asset,data.transform);example.lod=data.lod;example.phase=data.phase;
        const unsigned part=group/4;auto draw=PartDraw(example,asset,context,slots[group],part);
        draw.instances=buffer;draw.instanceCount=static_cast<unsigned>(groups[group].size());
        const auto&mesh=asset.asset->geometry.Model(0).Get()->meshes[part];
        renderer.Draw(asset.geometry[part],asset.textures[mesh.materialBindings[0]],draw);
        ++statistics.submitted;++statistics.batches;statistics.parts[unsigned(m.parts[part].kind)]+=groups[group].size();
        statistics.triangles+=std::uint64_t(mesh.indexCount/3)*groups[group].size();
    }
    statistics.cpuMilliseconds+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();return true;
}
}
bool DrawBatch(std::span<Instance* const> instances,const RenderAsset&asset,::Renderer::IStaticObjectRenderer&renderer,const RenderContext&context) {
    if(!context.modern){for(auto*instance:instances)Draw(*instance,asset,renderer,context);return true;}
    return DrawBatchImpl(instances.size(),[&](std::size_t i){return instances[i];},asset,renderer,context);
}
bool DrawGrassBatch(std::span<const GrassPlacement* const> placements,const RenderAsset&asset,::Renderer::IStaticObjectRenderer&renderer,const RenderContext&context) {
    if(!context.modern)return true;
    // One transient draw view over immutable placements, not a new world object
    // for each blade. Transform, phase and density were fixed at map load.
    Instance instance(asset.asset,Identity);
    return DrawBatchImpl(placements.size(),[&](std::size_t i){
        instance.transform=placements[i]->Transform();instance.phase=placements[i]->phase;instance.lod={};return &instance;
    },asset,renderer,context);
}
}
