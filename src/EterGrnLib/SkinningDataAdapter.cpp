#include "SkinningDataAdapter.h"
#include <cstring>
#include <limits>

namespace SkinningDataAdapter
{
using namespace Renderer;

std::shared_ptr<const SkinningModelData> Extract(const AssetRuntime::ModelHandle& handle)
{
    auto result=std::make_shared<SkinningModelData>();
    auto skeleton=std::make_shared<SkeletonLayout>();
    result->skeleton=skeleton;
    const auto* model=handle.Get();
    if(!model) return result;
    if(model->skeleton) for(const auto& bone:model->skeleton->bones) {
        skeleton->names.push_back(bone.name);
        skeleton->parents.push_back(bone.parentIndex);
    }
    result->meshes.resize(model->meshes.size());
    result->status.resize(model->meshes.size(),SkinDataStatus::MissingData);
    uint64_t deformOffset=0;
    for(size_t m=0;m<model->meshes.size();++m) {
        const auto& source=model->meshes[m];
        auto& status=result->status[m];
        if(!source.vertexCount) { status=SkinDataStatus::Empty; continue; }
        if(source.deformation==AssetRuntime::Deformation::Rigid) { status=SkinDataStatus::Rigid; continue; }
        if(source.vertexCount>std::numeric_limits<uint32_t>::max()-deformOffset) {
            status=SkinDataStatus::InvalidTopology; break;
        }
        const auto offset=static_cast<uint32_t>(deformOffset);
        deformOffset+=source.vertexCount;
        if(skeleton->names.empty()) { status=SkinDataStatus::InvalidSkeleton; continue; }
        if(skeleton->names.size()>preparedBoneLimit) { status=SkinDataStatus::PaletteTooLarge; continue; }
        if(source.skin.boneNames.empty()) { status=SkinDataStatus::InvalidSkeleton; continue; }
        if(source.vertexLayout!=AssetRuntime::VertexLayout::WeightedPositionNormalUV) {
            status=SkinDataStatus::UnsupportedLayout; continue;
        }
        auto data=std::make_shared<StaticSkinnedMeshData>();
        data->name=source.name;
        data->meshIndex=static_cast<uint32_t>(m); data->deformVertexOffset=offset;
        data->meshBoneCount=static_cast<uint32_t>(source.skin.boneNames.size());
        data->vertices.resize(source.vertexCount);
        if(handle.GetDocument()->CopyVertices(handle.Index(),m,source.vertexLayout,
            std::as_writable_bytes(std::span(data->vertices)))!=AssetRuntime::AssetError::None) continue;
        status=ValidateSkinVertices(data->vertices,data->meshBoneCount,data->diagnostics);
        if(status!=SkinDataStatus::Ready) continue;
        bool valid=source.skin.validRemap && source.skin.meshToSkeleton.size()==data->meshBoneCount;
        for(const auto bone:source.skin.meshToSkeleton) {
            if(bone<0 || size_t(bone)>=skeleton->names.size()) valid=false;
            else data->meshToSourceSkeleton.push_back(static_cast<uint16_t>(bone));
        }
        if(!valid) { status=SkinDataStatus::InvalidRemap; continue; }
        if(!source.indexCount || source.indexCount%3 || source.materialGroups.empty()) {
            status=SkinDataStatus::InvalidTopology; continue;
        }
        std::vector<int32_t> indices(source.indexCount);
        if(handle.GetDocument()->CopyIndices(handle.Index(),m,AssetRuntime::IndexWidth::UInt32,
            std::as_writable_bytes(std::span(indices)))!=AssetRuntime::AssetError::None) {
            status=SkinDataStatus::InvalidTopology; continue;
        }
        for(const auto index:indices)
            if(index<0 || size_t(index)>=source.vertexCount || index>std::numeric_limits<uint16_t>::max()) valid=false;
        for(const auto& group:source.materialGroups) {
            if(uint64_t(group.firstIndex)+group.indexCount>source.indexCount ||
                group.materialIndex>=source.materialBindings.size()) valid=false;
            else data->groups.push_back({group.materialIndex,group.firstIndex,group.indexCount});
        }
        if(!valid) { status=SkinDataStatus::InvalidTopology; continue; }
        data->indices.assign(indices.begin(),indices.end());
        result->meshes[m]=std::move(data);
    }
    return result;
}

std::shared_ptr<const BoneRemap> ExtractRemap(const StaticSkinnedMeshData& mesh,
    std::span<const std::int32_t> mapping, std::shared_ptr<const SkeletonLayout> destination, SkinDataStatus& status)
{
    return AcquireBoneRemap(mesh,std::move(destination),mapping,status);
}

SkinDataStatus CapturePose(BonePalette& palette, std::shared_ptr<const SkeletonLayout> skeleton,
    AssetRuntime::PoseView view)
{
    palette.ready=false;
    if(!view.Valid()) return SkinDataStatus::MissingData;
    const auto count=view.BoneCount();
    if(count<=0) return SkinDataStatus::InvalidSkeleton;
    if(count>preparedBoneLimit) return SkinDataStatus::PaletteTooLarge;
    const auto* matrices=view.values.data();
    if(!matrices) return SkinDataStatus::MissingData;
    // Copy the borrowed pose values; never retain the provider array pointer.
    std::array<SkinningMatrix,preparedBoneLimit> copy;
    std::memcpy(copy.data(),matrices,count*sizeof(SkinningMatrix));
    return CaptureBonePalette(palette,std::move(skeleton),{copy.data(),static_cast<size_t>(count)});
}
}
