#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <vector>
#include <algorithm>

namespace Renderer
{
// ZiiNAN: GPU skinning static mesh data
enum class SkinningMode { CPU, GPU };
inline constexpr SkinningMode productionSkinningMode = SkinningMode::CPU;
inline constexpr size_t preparedBoneLimit = 256; // Preparation limit, not an asset/runtime limit.
inline std::atomic_size_t liveSkinMeshes{}, liveBoneRemaps{}, liveBonePalettes{};
inline std::atomic_uint64_t nextSkeletonIdentity{1}, skinPaletteUpdates{}, skinSidecarFailures{};
// ZiiNAN: GPU skinning actor coverage
inline std::atomic_uint64_t nextPaletteIdentity{1};

struct SkinningVertex
{
    float position[3];
    uint8_t weights[4];
    uint8_t indices[4]; // Mesh-local; NEVER a skeleton index until BoneRemap is applied.
    float normal[3];
    float uv[2];
};
static_assert(sizeof(SkinningVertex)==40 && alignof(SkinningVertex)==4);
static_assert(offsetof(SkinningVertex, weights)==12 && offsetof(SkinningVertex, indices)==16);
static_assert(offsetof(SkinningVertex, normal)==20 && offsetof(SkinningVertex, uv)==32);

enum class SkinDataStatus
{
    Ready, Rigid, Empty, UnsupportedLayout, InvalidSkeleton, PaletteTooLarge,
    MissingData, NonFiniteAttribute, ZeroWeights, WeightSum, InvalidBoneIndex,
    InvalidTopology, InvalidRemap, DestinationChanged, InvalidMatrix
};
inline const char* SkinDataStatusName(SkinDataStatus value)
{
    switch(value) {
#define M2_SKIN_STATUS(v) case SkinDataStatus::v: return #v
        M2_SKIN_STATUS(Ready); M2_SKIN_STATUS(Rigid); M2_SKIN_STATUS(Empty);
        M2_SKIN_STATUS(UnsupportedLayout); M2_SKIN_STATUS(InvalidSkeleton);
        M2_SKIN_STATUS(PaletteTooLarge); M2_SKIN_STATUS(MissingData);
        M2_SKIN_STATUS(NonFiniteAttribute); M2_SKIN_STATUS(ZeroWeights);
        M2_SKIN_STATUS(WeightSum); M2_SKIN_STATUS(InvalidBoneIndex);
        M2_SKIN_STATUS(InvalidTopology); M2_SKIN_STATUS(InvalidRemap);
        M2_SKIN_STATUS(DestinationChanged); M2_SKIN_STATUS(InvalidMatrix);
#undef M2_SKIN_STATUS
    }
    return "Unknown";
}
struct SkinDataDiagnostics
{
    size_t exactWeightSums{}, floatWeightRoundoff{}, zeroWeights{}, weightSumAnomalies{};
    size_t invalidIndices{}, nonFiniteAttributes{}, ignoredZeroWeightIndices{};
    std::array<size_t,5> influences{};
    uint8_t maxInfluences{};
};
struct SkeletonLayout
{
    const uint64_t identity = nextSkeletonIdentity.fetch_add(1);
    std::vector<std::string> names;
    std::vector<int32_t> parents;
};
struct SkinMaterialGroup
{
    uint32_t materialIndex{}, firstIndex{}, indexCount{}; // Original mesh-local material slot/range.
};
struct BoneRemap
{
    BoneRemap() { ++liveBoneRemaps; }
    ~BoneRemap() { --liveBoneRemaps; }
    BoneRemap(const BoneRemap&) = delete;
    BoneRemap& operator=(const BoneRemap&) = delete;
    std::shared_ptr<const SkeletonLayout> destination;
    // Full-skeleton palette: palette index == destination skeleton index.
    std::vector<uint16_t> meshToSkeleton;
};
struct StaticSkinnedMeshData
{
    StaticSkinnedMeshData() { ++liveSkinMeshes; }
    ~StaticSkinnedMeshData() { --liveSkinMeshes; }
    StaticSkinnedMeshData(const StaticSkinnedMeshData&) = delete;
    StaticSkinnedMeshData& operator=(const StaticSkinnedMeshData&) = delete;
    std::string name;
    uint32_t meshIndex{}, deformVertexOffset{}, meshBoneCount{};
    std::vector<SkinningVertex> vertices;
    std::vector<uint16_t> meshToSourceSkeleton; // Asset's self binding; linked draws use BoneRemap instead.
    std::vector<uint16_t> indices;
    std::vector<SkinMaterialGroup> groups;
    SkinDataDiagnostics diagnostics;
    // Only accessed on the existing model/update thread; expired entries are pruned on acquisition.
    mutable std::vector<std::weak_ptr<const BoneRemap>> remapCache;
};
struct SkinningModelData
{
    std::shared_ptr<const SkeletonLayout> skeleton;
    std::vector<std::shared_ptr<const StaticSkinnedMeshData>> meshes; // Same slots as source model.
    std::vector<SkinDataStatus> status;
    bool HasSkinnedMeshes() const
    {
        return std::any_of(meshes.begin(), meshes.end(), [](const auto& mesh){ return bool(mesh); });
    }
};

// Row vectors: p' = (p,1) * composite; n' = (n,0) * composite. No transpose/normalization.
using SkinningMatrix = std::array<float,16>;
static_assert(sizeof(SkinningMatrix)==64);
struct BonePalette
{
    const uint64_t identity=nextPaletteIdentity.fetch_add(1);
    BonePalette() { ++liveBonePalettes; }
    ~BonePalette() { --liveBonePalettes; }
    BonePalette(const BonePalette&) = delete;
    BonePalette& operator=(const BonePalette&) = delete;
    std::shared_ptr<const SkeletonLayout> skeleton;
    std::vector<SkinningMatrix> matrices;
    uint64_t revision{};
    bool ready{};
};

inline SkinDataStatus ValidateSkinVertices(std::span<const SkinningVertex> vertices,
    size_t meshBones, SkinDataDiagnostics& diagnostics)
{
    diagnostics={};
    if(vertices.empty()) return SkinDataStatus::Empty;
    if(!meshBones) return SkinDataStatus::InvalidSkeleton;
    if(meshBones>preparedBoneLimit) return SkinDataStatus::PaletteTooLarge;
    for(const auto& vertex:vertices) {
        unsigned sum=0, influences=0; float normalizedSum=0;
        for(int i=0;i<4;++i) {
            sum+=vertex.weights[i]; normalizedSum+=vertex.weights[i]*(1.f/255.f);
            if(vertex.weights[i]) { ++influences; diagnostics.invalidIndices+=vertex.indices[i]>=meshBones; }
            else diagnostics.ignoredZeroWeightIndices+=vertex.indices[i]>=meshBones;
        }
        ++diagnostics.influences[influences];
        diagnostics.maxInfluences=std::max(diagnostics.maxInfluences,static_cast<uint8_t>(influences));
        diagnostics.exactWeightSums+=sum==255;
        diagnostics.floatWeightRoundoff+=sum==255 && normalizedSum!=1.f;
        diagnostics.zeroWeights+=sum==0;
        diagnostics.weightSumAnomalies+=sum!=255;
        for(float v:vertex.position) diagnostics.nonFiniteAttributes+=!std::isfinite(v);
        for(float v:vertex.normal) diagnostics.nonFiniteAttributes+=!std::isfinite(v);
        for(float v:vertex.uv) diagnostics.nonFiniteAttributes+=!std::isfinite(v);
    }
    if(diagnostics.nonFiniteAttributes) return SkinDataStatus::NonFiniteAttribute;
    if(diagnostics.invalidIndices) return SkinDataStatus::InvalidBoneIndex;
    if(diagnostics.zeroWeights) return SkinDataStatus::ZeroWeights;
    if(diagnostics.weightSumAnomalies) return SkinDataStatus::WeightSum;
    return SkinDataStatus::Ready;
}

inline std::shared_ptr<const BoneRemap> AcquireBoneRemap(const StaticSkinnedMeshData& mesh,
    std::shared_ptr<const SkeletonLayout> destination, std::span<const int> mapping, SkinDataStatus& status)
{
    status=SkinDataStatus::InvalidRemap;
    if(!destination || destination->names.empty() || mapping.size()!=mesh.meshBoneCount) return {};
    if(destination->names.size()>preparedBoneLimit) { status=SkinDataStatus::PaletteTooLarge; return {}; }
    for(int bone:mapping) if(bone<0 || static_cast<size_t>(bone)>=destination->names.size()) return {};
    auto& cache=mesh.remapCache;
    std::erase_if(cache,[](const auto& entry){ return entry.expired(); });
    for(const auto& entry:cache) if(auto cached=entry.lock())
        if(cached->destination->identity==destination->identity &&
           std::equal(mapping.begin(),mapping.end(),cached->meshToSkeleton.begin(),cached->meshToSkeleton.end())) {
            status=SkinDataStatus::Ready; return cached;
        }
    auto result=std::make_shared<BoneRemap>();
    result->destination=std::move(destination);
    result->meshToSkeleton.assign(mapping.begin(),mapping.end());
    cache.emplace_back(result); status=SkinDataStatus::Ready; return result;
}

inline SkinDataStatus CaptureBonePalette(BonePalette& palette,
    std::shared_ptr<const SkeletonLayout> skeleton, std::span<const SkinningMatrix> matrices)
{
    palette.ready=false;
    if(!skeleton || matrices.empty() || skeleton->names.size()!=matrices.size()) return SkinDataStatus::InvalidSkeleton;
    if(matrices.size()>preparedBoneLimit) return SkinDataStatus::PaletteTooLarge;
    for(const auto& matrix:matrices) for(float value:matrix)
        if(!std::isfinite(value)) return SkinDataStatus::InvalidMatrix;
    palette.skeleton=std::move(skeleton);
    palette.matrices.assign(matrices.begin(),matrices.end());
    ++palette.revision; ++skinPaletteUpdates; palette.ready=true;
    return SkinDataStatus::Ready;
}
}
