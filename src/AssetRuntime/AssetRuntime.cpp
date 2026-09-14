#include "AssetRuntime.h"
#include <cmath>
#include <limits>
#include <utility>

namespace AssetRuntime
{
const char* ErrorName(AssetError error)
{
    switch (error) {
#define ASSET_ERROR_NAME(value) case AssetError::value: return #value
    ASSET_ERROR_NAME(None); ASSET_ERROR_NAME(NoMatchingTracks); ASSET_ERROR_NAME(InvalidInput); ASSET_ERROR_NAME(InvalidAsset);
    ASSET_ERROR_NAME(InvalidHandle); ASSET_ERROR_NAME(UnsupportedLayout); ASSET_ERROR_NAME(InvalidIndexWidth);
    ASSET_ERROR_NAME(BufferTooSmall); ASSET_ERROR_NAME(UploadDataReleased); ASSET_ERROR_NAME(MissingSkeleton);
    ASSET_ERROR_NAME(ProviderMismatch); ASSET_ERROR_NAME(EvaluationFailed);
#undef ASSET_ERROR_NAME
    }
    return "Unknown";
}
std::size_t VertexStride(VertexLayout layout)
{
    switch (layout) {
    case VertexLayout::PositionNormalUV: return 32;
    case VertexLayout::PositionNormalUV2: return 40;
    case VertexLayout::WeightedPositionNormalUV: return 40;
    default: return 0;
    }
}
BoneId SkeletonAsset::FindBone(std::string_view name) const
{
    for (std::size_t index = 0; index < bones.size(); ++index)
        if (bones[index].name == name) return static_cast<BoneId>(index);
    return -1;
}
ModelHandle::ModelHandle(std::shared_ptr<AssetDocument> owner, std::size_t index)
    : owner_(std::move(owner)), index_(index) {}
const ModelAsset* ModelHandle::Get() const
{
    return owner_ && index_ < owner_->Models().size() ? &owner_->Models()[index_] : nullptr;
}
AnimationHandle::AnimationHandle(std::shared_ptr<AssetDocument> owner, std::size_t index)
    : owner_(std::move(owner)), index_(index) {}
const AnimationAsset* AnimationHandle::Get() const
{
    return owner_ && index_ < owner_->Animations().size() ? &owner_->Animations()[index_] : nullptr;
}
ModelHandle AssetHandle::Model(std::size_t index) const
{
    return document_ && index < document_->Models().size() ? ModelHandle(document_, index) : ModelHandle{};
}
AnimationHandle AssetHandle::Animation(std::size_t index) const
{
    return document_ && index < document_->Animations().size() ? AnimationHandle(document_, index) : AnimationHandle{};
}
std::size_t AssetHandle::ModelCount() const { return document_ ? document_->Models().size() : 0; }
std::size_t AssetHandle::AnimationCount() const { return document_ ? document_->Animations().size() : 0; }
void AssetHandle::ReleaseUploadData() const { if (document_) document_->ReleaseUploadData(); }
LoadResult LoadModel(AssetId id, std::span<const std::byte> bytes, AssetProvider& provider)
{
    if (bytes.empty() || !bytes.data()) return {{}, AssetError::InvalidInput};
    auto result = provider.Load(std::move(id), bytes);
    if (!result.asset && result.error == AssetError::None) result.error = AssetError::InvalidAsset;
    if (result.error != AssetError::None) result.asset.Reset();
    return result;
}
PoseResult EvaluatePose(PoseEvaluator& evaluator, const PoseRequest& request)
{
    if (!request.attachmentMatrix.empty() && request.attachmentMatrix.size() != 16)
        return {{}, AssetError::InvalidInput};
    for (const float value : request.attachmentMatrix)
        if (!std::isfinite(value)) return {{}, AssetError::InvalidInput};
    return evaluator.Evaluate(request);
}
AttachmentBinding ResolveAttachment(const SkeletonAsset& skeleton, std::string_view bone, AttachmentKind kind)
{
    return {kind, skeleton.FindBone(bone)};
}
}
