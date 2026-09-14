#include "SkinningDataAdapter.h"
#include "AssetRuntime/Granny/GrannyInterop.h"
#include "AssetRuntime/Granny/Native.h"
#include <cstring>
#include <limits>

namespace SkinningDataAdapter
{
using namespace Renderer;
static_assert(sizeof(granny_pwnt3432_vertex)==sizeof(SkinningVertex));
static_assert(offsetof(granny_pwnt3432_vertex,BoneWeights)==offsetof(SkinningVertex,weights));
static_assert(offsetof(granny_pwnt3432_vertex,BoneIndices)==offsetof(SkinningVertex,indices));
static_assert(offsetof(granny_pwnt3432_vertex,Normal)==offsetof(SkinningVertex,normal));
static_assert(offsetof(granny_pwnt3432_vertex,UV)==offsetof(SkinningVertex,uv));

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

std::shared_ptr<const SkinningModelData> Extract(const granny_model& model, const AssetRuntime::ModelAsset* asset)
{
    auto result=std::make_shared<SkinningModelData>();
    auto skeleton=std::make_shared<SkeletonLayout>();
    if(asset && asset->skeleton) {
        for(const auto& bone:asset->skeleton->bones) {
            skeleton->names.push_back(bone.name);
            skeleton->parents.push_back(bone.parentIndex);
        }
    } else if(model.Skeleton && model.Skeleton->BoneCount>0 && model.Skeleton->Bones) {
        for(int b=0;b<model.Skeleton->BoneCount;++b) {
            const auto& bone=model.Skeleton->Bones[b];
            skeleton->names.emplace_back(bone.Name?bone.Name:"");
            skeleton->parents.push_back(bone.ParentIndex);
        }
    }
    result->skeleton=skeleton;
    if(model.MeshBindingCount<=0 || !model.MeshBindings) return result;
    result->meshes.resize(model.MeshBindingCount);
    result->status.resize(model.MeshBindingCount,SkinDataStatus::MissingData);
    uint64_t deformOffset=0;
    for(int m=0;m<model.MeshBindingCount;++m) {
        const auto* source=model.MeshBindings[m].Mesh;
        auto& status=result->status[m];
        if(!source || !source->PrimaryVertexData) continue;
        const int count=GrannyGetMeshVertexCount(source);
        if(count<=0) { status=SkinDataStatus::Empty; continue; }
        const auto* type=GrannyGetMeshVertexType(source);
        if(!type) { status=SkinDataStatus::UnsupportedLayout; continue; }
        if(GrannyMeshIsRigid(source)) { status=SkinDataStatus::Rigid; continue; }
        // ZiiNAN: 64-bit safety cleanup
        if(static_cast<uint64_t>(count)>std::numeric_limits<uint32_t>::max()-deformOffset) {
            status=SkinDataStatus::InvalidTopology; break;
        }
        const uint32_t offset=static_cast<uint32_t>(deformOffset);
        deformOffset+=static_cast<uint32_t>(count);
        if(skeleton->names.empty()) { status=SkinDataStatus::InvalidSkeleton; continue; }
        if(skeleton->names.size()>preparedBoneLimit) { status=SkinDataStatus::PaletteTooLarge; continue; }
        if(source->BoneBindingCount<=0) { status=SkinDataStatus::InvalidSkeleton; continue; }
        if(!GrannyDataTypesAreEqualWithNames(type,GrannyPWNT3432VertexType)) {
            status=SkinDataStatus::UnsupportedLayout; continue;
        }
        const auto* vertices=GrannyGetMeshVertices(source);
        if(!vertices) continue;
        auto data=std::make_shared<StaticSkinnedMeshData>();
        data->name=source->Name?source->Name:"";
        data->meshIndex=static_cast<uint32_t>(m); data->deformVertexOffset=offset;
        data->meshBoneCount=static_cast<uint32_t>(source->BoneBindingCount);
        data->vertices.resize(count);
        std::memcpy(data->vertices.data(),vertices,count*sizeof(SkinningVertex));
        status=ValidateSkinVertices(data->vertices,data->meshBoneCount,data->diagnostics);
        if(status!=SkinDataStatus::Ready) continue;
        if(!source->BoneBindings) { status=SkinDataStatus::InvalidRemap; continue; }
        auto* selfBinding=GrannyNewMeshBinding(source,model.Skeleton,model.Skeleton);
        const auto* sourceIndices=selfBinding?GrannyGetMeshBindingToBoneIndices(selfBinding):nullptr;
        bool sourceMappingValid=sourceIndices && GrannyGetMeshBindingBoneCount(selfBinding)==data->meshBoneCount;
        if(sourceMappingValid) for(size_t bone=0;bone<data->meshBoneCount;++bone) {
            if(sourceIndices[bone]<0 || size_t(sourceIndices[bone])>=skeleton->names.size()) sourceMappingValid=false;
            else data->meshToSourceSkeleton.push_back(static_cast<uint16_t>(sourceIndices[bone]));
        }
        if(selfBinding) GrannyFreeMeshBinding(selfBinding);
        if(!sourceMappingValid) { status=SkinDataStatus::InvalidRemap; continue; }
        if(!source->PrimaryTopology) { status=SkinDataStatus::InvalidTopology; continue; }
        const int indexCount=GrannyGetMeshIndexCount(source);
        const int groupCount=GrannyGetMeshTriangleGroupCount(source);
        const auto* groups=GrannyGetMeshTriangleGroups(source);
        if(indexCount<=0 || indexCount%3 || groupCount<=0 || !groups) {
            status=SkinDataStatus::InvalidTopology; continue;
        }
        // Validate before narrowing to the existing 16-bit production index format.
        std::vector<int32_t> indices(indexCount);
        GrannyCopyMeshIndices(source,4,indices.data());
        bool valid=true;
        for(int index:indices) if(index<0 || index>=count || index>std::numeric_limits<uint16_t>::max()) valid=false;
        for(int g=0;g<groupCount;++g) {
            const auto& group=groups[g];
            const int64_t first=int64_t(group.TriFirst)*3, length=int64_t(group.TriCount)*3;
            if(first<0 || length<0 || first+length>indexCount || group.MaterialIndex<0 ||
               group.MaterialIndex>=source->MaterialBindingCount) valid=false;
            else data->groups.push_back({uint32_t(group.MaterialIndex),uint32_t(first),uint32_t(length)});
        }
        if(!valid) { status=SkinDataStatus::InvalidTopology; continue; }
        data->indices.assign(indices.begin(),indices.end());
        result->meshes[m]=std::move(data);
    }
    return result;
}

std::shared_ptr<const BoneRemap> ExtractRemap(const StaticSkinnedMeshData& mesh,
    const granny_mesh_binding* binding, std::shared_ptr<const SkeletonLayout> destination, SkinDataStatus& status)
{
    status=SkinDataStatus::InvalidRemap;
    if(!binding || GrannyGetMeshBindingBoneCount(binding)!=mesh.meshBoneCount) return {};
    const auto* indices=GrannyGetMeshBindingToBoneIndices(binding);
    if(!indices) return {};
    return AcquireBoneRemap(mesh,std::move(destination),{indices,mesh.meshBoneCount},status);
}

SkinDataStatus CapturePose(BonePalette& palette, std::shared_ptr<const SkeletonLayout> skeleton,
    const granny_world_pose* pose)
{
    return CapturePose(palette,std::move(skeleton),AssetRuntime::GrannyInterop::GetPoseView(pose));
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
    // Copy values across the SDK boundary; never alias an SDK array or retain its pointer.
    std::array<SkinningMatrix,preparedBoneLimit> copy;
    std::memcpy(copy.data(),matrices,count*sizeof(SkinningMatrix));
    return CaptureBonePalette(palette,std::move(skeleton),{copy.data(),static_cast<size_t>(count)});
}
}
