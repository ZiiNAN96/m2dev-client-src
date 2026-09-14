#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace AssetRuntime
{
// ZiiNAN: Asset Runtime boundary
using AssetId = std::string; // Existing resource/pack paths are preserved verbatim.
using BoneId = std::int32_t; // Original source order; -1 is the missing/root sentinel.
using Matrix4 = std::array<float, 16>;
inline std::atomic_size_t liveDocuments{};
inline std::atomic_size_t liveAnimationInstances{}, liveMeshBindings{};

enum class AssetError { None,
    NoMatchingTracks, // A broadcast clip does not animate this model; expected no-op, no fallback.
    InvalidInput, InvalidAsset, InvalidHandle, UnsupportedLayout,
    InvalidIndexWidth, BufferTooSmall, UploadDataReleased, MissingSkeleton, ProviderMismatch, EvaluationFailed };
const char* ErrorName(AssetError error);
enum class Deformation { Rigid, Skinned, Mixed };
enum class PrimitiveTopology { TriangleList };
enum class IndexWidth : std::uint8_t { Unknown = 0, UInt16 = 2, UInt32 = 4 };
enum class VertexLayout { Unknown, PositionNormalUV, PositionNormalUV2, WeightedPositionNormalUV };
enum VertexAttribute : std::uint32_t { Position = 1, Normal = 2, UV0 = 4, UV1 = 8 };
std::size_t VertexStride(VertexLayout layout);

struct Bounds { std::array<float, 3> min{}, max{}; bool valid{}; };
struct LocalTransform
{
    std::uint32_t flags{};
    std::array<float, 3> position{};
    std::array<float, 4> orientation{0, 0, 0, 1};
    std::array<float, 9> scaleShear{1,0,0,0,1,0,0,0,1};
};
struct BoneAsset
{
    BoneId id{-1}, parentIndex{-1};
    std::string name;
    LocalTransform localBind;
    Matrix4 inverseBind{};
    std::int32_t sourceNodeIndex{-1};
    Matrix4 localBindMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
    bool hasLocalBindMatrix{};
};
struct SkeletonAsset
{
    std::string name;
    std::vector<BoneAsset> bones;
    BoneId rootIndex{-1};
    std::int32_t sourceRootNode{-1};
    BoneId FindBone(std::string_view name) const;
};
enum class MaterialStage { Diffuse, DiffuseOpacity };
enum class Culling { Clockwise, None };
// ZiiNAN: Modern asset pipeline
struct EncodedImage
{
    AssetId id;
    std::string mimeType;
    std::vector<std::byte> bytes;
};
struct MaterialAsset
{
    std::string name;
    std::array<AssetId, 2> textures;
    std::array<AssetId, 2> matchingTextures;
    bool hasMatchingTextures{};
    MaterialStage stage{MaterialStage::Diffuse};
    Culling culling{Culling::Clockwise};
    bool alphaTest{true}, blending{}, depthWrite{true};
    bool specular{};
    float specularPower{};
    std::uint8_t sphereMapIndex{};
    std::array<std::shared_ptr<const EncodedImage>, 2> embeddedImages{};
    std::array<float, 4> baseColorFactor{1, 1, 1, 1};
    float alphaCutoff{0.5f};
    bool explicitRenderState{};
    // Per-draw actor/world passes may override these legacy defaults.
};
struct MaterialGroup
{
    std::uint32_t materialIndex{}, firstIndex{}, indexCount{};
};
struct SkinningAsset
{
    std::vector<std::string> boneNames;
    std::vector<BoneId> meshToSkeleton; // Source self binding, no bone reordering.
    std::vector<Bounds> boneBounds;
    std::uint32_t influencesPerVertex{}, weightOffset{}, indexOffset{};
    bool normalizedByteWeights{}, byteBoneIndices{}, validRemap{};
    // Owned import metadata; not a promise that a runtime skin evaluator is available.
    std::vector<std::array<std::uint32_t, 4>> jointIndices;
    std::vector<std::array<float, 4>> jointWeights;
};
struct SkinningStreamView
{
    std::span<const std::byte> vertices;
    std::size_t stride{}, weightOffset{}, indexOffset{};
    std::span<const std::uint16_t> meshToSkeleton;
    std::span<const std::uint16_t> indices;
};
struct MeshAsset
{
    std::string name;
    std::uint32_t vertexCount{}, indexCount{}, sourceVertexStride{};
    std::uint32_t vertexAttributes{}; // Semantic source channels, independent of exact packed layout.
    PrimitiveTopology topology{PrimitiveTopology::TriangleList};
    IndexWidth indexWidth{IndexWidth::Unknown};
    VertexLayout vertexLayout{VertexLayout::Unknown};
    Deformation deformation{Deformation::Rigid};
    Bounds bounds; // Original local vertex positions, before pose or world transforms.
    std::vector<std::uint32_t> materialBindings; // Mesh slots -> model material slots.
    std::vector<MaterialGroup> materialGroups; // Original mesh-local material slots.
    SkinningAsset skin;
    bool twoSided{};
    std::vector<std::array<float, 4>> tangents; // Optional transformed tangent + bitangent handedness metadata.
};
struct ModelAsset
{
    std::string name;
    std::vector<MeshAsset> meshes;
    std::vector<MaterialAsset> materials;
    std::optional<SkeletonAsset> skeleton;
    std::vector<std::uint32_t> animations; // Document animation slots.
    Deformation deformation{Deformation::Rigid};
    bool renderable{true};
};
enum class AnimationPath { Translation, Rotation, Scale };
enum class AnimationInterpolation { Linear, Step, CubicSpline };
struct AnimationChannelAsset
{
    std::int32_t targetNode{-1};
    std::string targetName;
    AnimationPath path{AnimationPath::Translation};
    AnimationInterpolation interpolation{AnimationInterpolation::Linear};
    std::uint32_t keyframeCount{};
};
struct AnimationTextEvent { std::string text; float time{}; };
struct AnimationAsset
{
    std::string name;
    float duration{}, timeStep{};
    std::uint32_t trackGroupCount{};
    std::vector<AnimationTextEvent> textEvents; // Existing first-track-group event order.
    std::vector<AnimationChannelAsset> channels;
    bool metadataOnly{};
};

class AssetDocument;
class ModelHandle
{
public:
    ModelHandle() = default;
    ModelHandle(std::shared_ptr<AssetDocument> owner, std::size_t index);
    const ModelAsset* Get() const;
    explicit operator bool() const { return Get() != nullptr; }
    const std::shared_ptr<AssetDocument>& GetDocument() const { return owner_; }
    std::size_t Index() const { return index_; }
private:
    std::shared_ptr<AssetDocument> owner_;
    std::size_t index_{};
};
class AnimationHandle
{
public:
    AnimationHandle() = default;
    AnimationHandle(std::shared_ptr<AssetDocument> owner, std::size_t index);
    const AnimationAsset* Get() const;
    explicit operator bool() const { return Get() != nullptr; }
    const std::shared_ptr<AssetDocument>& GetDocument() const { return owner_; }
    std::size_t Index() const { return index_; }
private:
    std::shared_ptr<AssetDocument> owner_;
    std::size_t index_{};
};
class AssetHandle
{
public:
    AssetHandle() = default;
    explicit AssetHandle(std::shared_ptr<AssetDocument> document) : document_(std::move(document)) {}
    AssetDocument* Get() const { return document_.get(); }
    explicit operator bool() const { return bool(document_); }
    ModelHandle Model(std::size_t index) const;
    AnimationHandle Animation(std::size_t index) const;
    std::size_t ModelCount() const;
    std::size_t AnimationCount() const;
    void ReleaseUploadData() const;
    void Reset() { document_.reset(); }
private:
    std::shared_ptr<AssetDocument> document_;
};

// Row-vector 4x4 values, unchanged composite/inverse-bind convention. Borrowed until next evaluation.
struct PoseView
{
    std::span<const float> values;
    std::size_t BoneCount() const { return values.size() / 16; }
    bool Valid() const { return !values.empty() && values.size() % 16 == 0; }
};
struct PoseRequest
{
    std::span<const float> attachmentMatrix; // Empty means no attachment; otherwise exactly 16 values.
};
struct PoseResult { PoseView pose; AssetError error{AssetError::None}; };
class PoseEvaluator
{
public:
    virtual ~PoseEvaluator() = default;
    virtual AssetError SetAnimation(const AnimationHandle&, float) { return AssetError::ProviderMismatch; }
    virtual PoseResult Evaluate(const PoseRequest&) = 0;
};
class MeshBinding
{
public:
    MeshBinding() { ++liveMeshBindings; }
    virtual ~MeshBinding() { --liveMeshBindings; }
    MeshBinding(const MeshBinding&) = delete;
    MeshBinding& operator=(const MeshBinding&) = delete;
    virtual std::span<const std::int32_t> BoneIndices() const = 0;
    virtual AssetError DeformVertices(std::span<std::byte> destination,
        std::span<const float> compositeMatrices, bool useSSE) const = 0;
};
class AnimationInstance : public PoseEvaluator
{
public:
    AnimationInstance() { ++liveAnimationInstances; }
    ~AnimationInstance() override { --liveAnimationInstances; }
    AnimationInstance(const AnimationInstance&) = delete;
    AnimationInstance& operator=(const AnimationInstance&) = delete;
    virtual bool PreparePose() = 0;
    virtual AssetError SetMotion(const AnimationHandle& clip, float localTime, float blendTime,
        int loopCount, float speedRatio) = 0;
    virtual AssetError ChangeMotion(const AnimationHandle& clip, float localTime, int loopCount, float speedRatio) = 0;
    virtual AssetError CopyMotionFrom(AnimationInstance& source, float localTime, bool freeSource) = 0;
    virtual bool IsPlaying() const = 0;
    virtual void SetMotionAtEnd() = 0;
    virtual void SetClock(float time) = 0;
    virtual void FreeCompletedControls() = 0;
    virtual void UpdateTransform(float elapsed, std::span<float, 16> transform) const = 0;
    virtual std::span<const float> BoneWorldMatrix(BoneId bone) const = 0;
    virtual PoseView CompositePose() const = 0;
    virtual std::unique_ptr<MeshBinding> CreateMeshBinding(const ModelHandle& source, std::size_t mesh) const = 0;
};
PoseResult EvaluatePose(PoseEvaluator& evaluator, const PoseRequest& request = {});

class AssetDocument
{
public:
    explicit AssetDocument(AssetId id) : id_(std::move(id)) { ++liveDocuments; }
    virtual ~AssetDocument() { --liveDocuments; }
    AssetDocument(const AssetDocument&) = delete;
    AssetDocument& operator=(const AssetDocument&) = delete;
    const AssetId& Id() const { return id_; }
    const std::vector<ModelAsset>& Models() const { return models_; }
    const std::vector<AnimationAsset>& Animations() const { return animations_; }
    // Caller owns output storage. Upload data remains available until explicit release after captures.
    virtual AssetError CopyVertices(std::size_t model, std::size_t mesh, VertexLayout layout,
        std::span<std::byte> destination) const = 0;
    virtual AssetError CopyIndices(std::size_t model, std::size_t mesh, IndexWidth width,
        std::span<std::byte> destination) const = 0;
    virtual void ReleaseUploadData() = 0;
    virtual std::unique_ptr<PoseEvaluator> CreatePose(const ModelHandle&) const = 0;
    virtual std::unique_ptr<AnimationInstance> CreateAnimationInstance(const ModelHandle&) const { return {}; }
protected:
    std::vector<ModelAsset> models_;
    std::vector<AnimationAsset> animations_;
private:
    AssetId id_;
};
struct LoadResult
{
    AssetHandle asset;
    AssetError error{AssetError::None};
    std::string diagnostic{};
    explicit operator bool() const { return bool(asset) && error == AssetError::None; }
};
class AssetProvider
{
public:
    virtual ~AssetProvider() = default;
    virtual LoadResult Load(AssetId id, std::span<const std::byte> bytes) = 0;
};
LoadResult LoadModel(AssetId id, std::span<const std::byte> bytes, AssetProvider& provider);

enum class AttachmentKind { Weapon, Shield, Hair, Armor, MountRider, Other };
struct AttachmentBinding
{
    AttachmentKind kind{AttachmentKind::Other};
    BoneId bone{-1};
    explicit operator bool() const { return bone >= 0; }
};
AttachmentBinding ResolveAttachment(const SkeletonAsset& skeleton, std::string_view bone,
    AttachmentKind kind = AttachmentKind::Other);
}
