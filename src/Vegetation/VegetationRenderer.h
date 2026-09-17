#pragma once
#include "VegetationRuntime.h"
#include "Renderer/StaticObjectRenderData.h"

namespace Vegetation {
inline std::atomic_size_t liveRenderAssets{},liveGeometry{};
struct Statistics {std::uint64_t loaded{},created{},submitted{},culled{},uploads{},failures{},lodChanges{},visible{},triangles{},batches{};double cpuMilliseconds{};std::array<std::uint64_t,4> parts{};};
inline Statistics statistics;
struct GrassPreparation {std::uint64_t placements{},cells{},tiles{},expectedTiles{},bytes{},serial{};double milliseconds{};};
inline GrassPreparation grassPreparation;
inline float developmentSeconds{-1}; // Only the optional native diagnostics endpoint writes this.
struct RenderAsset {
    AssetPtr asset;
    std::vector<::Renderer::StaticObjectGeometryPtr> geometry;
    std::vector<::Renderer::TerrainTexturePtr> textures;
    ::Renderer::TerrainTexturePtr shadow;
    std::vector<std::shared_ptr<const ::Renderer::MaterialRuntimeData>> materials;
    mutable std::vector<::Renderer::StaticObjectInstanceBufferPtr> instanceBuffers;
    RenderAsset(){++liveRenderAssets;}~RenderAsset(){--liveRenderAssets;for(const auto&g:geometry)if(g)--liveGeometry;}
    RenderAsset(const RenderAsset&)=delete;RenderAsset&operator=(const RenderAsset&)=delete;
};
using TextureResolver=std::function<::Renderer::TerrainTexturePtr(std::string_view)>;
std::shared_ptr<const RenderAsset> Prepare(AssetPtr,::Renderer::IStaticObjectRenderer&,const TextureResolver&,std::string&error,bool optional=false);
struct RenderContext {
    Matrix view{Identity},projection{Identity};Vec3 camera{};
    float time{},windStrength{1};
    float lodTime{}; // Frame clock, independent of the diagnostic frozen wind.
    float distanceScale{1.f};
    Vec3 windDirection{1,0,0};
    Quality quality;
    bool modern{},shadowPass{};
    bool fixedTreeDetail{true}; // Private fixed-high tree experiment; adaptive path retained for comparison.
    ::Renderer::StaticObjectDraw state;
};
bool Draw(Instance&,const RenderAsset&,::Renderer::IStaticObjectRenderer&,const RenderContext&);
bool DrawBatch(std::span<Instance* const>,const RenderAsset&,::Renderer::IStaticObjectRenderer&,const RenderContext&);
bool DrawGrassBatch(std::span<const GrassPlacement* const>,const RenderAsset&,::Renderer::IStaticObjectRenderer&,const RenderContext&);
}
