#include "AssetRuntime/AssetRuntime.h"
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace AssetRuntime;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

class KnownPose final : public PoseEvaluator
{
public:
    explicit KnownPose(ModelHandle owner = {}) : owner_(std::move(owner))
    {
        if (owner_ && owner_.Get()->skeleton) {
            for (std::size_t bone = 0; bone < owner_.Get()->skeleton->bones.size(); ++bone)
                ownedMatrices_.insert(ownedMatrices_.end(), matrix.begin(), matrix.end());
        }
    }
    unsigned evaluations{};
    Matrix4 matrix{1,0,0,0,0,1,0,0,0,0,1,0,7,-3,12,1};
    PoseResult Evaluate(const PoseRequest&) override
    {
        ++evaluations;
        return {{ownedMatrices_.empty() ? std::span<const float>(matrix) : std::span<const float>(ownedMatrices_)}, AssetError::None};
    }
private:
    ModelHandle owner_;
    std::vector<float> ownedMatrices_;
};

// A tiny in-memory provider exercises the complete neutral ownership contract on LP64 too.
class MemoryDocument final : public AssetDocument
{
public:
    explicit MemoryDocument(AssetId id) : AssetDocument(std::move(id))
    {
        ModelAsset model;
        model.name = "fixture";
        model.meshes.resize(1);
        model.meshes[0].vertexCount = 3;
        model.meshes[0].indexCount = 3;
        model.meshes[0].indexWidth = IndexWidth::UInt16;
        model.meshes[0].vertexLayout = VertexLayout::PositionNormalUV;
        model.meshes[0].sourceVertexStride = 32;
        model.skeleton.emplace();
        model.skeleton->bones = {{0, -1, "root"}, {1, 0, "hand"}, {2, 0, "hand"}};
        models_.push_back(std::move(model));
        animations_.push_back({"idle", 1.25f, 1.f / 30, 1});
    }
    AssetError CopyVertices(std::size_t model, std::size_t mesh, VertexLayout layout, std::span<std::byte> output) const override
    {
        if (model != 0 || mesh != 0) return AssetError::InvalidHandle;
        if (released_) return AssetError::UploadDataReleased;
        if (layout != VertexLayout::PositionNormalUV) return AssetError::UnsupportedLayout;
        constexpr std::array<float, 24> vertices{0,0,0,0,0,1,0,0, 1,0,0,0,0,1,1,0, 0,1,0,0,0,1,0,1};
        if (output.size() < sizeof(vertices)) return AssetError::BufferTooSmall;
        std::memcpy(output.data(), vertices.data(), sizeof(vertices));
        return AssetError::None;
    }
    AssetError CopyIndices(std::size_t model, std::size_t mesh, IndexWidth width, std::span<std::byte> output) const override
    {
        if (model != 0 || mesh != 0) return AssetError::InvalidHandle;
        if (released_) return AssetError::UploadDataReleased;
        if (width != IndexWidth::UInt16 && width != IndexWidth::UInt32) return AssetError::InvalidIndexWidth;
        if (output.size() < 3 * static_cast<std::size_t>(width)) return AssetError::BufferTooSmall;
        const std::array<std::uint16_t, 3> narrow{0,1,2};
        const std::array<std::uint32_t, 3> wide{0,1,2};
        if (width == IndexWidth::UInt16) std::memcpy(output.data(), narrow.data(), sizeof(narrow));
        else std::memcpy(output.data(), wide.data(), sizeof(wide));
        return AssetError::None;
    }
    void ReleaseUploadData() override { released_ = true; }
    std::unique_ptr<PoseEvaluator> CreatePose(const ModelHandle& model) const override
    {
        return model && model.GetDocument().get() == this ? std::make_unique<KnownPose>(model) : nullptr;
    }
private:
    bool released_{};
};
class MemoryProvider final : public AssetProvider
{
public:
    bool fail{}, emptySuccess{};
    unsigned loads{};
    LoadResult Load(AssetId id, std::span<const std::byte>) override
    {
        ++loads;
        if (emptySuccess) return {};
        return {AssetHandle(std::make_shared<MemoryDocument>(std::move(id))), fail ? AssetError::InvalidAsset : AssetError::None};
    }
};
int main()
{
    try {
        Check(liveDocuments == 0, "Initial document count");
        Check(!AssetHandle{} && !ModelHandle{} && !AnimationHandle{}, "Default handles are invalid");
        Check(!AssetHandle{}.Model(0) && !AssetHandle{}.Animation(0), "Invalid root gives invalid child handles");
        const std::byte byte{1};
        MemoryProvider provider;
        Check(LoadModel("missing.gr2", {}, provider).error == AssetError::InvalidInput && provider.loads == 0,
            "Missing data rejected before provider invocation");
        for (const auto* path : {"d:/ymir work/pc/body.gr2", "locale/de/model.gr2", "pack/model.gr2"}) {
            auto loaded = LoadModel(path, {&byte, 1}, provider);
            Check(bool(loaded) && loaded.asset.Get()->Id() == path, "Existing asset IDs retained verbatim");
            auto model = loaded.asset.Model(0);
            auto animation = loaded.asset.Animation(0);
            Check(model && animation && !loaded.asset.Model(1) && !loaded.asset.Animation(1), "Checked model and animation slots");
            Check(!ModelHandle(model.GetDocument(), std::numeric_limits<std::size_t>::max()), "Large model index rejected");
            Check(!AnimationHandle(model.GetDocument(), std::numeric_limits<std::size_t>::max()), "Large animation index rejected");
            Check(model.Get()->meshes[0].vertexCount == 3 && model.Get()->meshes[0].indexCount == 3, "Neutral mesh metadata");
            Check(model.Get()->deformation == Deformation::Rigid, "Explicit rigid default");
            std::array<float, 24> vertices{};
            std::array<std::uint16_t, 3> indices{};
            Check(loaded.asset.Get()->CopyVertices(0, 0, VertexLayout::PositionNormalUV, std::as_writable_bytes(std::span(vertices))) == AssetError::None &&
                vertices[8] == 1 && vertices[17] == 1, "Alternate provider supplies usable existing-layout vertices");
            Check(loaded.asset.Get()->CopyIndices(0, 0, IndexWidth::UInt16, std::as_writable_bytes(std::span(indices))) == AssetError::None &&
                indices == std::array<std::uint16_t, 3>{0,1,2}, "Alternate provider supplies triangle indices");
            auto ownedPose = loaded.asset.Get()->CreatePose(model);
            Check(ownedPose && EvaluatePose(*ownedPose).pose.values[12] == 7 && EvaluatePose(*ownedPose).pose.BoneCount() == 3,
                "Alternate provider supplies a neutral owned pose with matching bone count");
            Check(!loaded.asset.Get()->CreatePose(ModelHandle{}), "Alternate provider rejects an invalid model pose request");
            ownedPose.reset();
            const auto& skeleton = *model.Get()->skeleton;
            Check(skeleton.FindBone("root") == 0 && skeleton.FindBone("hand") == 1 && skeleton.FindBone("missing") == -1,
                "Bone lookup preserves original order and first duplicate");
            Check(skeleton.bones[0].parentIndex == -1 && skeleton.bones[1].parentIndex == 0, "Bone parent convention unchanged");
            for (auto kind : {AttachmentKind::Weapon, AttachmentKind::Shield, AttachmentKind::Hair,
                    AttachmentKind::Armor, AttachmentKind::MountRider, AttachmentKind::Other}) {
                auto attachment = ResolveAttachment(skeleton, "hand", kind);
                Check(attachment && attachment.bone == 1 && attachment.kind == kind, "Neutral attachment kind and stable bone ID");
                Check(!ResolveAttachment(skeleton, "invalid", kind), "Missing attachment bone is invalid");
            }
            Check(animation.Get()->name == "idle" && animation.Get()->duration == 1.25f, "Animation metadata survives neutral API");
            std::weak_ptr<AssetDocument> owner = model.GetDocument();
            loaded.asset.Reset();
            Check(!owner.expired() && model && animation && liveDocuments == 1, "Child handles retain document after root unload");
            model = {};
            Check(!owner.expired() && animation, "Animation alone retains its document");
            animation = {};
            Check(owner.expired() && liveDocuments == 0, "Last owning handle releases document exactly once");
        }
        provider.fail = true;
        auto failed = LoadModel("bad", {&byte, 1}, provider);
        Check(!failed && !failed.asset && failed.error == AssetError::InvalidAsset && liveDocuments == 0,
            "Provider error releases partial handle and propagates");
        provider.fail = false;
        provider.emptySuccess = true;
        Check(LoadModel("bad", {&byte, 1}, provider).error == AssetError::InvalidAsset, "Empty provider success cannot escape");
        KnownPose pose;
        auto output = EvaluatePose(pose);
        Check(output.error == AssetError::None && output.pose.Valid() && output.pose.BoneCount() == 1 &&
            output.pose.values.data() == pose.matrix.data() && output.pose.values[12] == 7, "Pose is a borrowed unchanged row-vector matrix");
        std::array<float, 15> shortMatrix{};
        Check(EvaluatePose(pose, {shortMatrix}).error == AssetError::InvalidInput && pose.evaluations == 1,
            "Malformed parent matrix rejected before native evaluation");
        auto badMatrix = pose.matrix;
        badMatrix[0] = std::numeric_limits<float>::quiet_NaN();
        Check(EvaluatePose(pose, {badMatrix}).error == AssetError::InvalidInput && pose.evaluations == 1,
            "Non-finite parent matrix rejected");
        Check(VertexStride(VertexLayout::PositionNormalUV) == 32 && VertexStride(VertexLayout::PositionNormalUV2) == 40 &&
            VertexStride(VertexLayout::WeightedPositionNormalUV) == 40 && VertexStride(VertexLayout::Unknown) == 0,
            "Existing vertex layout widths preserved");
        static_assert(sizeof(BoneId) == 4 && sizeof(Matrix4) == 64);
        std::cout << "PASS AssetRuntime handles, lifetime, metadata, failure, attachment and pose contract; pointer="
            << sizeof(void*) << " long=" << sizeof(long) << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
