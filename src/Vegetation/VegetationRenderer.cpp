#include "VegetationRenderer.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace Vegetation {
std::shared_ptr<const RenderAsset> Prepare(AssetPtr asset,::Renderer::IStaticObjectRenderer&renderer,const TextureResolver&texture,std::string&error){
    try{
        if(!asset)throw std::runtime_error("missing vegetation asset");auto result=std::make_shared<RenderAsset>();result->asset=asset;
        const auto model=asset->geometry.Model(0);result->textures.resize(model.Get()->materials.size());
        for(const auto&part:asset->metadata.parts){const auto&mesh=model.Get()->meshes[part.mesh];::Renderer::StaticObjectSource source;source.vertices.resize(mesh.vertexCount);source.indices32.resize(mesh.indexCount);
            if(asset->geometry.Get()->CopyVertices(0,part.mesh,AssetRuntime::VertexLayout::PositionNormalUV,std::as_writable_bytes(std::span(source.vertices)))!=AssetRuntime::AssetError::None||
               asset->geometry.Get()->CopyIndices(0,part.mesh,AssetRuntime::IndexWidth::UInt32,std::as_writable_bytes(std::span(source.indices32)))!=AssetRuntime::AssetError::None)throw std::runtime_error("vegetation mesh upload data unavailable");
            if(mesh.vertexExtras.size()!=mesh.vertexCount)throw std::runtime_error("vegetation GLB is missing auxiliary channels");source.vertexExtras.resize(mesh.vertexCount);
            for(std::size_t v=0;v<source.vertexExtras.size();++v){const auto&e=mesh.vertexExtras[v];source.vertexExtras[v]={e.color,e.uv1,e.pivot,e.flexibility,e.cardPitchCos,e.cardPitchSin};}
            auto geometry=renderer.UploadGeometry(source);if(!geometry)throw std::runtime_error("vegetation GPU upload failed");result->geometry.push_back(geometry);++liveGeometry;++statistics.uploads;
            if(mesh.materialBindings.size()!=1)throw std::runtime_error("vegetation part needs one material");const auto material=mesh.materialBindings[0];auto&image=result->textures.at(material);
            if(!image){image=texture(model.Get()->materials.at(material).textures[0]);if(!image)throw std::runtime_error("vegetation texture missing: "+model.Get()->materials.at(material).textures[0]);}
        }
        if(!asset->metadata.shadowTexture.empty()){result->shadow=texture(asset->metadata.shadowTexture);if(!result->shadow)throw std::runtime_error("vegetation shadow texture missing");}
        return result;
    }catch(const std::exception&e){error=e.what();++statistics.failures;return {};}
}
bool Draw(Instance&instance,const RenderAsset&asset,::Renderer::IStaticObjectRenderer&renderer,const RenderContext&context){
    if(instance.asset!=asset.asset)return false;
    const auto previous=instance.lod.meshes;
    if(!instance.Update(context.camera,{},context.distanceScale)){++statistics.culled;return false;}
    if(previous!=instance.lod.meshes)++statistics.lodChanges;
    const auto model=instance.asset->geometry.Model(0);
    for(unsigned slot=0;slot<5;++slot){const int index=instance.lod.meshes[slot];if(index<0)continue;
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
        renderer.Draw(asset.geometry[std::size_t(index)],asset.textures[mesh.materialBindings[0]],draw);++statistics.parts[unsigned(part.kind)];++statistics.submitted;
    }return true;
}
}
