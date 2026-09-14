#pragma once
#include "GR2Types.h"
#include "AssetRuntime/AssetRuntime.h"
#include "AnimationRuntime/AnimationRuntime.h"

namespace AssetRuntime::GR2
{
struct Vertex
{
    std::array<float,3> position{};
    std::array<std::uint8_t,4> weights{255,0,0,0}, joints{};
    std::array<float,3> normal{};
    std::array<float,2> uv{}, uv1{};
};
struct MeshData { std::vector<Vertex> vertices; std::vector<std::uint32_t> indices; };
struct Curve { std::uint32_t degree{}, dimension{}; std::vector<float> knots, controls; };
struct TransformTrack { std::string name; Curve translation, rotation, scale; };
struct PeriodicLoop
{
    float radius{},dAngle{},dZ{};
    std::array<float,3> basisX{},basisY{},axis{};
};
struct RootMotion
{
    std::array<float,3> velocity{};
    std::optional<PeriodicLoop> periodic;
    bool Delta(float elapsed,std::array<float,3>& translation,std::array<float,3>& rotation) const;
};
struct TrackGroup
{
    std::string name;
    LocalTransform initialPlacement;
    std::int32_t accumulationFlags{};
    std::array<float,3> loopTranslation{};
    std::optional<PeriodicLoop> periodicLoop;
    std::vector<TransformTrack> tracks;
};
struct AnimationData { std::vector<TrackGroup> groups; };
const TransformTrack* FindTransformTrack(const TrackGroup&,std::string_view name);
int CompareTrackNames(std::string_view,std::string_view) noexcept;
struct ModelData
{
    LocalTransform initialPlacement;
    std::vector<MeshData> meshes;
    std::shared_ptr<const AnimationRuntime::RuntimeSkeleton> skeleton;
};
struct Contents
{
    std::vector<ModelAsset> models;
    std::vector<ModelData> modelData;
    std::vector<AnimationAsset> animations;
    std::vector<AnimationData> animationData;
    std::map<std::string,std::size_t> vertexFormats, curveFormats;
};
Contents Read(const File& file);
LocalTransform ReadTransform(const File& file, Ref at);
SkeletonAsset ReadSkeleton(Types&, Object);
MaterialAsset ReadMaterial(Types&, Object);
MeshData ReadMesh(Types&, Object, MeshAsset&, ModelAsset&, std::map<Ref,std::uint32_t>&, Contents&);
AnimationData ReadAnimation(Types&, Object, AnimationAsset&, Contents&);
AnimationRuntime::LocalTransform RuntimeTransform(const LocalTransform&);
std::shared_ptr<const AnimationRuntime::RuntimeAnimationClip> BindAnimation(
    const AnimationAsset&, const AnimationData&, const AnimationRuntime::RuntimeSkeleton&, std::string& error,unsigned boundary=3,std::string_view modelName={});
}
