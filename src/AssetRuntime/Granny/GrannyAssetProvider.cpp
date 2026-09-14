#include "GrannyAssetProvider.h"
#include "GrannyInterop.h"
#include "EterGrnLib/Deform.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <utility>

namespace AssetRuntime
{
namespace
{
const char* Text(const char* value) { return value ? value : ""; }
struct FileDeleter { void operator()(granny_file* file) const { if (file) GrannyFreeFile(file); } };
using FileOwner = std::unique_ptr<granny_file, FileDeleter>;

granny_data_type_definition dualUVType[] = {
    {GrannyReal32Member, GrannyVertexPositionName, nullptr, 3},
    {GrannyReal32Member, GrannyVertexNormalName, nullptr, 3},
    {GrannyReal32Member, GrannyVertexTextureCoordinatesName "0", nullptr, 2},
    {GrannyReal32Member, GrannyVertexTextureCoordinatesName "1", nullptr, 2},
    {GrannyEndMember}
};
granny_data_type_definition* NativeLayout(VertexLayout layout)
{
    switch (layout) {
    case VertexLayout::PositionNormalUV: return GrannyPNT332VertexType;
    case VertexLayout::PositionNormalUV2: return dualUVType;
    case VertexLayout::WeightedPositionNormalUV: return GrannyPWNT3432VertexType;
    default: return nullptr;
    }
}
VertexLayout DescribeLayout(const granny_data_type_definition* type)
{
    if (!type) return VertexLayout::Unknown;
    for (auto layout : {VertexLayout::PositionNormalUV, VertexLayout::PositionNormalUV2,
            VertexLayout::WeightedPositionNormalUV})
        if (GrannyDataTypesAreEqualWithNames(type, NativeLayout(layout))) return layout;
    return VertexLayout::Unknown;
}
std::uint32_t DescribeVertexAttributes(const granny_data_type_definition* type)
{
    std::uint32_t attributes = 0;
    if (!type) return attributes;
    for (const auto* member = type; member->Type != GrannyEndMember; ++member) {
        if (!member->Name) continue;
        if (std::strcmp(member->Name, GrannyVertexPositionName) == 0) attributes |= Position;
        else if (std::strcmp(member->Name, GrannyVertexNormalName) == 0) attributes |= Normal;
        else if (std::strcmp(member->Name, GrannyVertexTextureCoordinatesName "0") == 0) attributes |= UV0;
        else if (std::strcmp(member->Name, GrannyVertexTextureCoordinatesName "1") == 0) attributes |= UV1;
    }
    return attributes;
}
Bounds DescribeBounds(const granny_mesh& mesh)
{
    Bounds bounds;
    const auto* type = GrannyGetMeshVertexType(&mesh);
    const auto* vertices = static_cast<const std::byte*>(GrannyGetMeshVertices(&mesh));
    const auto count = GrannyGetMeshVertexCount(&mesh);
    if (!type || !vertices || count <= 0) return bounds;
    const int stride = GrannyGetTotalObjectSize(type);
    int offset = 0;
    bool positionFound = false;
    for (const auto* member = type; member->Type != GrannyEndMember; ++member) {
        if (member->Name && std::strcmp(member->Name, GrannyVertexPositionName) == 0 &&
                member->Type == GrannyReal32Member && member->ArrayWidth == 3) {
            positionFound = true;
            break;
        }
        const int size = GrannyGetMemberTypeSize(member);
        if (size <= 0 || offset > stride - size) return bounds;
        offset += size;
    }
    if (!positionFound || stride < offset + 12) return bounds;
    for (int vertex = 0; vertex < count; ++vertex) {
        std::array<float, 3> position;
        std::memcpy(position.data(), vertices + static_cast<std::size_t>(vertex) * stride + offset, sizeof(position));
        for (const float value : position) if (!std::isfinite(value)) return {};
        if (!bounds.valid) { bounds.min = bounds.max = position; bounds.valid = true; }
        else for (int axis = 0; axis < 3; ++axis) {
            bounds.min[axis] = std::min(bounds.min[axis], position[axis]);
            bounds.max[axis] = std::max(bounds.max[axis], position[axis]);
        }
    }
    return bounds;
}
bool BlendName(const char* name)
{
    constexpr char prefix[] = "blend";
    if (!name) return false;
    for (int i = 0; i < 5; ++i)
        if (!name[i] || std::tolower(static_cast<unsigned char>(name[i])) != prefix[i]) return false;
    return true;
}
MaterialAsset DescribeMaterial(const granny_material* material)
{
    MaterialAsset result;
    if (!material) return result;
    result.name = Text(material->Name);
    result.hasMatchingTextures = true;
    if (const auto* texture = GrannyGetMaterialTextureByType(material, GrannyDiffuseColorTexture))
        result.matchingTextures[0] = Text(texture->FromFileName);
    if (const auto* texture = GrannyGetMaterialTextureByType(material, GrannyOpacityTexture))
        result.matchingTextures[1] = Text(texture->FromFileName);
    granny_texture* diffuse = nullptr;
    granny_texture* opacity = nullptr;
    if (material->MapCount > 1 && material->Maps && BlendName(material->Name)) {
        if (material->Maps[0].Material)
            diffuse = GrannyGetMaterialTextureByType(material->Maps[0].Material, GrannyDiffuseColorTexture);
        if (material->Maps[1].Material)
            opacity = GrannyGetMaterialTextureByType(material->Maps[1].Material, GrannyDiffuseColorTexture);
    } else {
        diffuse = GrannyGetMaterialTextureByType(material, GrannyDiffuseColorTexture);
        opacity = GrannyGetMaterialTextureByType(material, GrannyOpacityTexture);
    }
    if (diffuse) result.textures[0] = Text(diffuse->FromFileName);
    if (opacity) result.textures[1] = Text(opacity->FromFileName);
    result.blending = opacity != nullptr;
    result.stage = result.blending ? MaterialStage::DiffuseOpacity : MaterialStage::Diffuse;
    granny_int32 twoSided = 0;
    granny_data_type_definition fieldType[] = {{GrannyInt32Member, "Two-sided"}, {GrannyEndMember}};
    granny_variant field{};
    if (material->ExtendedData.Type && material->ExtendedData.Object &&
        GrannyFindMatchingMember(material->ExtendedData.Type, material->ExtendedData.Object, "Two-sided", &field) && field.Type)
        GrannyConvertSingleObject(field.Type, field.Object, fieldType, &twoSided, nullptr);
    result.culling = twoSided == 1 ? Culling::None : Culling::Clockwise;
    return result;
}
SkeletonAsset DescribeSkeleton(const granny_skeleton& skeleton)
{
    SkeletonAsset result;
    result.name = Text(skeleton.Name);
    result.bones.reserve(skeleton.BoneCount);
    for (int index = 0; index < skeleton.BoneCount; ++index) {
        const auto& source = skeleton.Bones[index];
        BoneAsset bone;
        bone.id = index;
        bone.parentIndex = source.ParentIndex;
        bone.name = Text(source.Name);
        bone.localBind.flags = source.LocalTransform.Flags;
        std::memcpy(bone.localBind.position.data(), source.LocalTransform.Position, sizeof(bone.localBind.position));
        std::memcpy(bone.localBind.orientation.data(), source.LocalTransform.Orientation, sizeof(bone.localBind.orientation));
        std::memcpy(bone.localBind.scaleShear.data(), source.LocalTransform.ScaleShear, sizeof(bone.localBind.scaleShear));
        std::memcpy(bone.inverseBind.data(), source.InverseWorld4x4, sizeof(bone.inverseBind));
        result.bones.push_back(std::move(bone));
    }
    return result;
}
AnimationAsset DescribeAnimation(const granny_animation& animation)
{
    AnimationAsset result{Text(animation.Name), animation.Duration, animation.TimeStep,
        static_cast<std::uint32_t>(std::max(0, animation.TrackGroupCount))};
    const auto* group = animation.TrackGroupCount > 0 && animation.TrackGroups ? animation.TrackGroups[0] : nullptr;
    if (group && group->TextTracks) for (int track = 0; track < group->TextTrackCount; ++track) {
        const auto& source = group->TextTracks[track];
        if (source.Entries) for (int entry = 0; entry < source.EntryCount; ++entry)
            result.textEvents.push_back({Text(source.Entries[entry].Text), source.Entries[entry].TimeStamp});
    }
    return result;
}

class GrannyDocument final : public AssetDocument
{
public:
    GrannyDocument(AssetId id, FileOwner file, granny_file_info* info)
        : AssetDocument(std::move(id)), file_(std::move(file)), info_(info) {}
    granny_model* Model(std::size_t index) const
    {
        return index < models_.size() ? info_->Models[index] : nullptr;
    }
    granny_animation* Animation(std::size_t index) const
    {
        return index < animations_.size() ? info_->Animations[index] : nullptr;
    }
    granny_mesh* Mesh(std::size_t model, std::size_t mesh) const
    {
        const auto* source = Model(model);
        return source && mesh < models_[model].meshes.size() ? source->MeshBindings[mesh].Mesh : nullptr;
    }
    bool BuildMetadata()
    {
        if (info_->ModelCount < 0 || info_->AnimationCount < 0 ||
            (info_->ModelCount && !info_->Models) || (info_->AnimationCount && !info_->Animations)) return false;
        for (int index = 0; index < info_->AnimationCount; ++index) {
            const auto* source = info_->Animations[index];
            if (!source || source->TrackGroupCount < 0 || !std::isfinite(source->Duration) || source->Duration < 0) return false;
            animations_.push_back(DescribeAnimation(*source));
        }
        for (int index = 0; index < info_->ModelCount; ++index) {
            const auto* source = info_->Models[index];
            if (!source || source->MeshBindingCount < 0 || (source->MeshBindingCount && !source->MeshBindings)) return false;
            ModelAsset model;
            model.name = Text(source->Name);
            if (source->Skeleton) {
                if (source->Skeleton->BoneCount < 0 || (source->Skeleton->BoneCount && !source->Skeleton->Bones)) return false;
                model.skeleton = DescribeSkeleton(*source->Skeleton);
            }
            for (std::size_t animation = 0; animation < animations_.size(); ++animation)
                model.animations.push_back(static_cast<std::uint32_t>(animation));
            std::vector<const granny_material*> materialPointers;
            bool rigid = false, skinned = false;
            for (int meshIndex = 0; meshIndex < source->MeshBindingCount; ++meshIndex) {
                const auto* native = source->MeshBindings[meshIndex].Mesh;
                if (!native || !native->PrimaryVertexData || !native->PrimaryTopology ||
                    native->MaterialBindingCount < 0 || native->BoneBindingCount < 0 ||
                    (native->MaterialBindingCount && !native->MaterialBindings) ||
                    (native->BoneBindingCount && !native->BoneBindings)) return false;
                const int vertexCount = GrannyGetMeshVertexCount(native), indexCount = GrannyGetMeshIndexCount(native);
                const auto* type = GrannyGetMeshVertexType(native);
                if (vertexCount < 0 || indexCount < 0 || (vertexCount && (!type || !GrannyGetMeshVertices(native))) ||
                    (indexCount && !GrannyGetMeshIndices(native))) return false;
                MeshAsset mesh;
                mesh.name = Text(native->Name);
                mesh.vertexCount = static_cast<std::uint32_t>(vertexCount);
                mesh.indexCount = static_cast<std::uint32_t>(indexCount);
                const auto width = GrannyGetMeshBytesPerIndex(native);
                mesh.indexWidth = width == 2 ? IndexWidth::UInt16 : width == 4 ? IndexWidth::UInt32 : IndexWidth::Unknown;
                mesh.vertexLayout = DescribeLayout(type);
                mesh.vertexAttributes = DescribeVertexAttributes(type);
                mesh.sourceVertexStride = type ? static_cast<std::uint32_t>(GrannyGetTotalObjectSize(type)) : 0;
                const bool isRigid = GrannyMeshIsRigid(native);
                rigid |= isRigid; skinned |= !isRigid;
                mesh.deformation = isRigid ? Deformation::Rigid : Deformation::Skinned;
                mesh.twoSided = mesh.name.rfind("2x", 0) == 0;
                mesh.bounds = DescribeBounds(*native);
                for (int slot = 0; slot < native->MaterialBindingCount; ++slot) {
                    const auto* material = native->MaterialBindings[slot].Material;
                    const auto found = std::find(materialPointers.begin(), materialPointers.end(), material);
                    auto position = static_cast<std::size_t>(found - materialPointers.begin());
                    if (found == materialPointers.end()) {
                        materialPointers.push_back(material);
                        model.materials.push_back(DescribeMaterial(material));
                    }
                    mesh.materialBindings.push_back(static_cast<std::uint32_t>(position));
                }
                const int groups = GrannyGetMeshTriangleGroupCount(native);
                const auto* groupData = GrannyGetMeshTriangleGroups(native);
                if (groups < 0 || (groups && !groupData)) return false;
                for (int group = 0; group < groups; ++group) {
                    const auto& data = groupData[group];
                    const std::int64_t first = std::int64_t(data.TriFirst) * 3, count = std::int64_t(data.TriCount) * 3;
                    if (first < 0 || count < 0 || first + count > indexCount) return false;
                    // Preserve the legacy invalid material slot fallback (model palette slot zero).
                    mesh.materialGroups.push_back({data.MaterialIndex < 0 || data.MaterialIndex >= native->MaterialBindingCount ?
                        std::numeric_limits<std::uint32_t>::max() : static_cast<std::uint32_t>(data.MaterialIndex),
                        static_cast<std::uint32_t>(first), static_cast<std::uint32_t>(count)});
                }
                auto& skin = mesh.skin;
                for (int bone = 0; bone < native->BoneBindingCount; ++bone) {
                    const auto& binding = native->BoneBindings[bone];
                    skin.boneNames.emplace_back(Text(binding.BoneName));
                    Bounds bound;
                    std::memcpy(bound.min.data(), binding.OBBMin, sizeof(bound.min));
                    std::memcpy(bound.max.data(), binding.OBBMax, sizeof(bound.max));
                    bound.valid = true;
                    skin.boneBounds.push_back(bound);
                }
                if (source->Skeleton && native->BoneBindingCount > 0) {
                    auto* binding = GrannyNewMeshBinding(native, source->Skeleton, source->Skeleton);
                    const auto* remap = binding ? GrannyGetMeshBindingToBoneIndices(binding) : nullptr;
                    if (remap && GrannyGetMeshBindingBoneCount(binding) == native->BoneBindingCount) {
                        skin.validRemap = true;
                        for (int bone = 0; bone < native->BoneBindingCount; ++bone) {
                            skin.meshToSkeleton.push_back(remap[bone]);
                            if (remap[bone] < 0 || remap[bone] >= source->Skeleton->BoneCount) skin.validRemap = false;
                        }
                    }
                    if (binding) GrannyFreeMeshBinding(binding);
                }
                if (mesh.vertexLayout == VertexLayout::WeightedPositionNormalUV) {
                    skin.influencesPerVertex = 4;
                    skin.weightOffset = 12; skin.indexOffset = 16;
                    skin.normalizedByteWeights = skin.byteBoneIndices = true;
                }
                model.meshes.push_back(std::move(mesh));
            }
            model.deformation = rigid && skinned ? Deformation::Mixed : skinned ? Deformation::Skinned : Deformation::Rigid;
            models_.push_back(std::move(model));
        }
        return true;
    }
    AssetError CopyVertices(std::size_t model, std::size_t mesh, VertexLayout layout,
        std::span<std::byte> destination) const override
    {
        const auto* native = Mesh(model, mesh);
        if (!native) return AssetError::InvalidHandle;
        if (released_) return AssetError::UploadDataReleased;
        const auto* type = NativeLayout(layout);
        if (!type) return AssetError::UnsupportedLayout;
        const std::size_t bytes = models_[model].meshes[mesh].vertexCount * VertexStride(layout);
        if (destination.size() < bytes) return AssetError::BufferTooSmall;
        if (bytes) GrannyCopyMeshVertices(native, type, destination.data());
        return AssetError::None;
    }
    AssetError CopyIndices(std::size_t model, std::size_t mesh, IndexWidth width,
        std::span<std::byte> destination) const override
    {
        const auto* native = Mesh(model, mesh);
        if (!native) return AssetError::InvalidHandle;
        if (released_) return AssetError::UploadDataReleased;
        if (width != IndexWidth::UInt16 && width != IndexWidth::UInt32) return AssetError::InvalidIndexWidth;
        const auto count = models_[model].meshes[mesh].indexCount;
        if (destination.size() < static_cast<std::size_t>(count) * static_cast<std::size_t>(width)) return AssetError::BufferTooSmall;
        if (width == IndexWidth::UInt16 && GrannyGetMeshBytesPerIndex(native) == 4) {
            const auto* indices = static_cast<const std::int32_t*>(GrannyGetMeshIndices(native));
            for (std::size_t i = 0; i < count; ++i)
                if (indices[i] < 0 || indices[i] > std::numeric_limits<std::uint16_t>::max()) return AssetError::InvalidIndexWidth;
        }
        if (count) GrannyCopyMeshIndices(native, static_cast<int>(width), destination.data());
        return AssetError::None;
    }
    void ReleaseUploadData() override
    {
        if (released_) return;
        GrannyFreeFileSection(file_.get(), GrannyStandardRigidVertexSection);
        GrannyFreeFileSection(file_.get(), GrannyStandardRigidIndexSection);
        GrannyFreeFileSection(file_.get(), GrannyStandardDeformableIndexSection);
        GrannyFreeFileSection(file_.get(), GrannyStandardTextureSection);
        released_ = true;
    }
    std::unique_ptr<PoseEvaluator> CreatePose(const ModelHandle& model) const override;
    std::unique_ptr<AnimationInstance> CreateAnimationInstance(const ModelHandle& model) const override;
private:
    FileOwner file_;
    granny_file_info* info_{};
    bool released_{};
};

class GrannyMeshBinding final : public MeshBinding
{
public:
    GrannyMeshBinding(ModelHandle sourceOwner, ModelHandle destinationOwner, granny_model* source,
        std::size_t mesh, granny_skeleton* destination)
        : sourceOwner_(std::move(sourceOwner)), destinationOwner_(std::move(destinationOwner))
    {
        if (!source || !source->Skeleton || !destination || mesh >= static_cast<std::size_t>(source->MeshBindingCount)) return;
        mesh_ = source->MeshBindings[mesh].Mesh;
        if (!mesh_) return;
        binding_ = GrannyNewMeshBinding(mesh_, source->Skeleton, destination);
        if (!GrannyMeshIsRigid(mesh_))
            deformer_ = GrannyNewMeshDeformer(GrannyGetMeshVertexType(mesh_), GrannyPNT332VertexType,
                GrannyDeformPositionNormal, GrannyAllowUncopiedTail);
    }
    ~GrannyMeshBinding() override
    {
        if (deformer_) GrannyFreeMeshDeformer(deformer_);
        if (binding_) GrannyFreeMeshBinding(binding_);
    }
    bool Valid() const { return binding_ != nullptr; }
    std::span<const std::int32_t> BoneIndices() const override
    {
        if (!binding_) return {};
        const auto count = GrannyGetMeshBindingBoneCount(binding_);
        const auto* indices = GrannyGetMeshBindingToBoneIndices(binding_);
        return count > 0 && indices ? std::span<const std::int32_t>(indices, static_cast<std::size_t>(count)) : std::span<const std::int32_t>{};
    }
    AssetError DeformVertices(std::span<std::byte> destination, std::span<const float> matrices, bool useSSE) const override
    {
        if (!mesh_ || !binding_ || !deformer_) return AssetError::InvalidHandle;
        const auto count = GrannyGetMeshVertexCount(mesh_);
        const auto* vertices = GrannyGetMeshVertices(mesh_);
        if (count < 0 || !vertices || matrices.empty() || matrices.size() % 16) return AssetError::InvalidInput;
        if (destination.size() < static_cast<std::size_t>(count) * sizeof(granny_pnt332_vertex)) return AssetError::BufferTooSmall;
        const auto indices = BoneIndices();
        if (indices.empty()) return AssetError::InvalidHandle;
        for (auto bone : indices) if (bone < 0 || static_cast<std::size_t>(bone) >= matrices.size() / 16) return AssetError::InvalidHandle;
        if (useSSE) {
            DeformPWNT3432toGrannyPNGBT33332(count, vertices, destination.data(), indices.data(),
                reinterpret_cast<const granny_matrix_4x4*>(matrices.data()), sizeof(granny_pwnt3432_vertex), sizeof(granny_pnt332_vertex));
        } else {
            GrannyDeformVertices(deformer_, indices.data(), matrices.data(), count, vertices, destination.data());
        }
        return AssetError::None;
    }
private:
    ModelHandle sourceOwner_, destinationOwner_;
    const granny_mesh* mesh_{};
    granny_mesh_binding* binding_{};
    granny_mesh_deformer* deformer_{};
};

class GrannyAnimationInstance final : public AnimationInstance
{
public:
    explicit GrannyAnimationInstance(ModelHandle model, granny_model* legacy = nullptr) : owner_(std::move(model))
    {
        const auto* native = legacy ? legacy : GrannyInterop::GetModel(owner_);
        if (!native || !native->Skeleton || native->Skeleton->BoneCount <= 0) return;
        boneCount_ = native->Skeleton->BoneCount;
        model_ = GrannyInstantiateModel(native);
    }
    ~GrannyAnimationInstance() override
    {
        // Controls are SDK-owned after FreeControlOnceUnused; release the model before clip handles.
        if (model_) GrannyFreeModelInstance(model_);
        if (world_) GrannyFreeWorldPose(world_);
    }
    bool Valid() const { return model_ != nullptr; }
    bool PreparePose() override { return EnsurePose() != nullptr; }
    granny_model_instance* NativeModel() const { return model_; }
    granny_world_pose* EnsurePose()
    {
        if (!world_ && model_) world_ = GrannyNewWorldPose(boneCount_);
        return world_;
    }
    AssetError SetAnimation(const AnimationHandle& clip, float time) override
    {
        if (!Valid() || !std::isfinite(time)) return AssetError::InvalidInput;
        if (clip.GetDocument() != clip_.GetDocument() || clip.Index() != clip_.Index() || !control_) {
            const auto result = ChangeMotion(clip, 0, 0, 1.0f);
            if (result != AssetError::None) return result;
        }
        GrannySetModelClock(model_, time);
        return AssetError::None;
    }
    AssetError SetMotion(const AnimationHandle& clip, float localTime, float blendTime, int loopCount, float speed) override
    {
        const auto* native = GrannyInterop::GetAnimation(clip);
        if (!model_ || !native) return clip ? AssetError::ProviderMismatch : AssetError::InvalidHandle;
        const bool first = control_ == nullptr;
        if (control_) {
            GrannySetControlEaseOutCurve(control_, localTime, localTime + blendTime, 1, 1, 0, 0);
            GrannySetControlEaseIn(control_, false);
            GrannySetControlEaseOut(control_, true);
            GrannyCompleteControlAt(control_, localTime + blendTime);
            GrannyFreeControlIfComplete(control_);
        }
        clip_ = clip;
        control_ = GrannyPlayControlledAnimation(localTime, native, model_);
        if (!control_) return PlaybackFailure(native);
        RetainClip(control_, clip);
        GrannySetControlSpeed(control_, speed);
        GrannySetControlLoopCount(control_, loopCount);
        GrannySetControlEaseIn(control_, !first);
        GrannySetControlEaseOut(control_, false);
        if (!first && blendTime > 0)
            GrannySetControlEaseInCurve(control_, localTime, localTime + blendTime, 0, 0, 1, 1);
        GrannyFreeControlOnceUnused(control_);
        return AssetError::None;
    }
    AssetError ChangeMotion(const AnimationHandle& clip, float localTime, int loopCount, float speed) override
    {
        const auto* native = GrannyInterop::GetAnimation(clip);
        if (!model_ || !native) return clip ? AssetError::ProviderMismatch : AssetError::InvalidHandle;
        if (control_) {
            GrannySetControlEaseIn(control_, false);
            GrannySetControlEaseOut(control_, false);
            GrannyCompleteControlAt(control_, localTime);
            GrannyFreeControlIfComplete(control_);
        }
        clip_ = clip;
        control_ = GrannyPlayControlledAnimation(localTime, native, model_);
        if (!control_) return PlaybackFailure(native);
        RetainClip(control_, clip);
        GrannySetControlSpeed(control_, speed);
        GrannySetControlLoopCount(control_, loopCount);
        GrannySetControlEaseIn(control_, false);
        GrannySetControlEaseOut(control_, false);
        GrannyFreeControlOnceUnused(control_);
        return AssetError::None;
    }
    AssetError CopyMotionFrom(AnimationInstance& source, float localTime, bool freeSource) override
    {
        auto* other = dynamic_cast<GrannyAnimationInstance*>(&source);
        if (!other) return AssetError::ProviderMismatch;
        if (!model_ || other == this || !other->IsPlaying()) return AssetError::InvalidInput;
        if (control_) GrannyFreeControl(control_);
        clip_ = other->clip_;
        control_ = GrannyPlayControlledAnimation(localTime, GrannyInterop::GetAnimation(clip_), model_);
        if (!control_) return PlaybackFailure(GrannyInterop::GetAnimation(clip_));
        RetainClip(control_, clip_);
        GrannySetControlSpeed(control_, GrannyGetControlSpeed(other->control_));
        GrannySetControlLoopCount(control_, GrannyGetControlLoopCount(other->control_));
        GrannySetControlEaseIn(control_, true);
        GrannySetControlEaseOut(control_, false);
        GrannySetControlRawLocalClock(control_, GrannyGetControlRawLocalClock(other->control_));
        GrannyFreeControlOnceUnused(control_);
        if (freeSource) {
            GrannyFreeControl(other->control_);
            other->control_ = nullptr;
            other->CollectClipOwners();
        }
        return AssetError::None;
    }
    bool IsPlaying() const override { return control_ && !GrannyControlIsComplete(control_); }
    void SetMotionAtEnd() override
    {
        if (control_) GrannySetControlRawLocalClock(control_, GrannyGetControlLocalDuration(control_));
    }
    void SetClock(float time) override { if (model_) GrannySetModelClock(model_, time); }
    void FreeCompletedControls() override
    {
        if (!model_) return;
        GrannyFreeCompletedModelControls(model_);
        CollectClipOwners();
    }
    void UpdateTransform(float elapsed, std::span<float, 16> transform) const override
    {
        if (model_) GrannyUpdateModelMatrix(model_, elapsed, transform.data(), transform.data(), false);
    }
    std::span<const float> BoneWorldMatrix(BoneId bone) const override
    {
        if (!world_ || bone < 0 || bone >= boneCount_) return {};
        const auto* matrix = GrannyGetWorldPose4x4(world_, bone);
        return matrix ? std::span<const float>(matrix, 16) : std::span<const float>{};
    }
    PoseView CompositePose() const override { return GrannyInterop::GetPoseView(world_); }
    std::unique_ptr<MeshBinding> CreateMeshBinding(const ModelHandle& source, std::size_t mesh) const override
    {
        return Bind(source, GrannyInterop::GetModel(source), mesh);
    }
    std::unique_ptr<MeshBinding> Bind(const ModelHandle& owner, granny_model* source, std::size_t mesh) const
    {
        if (!model_) return {};
        auto binding = std::make_unique<GrannyMeshBinding>(owner, owner_, source, mesh, GrannyGetSourceSkeleton(model_));
        return binding->Valid() ? std::move(binding) : nullptr;
    }
    PoseResult Evaluate(const PoseRequest& request) override
    {
        // Same shared scratch policy as the legacy model-update thread, now owned by the provider.
        struct SharedScratch {
            granny_local_pose* pose{};
            int bones{};
            ~SharedScratch() { if (pose) GrannyFreeLocalPose(pose); }
            granny_local_pose* Get(int count) {
                if (count > bones || !pose) {
                    if (pose) GrannyFreeLocalPose(pose);
                    pose = GrannyNewLocalPose(count); bones = count;
                }
                return pose;
            }
        };
        static SharedScratch scratch;
        if (!model_ || !EnsurePose()) return {{}, AssetError::EvaluationFailed};
        GrannyInterop::GrannyPoseEvaluator evaluator(model_, scratch.Get(boneCount_), world_, boneCount_);
        return evaluator.Evaluate(request);
    }
private:
    struct ClipOwner { granny_control* control; AnimationHandle clip; };
    AssetError PlaybackFailure(const granny_animation* animation) const
    {
        const auto* model = model_ ? GrannyGetSourceModel(model_) : nullptr;
        granny_int32x group = -1;
        // Preserve the legacy no-control result for clips broadcast to rigid attachments.
        if (model && model->Name && animation && !GrannyFindTrackGroupForModel(animation, model->Name, &group))
            return AssetError::NoMatchingTracks;
        return AssetError::EvaluationFailed;
    }
    void RetainClip(granny_control* control, const AnimationHandle& clip)
    {
        CollectClipOwners();
        clips_.push_back({control, clip});
    }
    void CollectClipOwners()
    {
        auto active = [this](granny_control* control) {
            for (auto* binding = GrannyModelControlsBegin(model_); binding != GrannyModelControlsEnd(model_);
                 binding = GrannyModelControlsNext(binding))
                if (GrannyGetControlFromBinding(binding) == control) return true;
            return false;
        };
        std::erase_if(clips_, [&](const ClipOwner& entry) { return !active(entry.control); });
        if (control_ && !active(control_)) control_ = nullptr;
    }
    ModelHandle owner_;
    AnimationHandle clip_;
    std::vector<ClipOwner> clips_;
    granny_model_instance* model_{};
    granny_world_pose* world_{};
    granny_control* control_{};
    int boneCount_{};
};
std::unique_ptr<PoseEvaluator> GrannyDocument::CreatePose(const ModelHandle& model) const
{
    return CreateAnimationInstance(model);
}
std::unique_ptr<AnimationInstance> GrannyDocument::CreateAnimationInstance(const ModelHandle& model) const
{
    if (!model || model.GetDocument().get() != this || !model.Get()->skeleton || model.Get()->skeleton->bones.empty()) return {};
    auto result = std::make_unique<GrannyAnimationInstance>(model);
    return result->Valid() ? std::move(result) : nullptr;
}
class GrannyProvider final : public AssetProvider
{
public:
    LoadResult Load(AssetId id, std::span<const std::byte> bytes) override
    {
        if (bytes.empty() || !bytes.data() || bytes.size() > static_cast<std::size_t>(std::numeric_limits<granny_int32>::max()))
            return {{}, AssetError::InvalidInput};
        FileOwner file(GrannyReadEntireFileFromMemory(static_cast<granny_int32>(bytes.size()), const_cast<std::byte*>(bytes.data())));
        if (!file) return {{}, AssetError::InvalidAsset};
        auto* info = GrannyGetFileInfo(file.get());
        if (!info) return {{}, AssetError::InvalidAsset};
        auto document = std::make_shared<GrannyDocument>(std::move(id), std::move(file), info);
        if (!document->BuildMetadata()) return {{}, AssetError::InvalidAsset};
        return {AssetHandle(std::move(document)), AssetError::None};
    }
};
class BorrowedAnimationDocument final : public AssetDocument
{
public:
    explicit BorrowedAnimationDocument(granny_animation* animation) : AssetDocument("legacy-test-animation"), animation_(animation)
    {
        if (animation) animations_.push_back(DescribeAnimation(*animation));
    }
    granny_animation* Animation() const { return animation_; }
    AssetError CopyVertices(std::size_t, std::size_t, VertexLayout, std::span<std::byte>) const override { return AssetError::UnsupportedLayout; }
    AssetError CopyIndices(std::size_t, std::size_t, IndexWidth, std::span<std::byte>) const override { return AssetError::UnsupportedLayout; }
    void ReleaseUploadData() override {}
    std::unique_ptr<PoseEvaluator> CreatePose(const ModelHandle&) const override { return {}; }
private:
    granny_animation* animation_;
};
}
AssetProvider& GetGrannyAssetProvider()
{
    static GrannyProvider provider;
    return provider;
}
void ConfigureGrannyDiagnostics()
{
    granny_log_callback callback{};
    GrannySetLogCallback(&callback);
}

namespace GrannyInterop
{
granny_model_instance* GetAnimationInstance(AnimationInstance& instance)
{
    auto* native = dynamic_cast<GrannyAnimationInstance*>(&instance);
    return native ? native->NativeModel() : nullptr;
}
granny_world_pose* EnsureWorldPose(AnimationInstance& instance)
{
    auto* native = dynamic_cast<GrannyAnimationInstance*>(&instance);
    return native ? native->EnsurePose() : nullptr;
}
std::unique_ptr<AnimationInstance> CreateLegacyAnimationInstance(granny_model* model)
{
    auto result = std::make_unique<GrannyAnimationInstance>(ModelHandle{}, model);
    return result->Valid() ? std::move(result) : nullptr;
}
AnimationHandle CreateLegacyAnimationHandle(granny_animation* animation)
{
    return animation ? AnimationHandle(std::make_shared<BorrowedAnimationDocument>(animation), 0) : AnimationHandle{};
}
std::unique_ptr<MeshBinding> CreateLegacyMeshBinding(granny_model* source, std::size_t mesh, AnimationInstance& destination)
{
    const auto* session = dynamic_cast<const GrannyAnimationInstance*>(&destination);
    return session ? session->Bind({}, source, mesh) : nullptr;
}
granny_model* GetModel(const ModelHandle& handle)
{
    const auto* document = dynamic_cast<const GrannyDocument*>(handle.GetDocument().get());
    return document && handle ? document->Model(handle.Index()) : nullptr;
}
granny_animation* GetAnimation(const AnimationHandle& handle)
{
    const auto* document = dynamic_cast<const GrannyDocument*>(handle.GetDocument().get());
    if (document && handle) return document->Animation(handle.Index());
    const auto* borrowed = dynamic_cast<const BorrowedAnimationDocument*>(handle.GetDocument().get());
    return borrowed && handle && handle.Index() == 0 ? borrowed->Animation() : nullptr;
}
granny_mesh* GetMesh(const ModelHandle& handle, std::size_t mesh)
{
    const auto* document = dynamic_cast<const GrannyDocument*>(handle.GetDocument().get());
    return document && handle ? document->Mesh(handle.Index(), mesh) : nullptr;
}
PoseView GetPoseView(const granny_world_pose* pose)
{
    if (!pose) return {};
    const int count = GrannyGetWorldPoseBoneCount(pose);
    const auto* matrices = GrannyGetWorldPoseComposite4x4Array(pose);
    if (count <= 0 || !matrices) return {};
    static_assert(sizeof(granny_matrix_4x4) == sizeof(Matrix4));
    return {{&matrices[0][0][0], static_cast<std::size_t>(count) * 16}};
}
bool CopyWorldPose(const granny_world_pose* pose, std::span<Matrix4> destination)
{
    const auto view = GetPoseView(pose);
    if (!view.Valid() || destination.size() < view.BoneCount()) return false;
    std::memcpy(destination.data(), view.values.data(), view.values.size_bytes());
    return true;
}
PoseResult GrannyPoseEvaluator::Evaluate(const PoseRequest& request)
{
    if (!model_ || !scratch_ || !world_ || boneCount_ <= 0 || GrannyGetWorldPoseBoneCount(world_) < boneCount_)
        return {{}, AssetError::EvaluationFailed};
    if (!request.attachmentMatrix.empty() && request.attachmentMatrix.size() != 16)
        return {{}, AssetError::InvalidInput};
    GrannySampleModelAnimationsAccelerated(model_, boneCount_, request.attachmentMatrix.empty() ? nullptr : request.attachmentMatrix.data(),
        scratch_, world_);
    return {GetPoseView(world_), AssetError::None};
}
}
}
