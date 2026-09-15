#pragma once
#include "VegetationRuntime.h"
#include "Renderer/StaticObjectRenderData.h"

namespace Vegetation {
inline std::atomic_size_t liveRenderAssets{},liveGeometry{};
struct Statistics {std::uint64_t loaded{},created{},submitted{},culled{},uploads{},failures{},lodChanges{};std::array<std::uint64_t,4> parts{};};
inline Statistics statistics;
struct RenderAsset {
    AssetPtr asset;
    std::vector<::Renderer::StaticObjectGeometryPtr> geometry;
    std::vector<::Renderer::TerrainTexturePtr> textures;
    ::Renderer::TerrainTexturePtr shadow;
    RenderAsset(){++liveRenderAssets;}~RenderAsset(){--liveRenderAssets;for(const auto&g:geometry)if(g)--liveGeometry;}
    RenderAsset(const RenderAsset&)=delete;RenderAsset&operator=(const RenderAsset&)=delete;
};
using TextureResolver=std::function<::Renderer::TerrainTexturePtr(std::string_view)>;
std::shared_ptr<const RenderAsset> Prepare(AssetPtr,::Renderer::IStaticObjectRenderer&,const TextureResolver&,std::string&error);
struct RenderContext {
    Matrix view{Identity},projection{Identity};Vec3 camera{};
    float time{},windStrength{1};;
    float distanceScale{1.f};
    ::Renderer::StaticObjectDraw state;
};
bool Draw(Instance&,const RenderAsset&,::Renderer::IStaticObjectRenderer&,const RenderContext&);
}
