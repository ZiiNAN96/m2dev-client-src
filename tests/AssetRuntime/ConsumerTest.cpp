#include "AssetRuntime/AssetRuntime.h"
#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterLib/Camera.h"
#include "EterLib/ResourceManager.h"
#include "PackLib/PackManager.h"
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/SkinningBenchmark.h"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

// This translation unit includes no native provider API and never creates a native model.
using namespace AssetRuntime;
float CCamera::CAMERA_MAX_DISTANCE = 2500.f;
static void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
static constexpr Matrix4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
static unsigned sessions{}, evaluations{}, meshBindings{}, cpuAttempts{}, motionSets{}, vertexCopies{};

class FixtureBinding final : public MeshBinding
{
public:
    explicit FixtureBinding(ModelHandle source) : source_(std::move(source)) { ++meshBindings; }
    ~FixtureBinding() override { --meshBindings; }
    std::span<const std::int32_t> BoneIndices() const override { return indices_; }
    AssetError DeformVertices(std::span<std::byte>, std::span<const float>, bool) const override
    {
        ++cpuAttempts;
        return AssetError::UnsupportedLayout;
    }
private:
    ModelHandle source_;
    std::array<std::int32_t, 1> indices_{0};
};
class FixtureAnimation final : public AnimationInstance
{
public:
    explicit FixtureAnimation(ModelHandle owner) : owner_(std::move(owner)) { ++sessions; }
    ~FixtureAnimation() override { --sessions; }
    bool PreparePose() override { return true; }
    AssetError SetMotion(const AnimationHandle& clip, float localTime, float, int, float) override
    {
        if (!clip || clip.GetDocument() != owner_.GetDocument()) return AssetError::InvalidHandle;
        ++motionSets;
        clip_ = clip;
        clock_ = localTime;
        playing_ = true;
        return AssetError::None;
    }
    AssetError SetAnimation(const AnimationHandle& clip, float time) override { return SetMotion(clip, time, 0, 0, 1); }
    AssetError ChangeMotion(const AnimationHandle& clip, float time, int loops, float speed) override { return SetMotion(clip, time, 0, loops, speed); }
    AssetError CopyMotionFrom(AnimationInstance& source, float time, bool freeSource) override
    {
        auto* other = dynamic_cast<FixtureAnimation*>(&source);
        if (!other) return AssetError::ProviderMismatch;
        const auto result = SetMotion(other->clip_, time, 0, 0, 1);
        if (freeSource) other->playing_ = false;
        return result;
    }
    bool IsPlaying() const override { return playing_; }
    void SetMotionAtEnd() override { clock_ = 1; playing_ = false; }
    void SetClock(float time) override { clock_ = time; }
    void FreeCompletedControls() override {}
    void UpdateTransform(float, std::span<float, 16>) const override {}
    std::span<const float> BoneWorldMatrix(BoneId bone) const override { return bone == 0 ? std::span<const float>(matrix_) : std::span<const float>{}; }
    PoseView CompositePose() const override { return {matrix_}; }
    std::unique_ptr<MeshBinding> CreateMeshBinding(const ModelHandle& source, std::size_t mesh) const override
    {
        return source && mesh == 0 ? std::make_unique<FixtureBinding>(source) : nullptr;
    }
    PoseResult Evaluate(const PoseRequest& request) override
    {
        ++evaluations;
        matrix_ = identity;
        if (!request.attachmentMatrix.empty()) {
            if (request.attachmentMatrix.size() != 16) return {{}, AssetError::InvalidInput};
            std::copy(request.attachmentMatrix.begin(), request.attachmentMatrix.end(), matrix_.begin());
        }
        matrix_[12] += playing_ ? clock_ : 0;
        return {{matrix_}, AssetError::None};
    }
private:
    ModelHandle owner_;
    AnimationHandle clip_;
    Matrix4 matrix_{identity};
    float clock_{};
    bool playing_{};
};
class FixtureDocument final : public AssetDocument
{
public:
    FixtureDocument(AssetId id, bool skinned) : AssetDocument(std::move(id))
    {
        ModelAsset model;
        model.name = skinned ? "fixture-skinned" : "fixture-rigid";
        model.deformation = skinned ? Deformation::Skinned : Deformation::Rigid;
        model.skeleton.emplace();
        model.skeleton->name = "fixture-root";
        BoneAsset bone;
        bone.id = 0; bone.parentIndex = -1; bone.name = "root"; bone.inverseBind = identity;
        model.skeleton->bones.push_back(bone);
        model.animations.push_back(0);
        MaterialAsset material;
        material.name = "fixture-white";
        model.materials.push_back(material);
        MeshAsset mesh;
        mesh.name = model.name;
        mesh.vertexCount = mesh.indexCount = 3;
        mesh.indexWidth = IndexWidth::UInt16;
        mesh.deformation = model.deformation;
        mesh.vertexLayout = skinned ? VertexLayout::WeightedPositionNormalUV : VertexLayout::PositionNormalUV;
        mesh.sourceVertexStride = skinned ? 40 : 32;
        mesh.materialBindings = {0};
        mesh.materialGroups = {{0,0,3}};
        mesh.bounds = {{-.4f,-.4f,.5f}, {.4f,.4f,.5f}, true};
        mesh.skin.boneNames = {"root"};
        mesh.skin.meshToSkeleton = {0};
        mesh.skin.boneBounds = {mesh.bounds};
        mesh.skin.validRemap = true;
        if (skinned) {
            mesh.skin.normalizedByteWeights = mesh.skin.byteBoneIndices = true;
            mesh.skin.influencesPerVertex = 4;
            mesh.skin.weightOffset = 12; mesh.skin.indexOffset = 16;
        }
        model.meshes.push_back(std::move(mesh));
        models_.push_back(std::move(model));
        animations_.push_back({"fixture-translation", 1, 1.f/30.f, 1});
    }
    AssetError CopyVertices(std::size_t model, std::size_t mesh, VertexLayout layout, std::span<std::byte> output) const override
    {
        if (model != 0 || mesh != 0) return AssetError::InvalidHandle;
        if (released_) return AssetError::UploadDataReleased;
        if (layout != VertexLayout::PositionNormalUV && layout != VertexLayout::WeightedPositionNormalUV) return AssetError::UnsupportedLayout;
        const std::array<std::array<float, 8>, 3> pnt{{{-.4f,-.4f,.5f,0,0,1,0,0}, {.4f,-.4f,.5f,0,0,1,1,0}, {0,.4f,.5f,0,0,1,.5f,1}}};
        if (output.size() < VertexStride(layout) * 3) return AssetError::BufferTooSmall;
        ++vertexCopies;
        if (layout == VertexLayout::PositionNormalUV) std::memcpy(output.data(), pnt.data(), sizeof(pnt));
        else {
            struct WeightedVertex { float position[3]; std::uint8_t weights[4], bones[4]; float normal[3], uv[2]; };
            static_assert(sizeof(WeightedVertex) == 40);
            std::array<WeightedVertex, 3> weighted{};
            for (std::size_t v = 0; v < weighted.size(); ++v) {
                std::memcpy(weighted[v].position, pnt[v].data(), 12);
                weighted[v].weights[0] = 255;
                std::memcpy(weighted[v].normal, pnt[v].data() + 3, 12);
                std::memcpy(weighted[v].uv, pnt[v].data() + 6, 8);
            }
            std::memcpy(output.data(), weighted.data(), sizeof(weighted));
        }
        return AssetError::None;
    }
    AssetError CopyIndices(std::size_t model, std::size_t mesh, IndexWidth width, std::span<std::byte> output) const override
    {
        if (model != 0 || mesh != 0) return AssetError::InvalidHandle;
        if (released_) return AssetError::UploadDataReleased;
        const std::array<std::uint16_t, 3> narrow{0,1,2};
        const std::array<std::uint32_t, 3> wide{0,1,2};
        if (width != IndexWidth::UInt16 && width != IndexWidth::UInt32) return AssetError::InvalidIndexWidth;
        if (output.size() < 3 * static_cast<std::size_t>(width)) return AssetError::BufferTooSmall;
        if (width == IndexWidth::UInt16) std::memcpy(output.data(), narrow.data(), sizeof(narrow));
        else std::memcpy(output.data(), wide.data(), sizeof(wide));
        return AssetError::None;
    }
    void ReleaseUploadData() override { released_ = true; }
    std::unique_ptr<PoseEvaluator> CreatePose(const ModelHandle& model) const override { return CreateAnimationInstance(model); }
    std::unique_ptr<AnimationInstance> CreateAnimationInstance(const ModelHandle& model) const override
    {
        return model && model.GetDocument().get() == this ? std::make_unique<FixtureAnimation>(model) : nullptr;
    }
private:
    bool released_{};
};
class FixtureProvider final : public AssetProvider
{
public:
    explicit FixtureProvider(bool skinned) : skinned_(skinned) {}
    LoadResult Load(AssetId id, std::span<const std::byte>) override
    {
        return {AssetHandle(std::make_shared<FixtureDocument>(std::move(id), skinned_)), AssetError::None};
    }
private:
    bool skinned_;
};

int main()
{
    HWND window = nullptr;
    try {
        Check(Renderer::startupSkinningMode == Renderer::PrototypeSkinningMode::GPU, "GPU is still the production default");
        CPackManager packs;
        CResourceManager resources;
        window = CreateWindowW(L"STATIC", L"D1-X consumer provider substitution", WS_OVERLAPPEDWINDOW,
            0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Renderer::DiligentD3D11Backend backend;
        Check(window && backend.Initialize({window,256,256}), "Existing D3D11 renderer initializes");
        {
            Renderer::DiligentActorRenderer renderer(backend);
            Check(renderer.Initialize(), "Existing actor pipelines initialize");
            Renderer::actorRenderer = &renderer;
            Renderer::actorWorldFrame = true;
            const std::uint8_t pixel[]{255,255,255,255};
            Renderer::TerrainTextureData textureData{1,1,Renderer::TerrainTextureFormat::RGBA8,{{pixel,4,4}}};
            auto texture = renderer.UploadTexture(textureData);
            Check(bool(texture), "Fixture white texture");
            for (const bool skinned : {false, true}) {
                FixtureProvider provider(skinned);
                const std::byte byte{1};
                auto loaded = LoadModel(skinned ? "fixture/skinned" : "fixture/rigid", {&byte,1}, provider);
                auto model = std::make_unique<CGrannyModel>();
                Check(model->CreateFromAsset(loaded.asset.Model(0)), "Unchanged model consumer accepts a non-native provider");
                Check(model->GetAsset() && model->GetMeshCount() == 1 && model->GetVertexCount() == 3 && model->GetIdxCount() == 3,
                    "Model/mesh consumer preserves neutral metadata");
                Check(model->CaptureActorSource() && model->GetActorSource(), "Actor immutable geometry comes from neutral provider");
                if (!skinned) {
                    ++Renderer::staticObjectLoadDepth;
                    const bool captured = model->CaptureStaticObjectSource();
                    --Renderer::staticObjectLoadDepth;
                    Check(captured && model->GetStaticObjectSource() && model->GetStaticObjectSource()->vertices.size() == 3,
                        "Static model source uses the same neutral provider");
                }
                CGrannyMotion motion;
                Check(motion.BindAsset(loaded.asset.Animation(0)), "Motion consumer accepts neutral animation");
                CGrannyModelInstance actor;
                actor.SetMainModelPointer(model.get(), nullptr);
                actor.SetMotionPointer(&motion);
                const auto copiesAfterLoad = vertexCopies;
                std::weak_ptr<AssetDocument> owner = loaded.asset.Model(0).GetDocument();
                loaded.asset.Reset();
                for (const float time : {.1f, .25f}) {
                    Check(backend.BeginFrame(), "Fixture frame begins");
                    renderer.ResetFrame();
                    ++Renderer::actorFrameSerial;
                    backend.Clear({true,Renderer::ClearColor{0,0,0,1}});
                    actor.SetLocalTime(time);
                    actor.Update(120);
                    Math::Matrix world;
                    std::memcpy(&world, identity.data(), sizeof(world));
                    Renderer::ActorInstanceSet targets;
                    targets.instances[0] = &actor;
                    targets.gpuSkinning = true;
                    { Renderer::ActorDeformScope scope(targets); actor.Deform(&world); }
                    Check(!actor.IsEmpty() && actor.GetActorRenderData().ready, "Instance updates and produces current render data");
                    Check(actor.GetActorRenderData().gpuPrototype == skinned, "Generic weighted provider uses existing GPU path; rigid remains rigid");
                    const auto* bone = actor.GetBoneMatrixPointer(0);
                    Check(bone && std::abs(bone[12] - time) < 1e-6f, "Game-facing bone output comes from provider animation clock");
                    int rootIndex = -1;
                    Check(actor.GetBoneIndexByName("root", &rootIndex) && rootIndex == 0, "Game-facing attachment lookup uses neutral skeleton");
                    auto& renderData = actor.GetActorRenderData();
                    if (!renderData.geometry) renderData.geometry = renderer.CreateGeometry(*model->GetActorSource());
                    Renderer::StaticObjectDraw draw;
                    draw.matrices.view = draw.matrices.projection = identity;
                    std::memcpy(draw.matrices.world.data(), actor.GetStaticObjectWorldMatrix(0), 64);
                    draw.normalTransform = identity;
                    draw.cull = Renderer::StaticObjectCull::None;
                    draw.vertexCount = draw.indexCount = 3;
                    renderer.Draw(&actor, renderData.geometry, texture, draw);
                    Check(!renderer.Failed() && renderer.DrawCount() == 1, "Existing renderer draws non-native provider geometry");
                    std::vector<std::uint8_t> image;
                    std::uint32_t width{}, height{};
                    Check(backend.CaptureRGB(image,width,height), "Actual renderer readback");
                    Check(std::count_if(image.begin(), image.end(), [](std::uint8_t value) { return value > 64; }) > 100,
                        "Non-native triangle produced visible pixels");
                    backend.EndFrame(); backend.Present(); renderer.ReleaseBindings();
                }
                Check(vertexCopies == copiesAfterLoad, "Provider vertex data is not copied per frame");
                Check(!owner.expired(), "Consumers retain provider document after root handle unload");
                actor.Clear(); motion.Destroy();
                // Consumer model was initially retained once by CreateFromAsset; release through its normal owner.
                auto* retained = model.release(); retained->Release();
                Check(owner.expired() && liveDocuments == 0 && sessions == 0 && meshBindings == 0,
                    "Consumers release provider sessions, mesh bindings and document");
                std::cout << "CONSUMER PASS " << (skinned ? "skinned GPU" : "rigid/static") << '\n';
            }
            Check(cpuAttempts == 0 && Renderer::skinningCpuCalls == 0 && Renderer::skinningFallbacks == 0,
                "No CPU deformation or hidden fallback for provider substitution");
            Check(evaluations >= 4 && motionSets >= 2, "Production instance animation API exercised");
            texture.reset(); renderer.ReleaseBindings();
            Check(!renderer.LiveGeometryCount() && !renderer.LiveTextureCount(), "All renderer fixture resources released");
            Renderer::actorRenderer = nullptr; Renderer::actorWorldFrame = false; Renderer::actorDeformTargets = {};
        }
        resources.Destroy(); backend.Shutdown(); DestroyWindow(window); window = nullptr;
        std::cout << "PASS interchangeable provider reaches model, mesh, material, animation, static source and actual actor GPU rendering\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; if (window) DestroyWindow(window); return 1; }
}
