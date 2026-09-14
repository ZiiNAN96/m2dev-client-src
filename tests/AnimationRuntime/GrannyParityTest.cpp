#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/SkinningDataAdapter.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AssetRuntime/Granny/Native.h"
#include "AssetRuntime/Granny/GrannyAssetProvider.h"
#include "AssetRuntime/Granny/GrannyAnimationAdapter.h"
#include "AssetRuntime/Granny/GrannyInterop.h"
#include "AnimationRuntime/AnimationRuntime.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Renderer/GpuSkinningPrototype.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
#include "../Renderer/SkinningGpuReadback.h"

namespace AR = AnimationRuntime;
namespace GA = AssetRuntime::GrannyAnimationAdapter;
using Renderer::BackendTestAccess;

namespace
{
constexpr double TranslationTolerance = 2e-4;
constexpr double RotationTolerance = 2e-5;
constexpr double ScaleTolerance = 2e-5;
constexpr double MatrixTolerance = 2e-3;
constexpr double VertexTolerance = 5e-3;
constexpr double NormalTolerance = 5e-5;
unsigned warnings = 0;
std::string expectedFailure;
void CaptureExpectedFailure(const char* text) { expectedFailure = text ? text : ""; }
void DILIGENT_CALL_TYPE Message(Diligent::DEBUG_MESSAGE_SEVERITY severity, const char* message,
    const char*, const char*, int)
{
    if (severity >= Diligent::DEBUG_MESSAGE_SEVERITY_WARNING) { ++warnings; std::cerr << message << '\n'; }
}

std::vector<std::byte> Read(const std::filesystem::path& path)
{
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    Check(bool(file), "Required original asset exists");
    const auto size = file.tellg(); Check(size > 0, "Nonempty original asset");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    file.seekg(0); file.read(reinterpret_cast<char*>(bytes.data()), size);
    Check(bool(file), "Complete asset read"); return bytes;
}

struct File
{
    granny_file* file{};
    granny_file_info* info{};
    AssetRuntime::AssetHandle handle;
    explicit File(const std::filesystem::path& path)
    {
        file = GrannyReadEntireFile(path.string().c_str()); Check(file != nullptr, "Independent native file");
        info = GrannyGetFileInfo(file); Check(info != nullptr, "Independent native file info");
        auto bytes = Read(path);
        auto result = AssetRuntime::LoadModel(path.string(), bytes, AssetRuntime::GetGrannyAssetProvider());
        Check(bool(result), "Original asset through GrannyAssetProvider"); handle = std::move(result.asset);
    }
    ~File() { handle.Reset(); if (file) GrannyFreeFile(file); }
    File(const File&) = delete;
};

struct Reference
{
    granny_model_instance* model{};
    granny_local_pose* local{};
    granny_world_pose* world{};
    granny_control* control{};
    std::size_t bones{};
    Reference(granny_model* source, granny_animation* animation)
        : bones(static_cast<std::size_t>(source->Skeleton->BoneCount))
    {
        model = GrannyInstantiateModel(source); local = GrannyNewLocalPose(static_cast<int>(bones));
        world = GrannyNewWorldPose(static_cast<int>(bones));
        Check(model && local && world, "Independent Granny pose allocation");
        control = GrannyPlayControlledAnimation(0, animation, model);
        Check(control != nullptr, "Original clip binds to original model");
        GrannySetControlLoopCount(control, 0);
        GrannySetControlEaseIn(control, false); GrannySetControlEaseOut(control, false);
    }
    void Sample(float time, const AR::Matrix* parent = nullptr, bool captureLocal = true)
    {
        GrannySetModelClock(model, time);
        GrannySampleModelAnimationsAccelerated(model, static_cast<int>(bones), parent ? parent->data() : nullptr, local, world);
        // Accelerated evaluation may bypass local scratch for direct world writes.
        // Obtain local reference values explicitly without replacing the world result.
        if (captureLocal) GrannySampleModelAnimations(model, 0, static_cast<int>(bones), local);
    }
    ~Reference()
    {
        if (control) GrannyFreeControl(control);
        if (model) GrannyFreeModelInstance(model);
        if (world) GrannyFreeWorldPose(world);
        if (local) GrannyFreeLocalPose(local);
    }
};

struct Errors
{
    double translation{}, rotation{}, scale{}, model{}, palette{}, position{}, normal{};
    std::size_t samples{}, vertices{}, gpuVertices{};
    void Validate(const std::string& label) const
    {
        if (translation > TranslationTolerance || rotation > RotationTolerance || scale > ScaleTolerance ||
            model > MatrixTolerance || palette > MatrixTolerance || position > VertexTolerance || normal > NormalTolerance)
        {
            std::cerr << "PARITY FAIL " << label << " t=" << translation << " r=" << rotation << " s=" << scale
                << " model=" << model << " palette=" << palette << " vertex=" << position << " normal=" << normal << '\n';
            throw std::runtime_error("Animation parity exceeds documented tolerance");
        }
    }
};

double Difference(float first, float second)
{
    Check(std::isfinite(first) && std::isfinite(second), "Parity components are finite");
    return std::abs(static_cast<double>(first) - second);
}
double QuaternionAngle(const AR::Quaternion& first, const float* second)
{
    double dot = 0, a = 0, b = 0;
    for (std::size_t c = 0; c < 4; ++c) { dot += double(first[c]) * second[c]; a += double(first[c]) * first[c]; b += double(second[c]) * second[c]; }
    Check(a > 0 && b > 0 && std::isfinite(dot), "Finite nonzero rotation quaternion");
    return 2 * std::acos(std::clamp(std::abs(dot) / std::sqrt(a * b), 0.0, 1.0));
}
void ComparePose(const Reference& reference, const AR::AnimationPose& pose,
    const std::vector<AR::Matrix>& model, const std::vector<AR::Matrix>& palette, Errors& error)
{
    const auto* composites = reinterpret_cast<const float*>(GrannyGetWorldPoseComposite4x4Array(reference.world));
    for (std::size_t bone = 0; bone < reference.bones; ++bone)
    {
        const auto* native = GrannyGetLocalPoseTransform(reference.local, static_cast<int>(bone));
        const auto& actual = pose.localTransforms[bone];
        const AR::LocalTransform identity;
        const auto* position = (native->Flags & GrannyHasPosition) ? native->Position : identity.translation.data();
        const auto* orientation = (native->Flags & GrannyHasOrientation) ? native->Orientation : identity.rotation.data();
        const auto* scale = (native->Flags & GrannyHasScaleShear) ? &native->ScaleShear[0][0] : identity.scaleShear.data();
        for (std::size_t c = 0; c < 3; ++c) error.translation = std::max(error.translation, Difference(actual.translation[c], position[c]));
        error.rotation = std::max(error.rotation, QuaternionAngle(actual.rotation, orientation));
        for (std::size_t c = 0; c < 9; ++c) error.scale = std::max(error.scale, Difference(actual.scaleShear[c], scale[c]));
        const auto* world = GrannyGetWorldPose4x4(reference.world, static_cast<int>(bone));
        for (std::size_t c = 0; c < 16; ++c)
        {
            error.model = std::max(error.model, Difference(model[bone][c], world[c]));
            error.palette = std::max(error.palette, Difference(palette[bone][c], composites[bone * 16 + c]));
        }
    }
    ++error.samples;
}

struct SkinSource
{
    std::shared_ptr<const Renderer::SkinningModelData> data;
    std::vector<std::shared_ptr<const Renderer::BoneRemap>> remaps;
    std::vector<Renderer::SkinningVertex> vertices;
    SkinSource(File& source, File& destination, std::shared_ptr<const Renderer::SkeletonLayout> skeleton)
    {
        data = SkinningDataAdapter::Extract(source.handle.Model(0)); Check(data && data->HasSkinnedMeshes(), "Original deformable skin data");
        remaps.resize(data->meshes.size());
        for (std::size_t m = 0; m < data->meshes.size(); ++m) if (data->meshes[m])
        {
            auto* binding = GrannyNewMeshBinding(source.info->Models[0]->MeshBindings[m].Mesh,
                source.info->Models[0]->Skeleton, destination.info->Models[0]->Skeleton);
            Renderer::SkinDataStatus status{};
            remaps[m] = SkinningDataAdapter::ExtractRemap(*data->meshes[m], binding, skeleton, status);
            if (binding) GrannyFreeMeshBinding(binding);
            Check(remaps[m] && status == Renderer::SkinDataStatus::Ready, "Native source-to-destination remap unchanged");
        }
    }
    void Prepare(const Renderer::BonePalette& palette)
    {
        std::vector<std::uint16_t> indices;
        Check(Renderer::BuildPrototypeVertices(*data, remaps, palette, vertices, indices), "Unchanged Phase-B vertex and binding preparation");
    }
};

std::array<float, 6> Skin(const Renderer::SkinningVertex& vertex, const std::vector<Renderer::SkinningMatrix>& palette)
{
    std::array<float, 6> result{};
    for (std::size_t influence = 0; influence < 4; ++influence) if (vertex.weights[influence])
    {
        const auto& matrix = palette.at(vertex.indices[influence]);
        const float weight = vertex.weights[influence] / 255.0f;
        for (std::size_t c = 0; c < 3; ++c)
        {
            result[c] += weight * (vertex.position[0] * matrix[c] + vertex.position[1] * matrix[4+c] + vertex.position[2] * matrix[8+c] + matrix[12+c]);
            result[c+3] += weight * (vertex.normal[0] * matrix[c] + vertex.normal[1] * matrix[4+c] + vertex.normal[2] * matrix[8+c]);
        }
    }
    return result;
}
void CompareVertices(SkinSource& source, const Renderer::BonePalette& reference, const Renderer::BonePalette& actual,
    Renderer::DiligentD3D11Backend& backend, bool gpu, Errors& error)
{
    if (source.vertices.empty()) source.Prepare(reference);
    for (const auto& vertex : source.vertices)
    {
        const auto expected = Skin(vertex, reference.matrices), sampled = Skin(vertex, actual.matrices);
        for (std::size_t c = 0; c < 3; ++c)
        {
            error.position = std::max(error.position, Difference(sampled[c], expected[c]));
            error.normal = std::max(error.normal, Difference(sampled[c+3], expected[c+3]));
        }
    }
    error.vertices += source.vertices.size();
    if (!gpu) return;
    const auto expected = BackendTestAccess::Skin(backend, source.vertices, reference);
    const auto sampled = BackendTestAccess::Skin(backend, source.vertices, actual);
    for (std::size_t vertex = 0; vertex < source.vertices.size(); ++vertex) for (std::size_t c = 0; c < 3; ++c)
    {
        error.position = std::max(error.position, Difference(sampled[vertex*2][c], expected[vertex*2][c]));
        error.normal = std::max(error.normal, Difference(sampled[vertex*2+1][c], expected[vertex*2+1][c]));
    }
    error.gpuVertices += source.vertices.size();
}

struct Example { const char* name; const char* directory; const char* model; std::vector<const char*> clips; };
void ClearReleasedImports();
void FailedImportLifetime(const std::filesystem::path& root)
{
    ClearReleasedImports();
    const auto base = root / "PC/ymir work/pc/warrior";
    File model(base/"warrior_novice.gr2"), clip(base/"general/wait.gr2");
    auto* animation = AssetRuntime::GrannyInterop::GetAnimation(clip.handle.Animation(0));
    Check(animation && animation->TrackGroupCount, "Owned native fixture animation");
    granny_curve_data_header* header = nullptr;
    const auto* group = animation->TrackGroups[0];
    for (int track = 0; track < group->TransformTrackCount && !header; ++track)
    {
        auto& curve = group->TransformTracks[track].OrientationCurve;
        if (GrannyCurveGetDegree(&curve) == 2 && GrannyCurveGetKnotCount(&curve) > 4)
            header = static_cast<granny_curve_data_header*>(curve.CurveData.Object);
    }
    Check(header != nullptr, "Owned fixture has a mutable quadratic curve");
    const auto baseline = AR::GetLifetimeCounts();
    {
        AssetRuntime::startupAnimationRuntime = AssetRuntime::AnimationRuntimeMode::ZiiNAN;
        auto session = model.handle.Model(0).GetDocument()->CreateAnimationInstance(model.handle.Model(0));
        AssetRuntime::startupAnimationRuntime = AssetRuntime::AnimationRuntimeMode::Granny;
        Check(session && session->PreparePose(), "Failure fixture owns neutral skeleton");
        auto* native = AssetRuntime::GrannyInterop::GetAnimationInstance(*session);
        Check(native != nullptr, "Failure fixture owns compatibility controls");
        const auto referenceBefore = AssetRuntime::referencePoseSamples.load(), importsBefore = AssetRuntime::importPoseSamples.load();
        const auto failuresBefore = AssetRuntime::animationRuntimeFailures.load();
        struct RestoreFixture
        {
            granny_curve_data_header* header;
            granny_uint8 degree;
            decltype(AssetRuntime::animationRuntimeErrorSink) sink;
            ~RestoreFixture() { header->Degree = degree; AssetRuntime::animationRuntimeErrorSink = sink; }
        };
        {
            RestoreFixture restore{header,header->Degree,AssetRuntime::animationRuntimeErrorSink};
            // Valid SDK degree, deliberately unsupported by this F1-X adapter;
            // modify only this test-owned in-memory curve and restore immediately.
            header->Degree = 3; expectedFailure.clear();
            AssetRuntime::animationRuntimeErrorSink = CaptureExpectedFailure;
            Check(session->SetMotion(clip.handle.Animation(0),0,0,0,1)==AssetRuntime::AssetError::EvaluationFailed,
                "Unsupported curve rejects opt-in animation visibly");
            Check(!expectedFailure.empty() && expectedFailure.find("curve") != std::string::npos &&
                AssetRuntime::animationRuntimeFailures==failuresBefore+1,"Exactly one explicit import diagnostic");
            Check(GrannyModelControlsBegin(native)==GrannyModelControlsEnd(native) && !session->IsPlaying(),
                "Failed import frees newly created native control");
            session->SetClock(.2f); session->FreeCompletedControls();
            const auto result=AssetRuntime::EvaluatePose(*session);
            Check(result.error==AssetRuntime::AssetError::EvaluationFailed && result.pose.values.empty(),
                "Failed explicit runtime does not produce stale/fallback pose");
            Check(AssetRuntime::referencePoseSamples==referenceBefore && AssetRuntime::importPoseSamples==importsBefore,
                "Unsupported curve fails before any reference/import pose sampling");
        }
        Check(session->SetMotion(clip.handle.Animation(0),0,0,0,1)==AssetRuntime::AssetError::None,
            "Restored supported clip recovers after failed import");
        session->SetClock(.2f);
        Check(AssetRuntime::EvaluatePose(*session).error==AssetRuntime::AssetError::None,
            "Recovered session evaluates owned runtime data");
    }
    // Drop source owners first; persistent neutral retention is released explicitly.
    model.handle.Reset(); clip.handle.Reset();
    ClearReleasedImports();
    const auto after = AR::GetLifetimeCounts();
    Check(after.skeletons==baseline.skeletons && after.clips==baseline.clips && !AssetRuntime::liveIndependentAnimationInstances,
        "Failed-import and recovery session releases runtime/control owners");
    std::cout << "FAILURE unsupported curve diagnostic/control cleanup/no fallback/recovery PASS\n";
}

AssetRuntime::AssetHandle LoadCacheSource(const char* id, const std::vector<std::byte>& bytes)
{
    auto loaded = AssetRuntime::LoadModel(id, bytes, AssetRuntime::GetGrannyAssetProvider());
    Check(bool(loaded), "Cache fixture loads valid original bytes through provider");
    return std::move(loaded.asset);
}
void CheckNoSourceOwners()
{
    Check(!AssetRuntime::liveDocuments && !AssetRuntime::liveAnimationInstances &&
        !AssetRuntime::liveMeshBindings && !AssetRuntime::liveIndependentAnimationInstances,
        "Persistent neutral cache owns no source documents, sessions or bindings");
}
void ClearReleasedImports()
{
    CheckNoSourceOwners();
    GA::ClearImportCache();
    const auto counts = AR::GetLifetimeCounts();
    Check(!counts.clips && !counts.skeletons && !AssetRuntime::retainedImportKeyBytes,
        "Explicit cache shutdown releases all neutral clip/skeleton/key owners");
}
std::size_t ActualKeyCapacity(const GA::ImportedAnimation& imported)
{
    std::size_t bytes = 0;
    for (const auto& clip : imported.boundaryClips) if (clip)
        for (const auto& track : clip->Tracks()) {
            bytes += track.translation.keys.capacity() * sizeof(AR::Keyframe<AR::Vector3>);
            bytes += track.rotation.keys.capacity() * sizeof(AR::Keyframe<AR::Quaternion>);
            bytes += track.scaleShear.keys.capacity() * sizeof(AR::Keyframe<AR::ScaleShear>);
        }
    return bytes;
}
void ImportCacheLifetime(const std::filesystem::path& root)
{
    ClearReleasedImports();
    constexpr std::size_t MiB = 1024 * 1024;
    Check(GA::RuntimeImportCache::ModelKeyByteLimit==128*MiB &&
        GA::RuntimeImportCache::ProcessKeyByteLimit==512*MiB, "Production content-cache limits are 128/512 MiB");
    const auto base = root / "PC/ymir work/pc/warrior";
    const auto modelBytes = Read(base/"warrior_novice.gr2");
    const auto idleBytes = Read(base/"general/wait.gr2");
    const auto walkBytes = Read(base/"general/walk.gr2");
    const auto runBytes = Read(base/"general/run.gr2");
    std::weak_ptr<AssetRuntime::AssetDocument> weakModel, weakAnimation;
    std::weak_ptr<const GA::ImportedAnimation> weakImport;
    std::uint64_t originalBinding = 0;
    const auto importsBefore = AssetRuntime::importedAnimationClips.load();
    const auto runSession = [](const AssetRuntime::ModelHandle& model, const AssetRuntime::AnimationHandle& animation) {
        const auto previousMode = AssetRuntime::startupAnimationRuntime;
        AssetRuntime::startupAnimationRuntime = AssetRuntime::AnimationRuntimeMode::ZiiNAN;
        auto session = model.GetDocument()->CreateAnimationInstance(model);
        AssetRuntime::startupAnimationRuntime = previousMode;
        Check(session && session->PreparePose(), "Content cache playback session");
        Check(session->SetMotion(animation,0,0,0,1)==AssetRuntime::AssetError::None, "Content cache motion bound");
        session->SetClock(.2f);
        Check(AssetRuntime::EvaluatePose(*session).error==AssetRuntime::AssetError::None, "Content cache pose evaluated");
    };
    {
        auto model = LoadCacheSource("cache/original-model.gr2",modelBytes);
        auto animation = LoadCacheSource("cache/original-animation.gr2",idleBytes);
        weakModel = model.Model(0).GetDocument(); weakAnimation = animation.Animation(0).GetDocument();
        runSession(model.Model(0),animation.Animation(0));
        const auto samples = AssetRuntime::importPoseSamples.load();
        runSession(model.Model(0),animation.Animation(0));
        Check(AssetRuntime::importedAnimationClips==importsBefore+1 && AssetRuntime::importPoseSamples==samples,
            "Second actor reuses import after the first actor is destroyed");
        std::string error;
        auto skeleton = GA::ImportSkeleton(model.Model(0),error);
        Check(bool(skeleton), "Real content-cache skeleton"); originalBinding = skeleton->BindingId();
        auto imported = GA::ImportAnimation(model.Model(0),animation.Animation(0),skeleton,error);
        Check(imported && imported->keyBytes && imported->keyBytes==ActualKeyCapacity(*imported),
            "Real import accounting equals allocated key vector capacities");
        weakImport = imported;
    }
    CheckNoSourceOwners();
    Check(weakModel.expired() && weakAnimation.expired() && !weakImport.expired() && AssetRuntime::retainedImportKeyBytes>0,
        "Neutral import survives destruction of every source document and actor without a source cycle");
    const auto samplesBeforeReload = AssetRuntime::importPoseSamples.load();
    {
        auto model = LoadCacheSource("another/location/model-copy.gr2",modelBytes);
        auto animation = LoadCacheSource("another/location/animation-copy.gr2",idleBytes);
        std::string error; auto skeleton = GA::ImportSkeleton(model.Model(0),error);
        Check(skeleton && skeleton->BindingId()==originalBinding, "Reload preserves neutral skeleton binding identity");
        auto imported = GA::ImportAnimation(model.Model(0),animation.Animation(0),skeleton,error);
        Check(imported && imported==weakImport.lock(), "Identical original bytes under different AssetIds reuse exact neutral import");
        runSession(model.Model(0),animation.Animation(0));
        Check(AssetRuntime::importedAnimationClips==importsBefore+1 && AssetRuntime::importPoseSamples==samplesBeforeReload,
            "Source reload and actor playback perform no SDK reimport");
        auto* cache = AssetRuntime::GrannyInterop::GetRuntimeImportCache(model.Model(0));
        Check(cache && !cache->Find(animation.Animation(0),1,skeleton->BindingId()) &&
            !cache->Find(animation.Animation(0),0,skeleton->BindingId()^1), "Model index and skeleton binding remain part of the content key");
    }
    CheckNoSourceOwners();
    {
        auto model = LoadCacheSource("cache/original-model.gr2",modelBytes);
        auto changedAnimation = LoadCacheSource("cache/original-animation.gr2",walkBytes);
        std::string error; auto skeleton = GA::ImportSkeleton(model.Model(0),error);
        const auto imports = AssetRuntime::importedAnimationClips.load();
        auto imported = GA::ImportAnimation(model.Model(0),changedAnimation.Animation(0),skeleton,error);
        Check(imported && imported!=weakImport.lock() && AssetRuntime::importedAnimationClips==imports+1,
            "Same animation AssetId with different valid source bytes cannot hit stale content");
    }
    {
        const auto changedBytes = Read(base/"warrior_novice_lod_03.gr2");
        auto changedModel = LoadCacheSource("cache/original-model.gr2",changedBytes);
        auto animation = LoadCacheSource("cache/original-animation.gr2",idleBytes);
        std::string error; auto skeleton = GA::ImportSkeleton(changedModel.Model(0),error);
        Check(skeleton && skeleton->BindingId()==originalBinding, "Changed model fixture retains same bone layout");
        const auto imports = AssetRuntime::importedAnimationClips.load();
        auto imported = GA::ImportAnimation(changedModel.Model(0),animation.Animation(0),skeleton,error);
        Check(imported && imported!=weakImport.lock() && AssetRuntime::importedAnimationClips==imports+1,
            "Same model AssetId and bone layout with different valid source bytes cannot hit stale content");
    }
    ClearReleasedImports(); Check(weakImport.expired(), "Explicit Clear releases source-independent retained import");

    // Bounded accounting fixtures exercise production byte limits without
    // allocating 512 MiB in the short gate. Real import capacity accounting is
    // checked above; only these small valid fixture clips declare larger sizes.
    const auto budgetFixture = [](const AR::RuntimeSkeleton& skeleton, std::size_t declaredBytes) {
        std::vector<AR::AnimationTrack> tracks(1);
        tracks[0].translation.keys.push_back({0,{0,0,0}});
        auto clip = std::make_shared<AR::RuntimeAnimationClip>(); std::string error;
        Check(clip->Initialize("bounded cache accounting fixture",1,false,std::move(tracks),skeleton,error),
            "Budget fixture contains a valid neutral clip");
        auto imported = std::make_shared<GA::ImportedAnimation>();
        imported->boundaryClips[0] = clip; imported->clip = clip; imported->keyBytes = declaredBytes;
        return imported;
    };
    {
        auto owner = LoadCacheSource("cache/budget-model.gr2",modelBytes);
        auto firstSource = LoadCacheSource("cache/budget-idle.gr2",idleBytes);
        auto secondSource = LoadCacheSource("cache/budget-run.gr2",runBytes);
        std::string error; auto skeleton = GA::ImportSkeleton(owner.Model(0),error);
        Check(bool(skeleton), "Model budget source skeleton");
        auto* cache = AssetRuntime::GrannyInterop::GetRuntimeImportCache(owner.Model(0));
        const auto footprint = GA::RuntimeImportCache::ModelKeyByteLimit/2+1;
        auto first = budgetFixture(*skeleton,footprint), second = budgetFixture(*skeleton,footprint);
        const auto evictions = AssetRuntime::importCacheEvictions.load();
        cache->Retain(owner.Model(0),firstSource.Animation(0),skeleton->BindingId(),first);
        cache->Retain(owner.Model(0),secondSource.Animation(0),skeleton->BindingId(),second);
        Check(AssetRuntime::importCacheEvictions==evictions+1 && cache->RetainedKeyBytes(0)==footprint &&
            AssetRuntime::retainedImportKeyBytes==footprint, "128 MiB model limit evicts oldest retained allocation");
        const auto imports = AssetRuntime::importedAnimationClips.load(), samples = AssetRuntime::importPoseSamples.load();
        auto activeReuse = GA::ImportAnimation(owner.Model(0),firstSource.Animation(0),skeleton,error);
        Check(activeReuse==first && AssetRuntime::importedAnimationClips==imports && AssetRuntime::importPoseSamples==samples &&
            cache->RetainedKeyBytes(0)==footprint, "Evicted active import remains reusable without rebaking or growing retention");
        std::weak_ptr<const GA::ImportedAnimation> weakFirst = first;
        activeReuse.reset(); first.reset();
        Check(weakFirst.expired() && !cache->Find(firstSource.Animation(0),0,skeleton->BindingId()),
            "Weak live lookup releases evicted import after final active owner disappears");
    }
    ClearReleasedImports();
    {
        const std::filesystem::path modelPaths[]{base/"warrior_novice.gr2",base/"warrior_novice_lod_03.gr2",base/"warrior_4-1.gr2",
            root/"Monster/ymir work/monster/wolf/wolf.gr2",root/"NPC/ymir work/npc/horse/horse_normal.gr2"};
        std::vector<AssetRuntime::AssetHandle> models;
        std::vector<std::shared_ptr<const AR::RuntimeSkeleton>> skeletons;
        std::vector<std::shared_ptr<GA::ImportedAnimation>> pinned;
        std::vector<GA::RuntimeImportCache*> caches;
        auto animation = LoadCacheSource("cache/global-budget-animation.gr2",idleBytes);
        const auto evictions = AssetRuntime::importCacheEvictions.load();
        for (std::size_t i=0;i<5;++i) {
            models.push_back(LoadCacheSource("cache/global-budget-model.gr2",Read(modelPaths[i])));
            std::string error; skeletons.push_back(GA::ImportSkeleton(models.back().Model(0),error));
            Check(bool(skeletons.back()), "Global budget source skeleton");
            caches.push_back(AssetRuntime::GrannyInterop::GetRuntimeImportCache(models.back().Model(0)));
            pinned.push_back(budgetFixture(*skeletons.back(),GA::RuntimeImportCache::ModelKeyByteLimit));
            if (i==4) Check(caches[0]->Find(animation.Animation(0),0,skeletons[0]->BindingId())==pinned[0], "Touch first model before global eviction");
            caches.back()->Retain(models.back().Model(0),animation.Animation(0),skeletons.back()->BindingId(),pinned.back());
            Check(AssetRuntime::retainedImportKeyBytes<=GA::RuntimeImportCache::ProcessKeyByteLimit,
                "Global retained accounting never exceeds 512 MiB");
        }
        Check(AssetRuntime::retainedImportKeyBytes==GA::RuntimeImportCache::ProcessKeyByteLimit &&
            AssetRuntime::importCacheEvictions==evictions+1 && caches[0]->RetainedKeyBytes(0)==GA::RuntimeImportCache::ModelKeyByteLimit &&
            caches[1]->RetainedKeyBytes(0)==0, "512 MiB process limit evicts least-recent model while preserving recently touched model");
        Check(caches[1]->Find(animation.Animation(0),0,skeletons[1]->BindingId())==pinned[1] && caches[1]->RetainedKeyBytes(0)==0,
            "Global eviction also preserves weak lookup of active imports");
    }
    ClearReleasedImports();
    GA::ClearImportCache(); // Idempotent after complete teardown.
    Check(!AssetRuntime::retainedImportKeyBytes, "Repeated explicit cache shutdown is harmless");
    std::cout << "CACHE source-unload/content reload/changed bytes/128-512MiB accounting/active eviction/explicit cleanup PASS\n";
}
void PlaybackParity(const std::filesystem::path& root)
{
    const auto base = root / "PC/ymir work/pc/warrior";
    File model(base/"warrior_novice.gr2"), idle(base/"general/wait.gr2"), run(base/"general/run.gr2");
    const auto owner = model.handle.Model(0);
    AssetRuntime::startupAnimationRuntime = AssetRuntime::AnimationRuntimeMode::Granny;
    auto reference = owner.GetDocument()->CreateAnimationInstance(owner);
    AssetRuntime::startupAnimationRuntime = AssetRuntime::AnimationRuntimeMode::ZiiNAN;
    auto runtime = owner.GetDocument()->CreateAnimationInstance(owner);
    AssetRuntime::startupAnimationRuntime = AssetRuntime::AnimationRuntimeMode::Granny;
    Check(reference && runtime && reference->PreparePose() && runtime->PreparePose(), "Independent provider playback sessions");
    double maximum = 0;
    auto compare = [&](float time)
    {
        reference->SetClock(time); runtime->SetClock(time);
        const auto expected = AssetRuntime::EvaluatePose(*reference);
        const auto nativeBefore = AssetRuntime::referencePoseSamples.load(), importsBefore = AssetRuntime::importPoseSamples.load();
        const auto actual = AssetRuntime::EvaluatePose(*runtime);
        Check(expected.error == AssetRuntime::AssetError::None && actual.error == AssetRuntime::AssetError::None &&
            expected.pose.values.size() == actual.pose.values.size(), "Playback paths both provide complete palettes");
        Check(AssetRuntime::referencePoseSamples == nativeBefore && AssetRuntime::importPoseSamples == importsBefore,
            "Playback frame does not sample/import through Granny");
        for (std::size_t i=0; i<actual.pose.values.size(); ++i) maximum = std::max(maximum,Difference(actual.pose.values[i],expected.pose.values[i]));
        if (maximum > MatrixTolerance) {
            std::cerr << "PLAYBACK difference time=" << time << " maximum=" << maximum << '\n';
            throw std::runtime_error("Playback rate/crossfade palette parity");
        }
        Check(reference->IsPlaying() == runtime->IsPlaying(), "Playback active state parity");
    };
    for (auto* session : {reference.get(),runtime.get()})
        Check(session->SetMotion(idle.handle.Animation(0),0,0,0,.5f)==AssetRuntime::AssetError::None,"Half-speed idle control");
    for (float time : {0.f,.2f,.6f}) compare(time);
    for (auto* session : {reference.get(),runtime.get()})
        Check(session->SetMotion(run.handle.Animation(0),.6f,.2f,0,2.f)==AssetRuntime::AssetError::None,"Single idle-run crossfade at double rate");
    for (float time : {.6f,.65f,.7f,.8f,1.2f}) compare(time);
    for (auto* session : {reference.get(),runtime.get()})
        Check(session->ChangeMotion(idle.handle.Animation(0),1.3f,0,1.f)==AssetRuntime::AssetError::None,"Rapid-transition initial clip");
    compare(1.3f);
    for (auto* session : {reference.get(),runtime.get()})
        Check(session->SetMotion(run.handle.Animation(0),1.4f,.3f,0,1.f)==AssetRuntime::AssetError::None,"Rapid-transition first fade");
    compare(1.4f);
    for (auto* session : {reference.get(),runtime.get()})
        Check(session->SetMotion(idle.handle.Animation(0),1.5f,.3f,0,1.f)==AssetRuntime::AssetError::None,"Three overlapping compatibility controls");
    for (float time : {1.5f,1.6f,1.7f,1.8f}) compare(time);
    for (auto* session : {reference.get(),runtime.get()})
        Check(session->ChangeMotion(run.handle.Animation(0),2.f,1,1.f)==AssetRuntime::AssetError::None,"Finite clip at normal rate");
    for (float time : {2.f,2.2f,2.5f}) compare(time);
    reference->SetMotionAtEnd(); runtime->SetMotionAtEnd(); compare(2.5f);
    std::cout << "PLAYBACK 0.5/1/2 rates, idle-run crossfade, overlapping transitions, finite end maxPalette=" << maximum << " PASS\n";
}
void RunExample(const std::filesystem::path& root, const Example& example,
    Renderer::DiligentD3D11Backend& backend, std::ostream& csv)
{
    const auto base = root / example.directory;
    File model(base / example.model);
    std::string diagnostic;
    auto skeleton = GA::ImportSkeleton(model.handle.Model(0), diagnostic);
    if (!skeleton) throw std::runtime_error("ImportSkeleton: " + diagnostic);
    Check(skeleton->Bones().size() == std::size_t(model.info->Models[0]->Skeleton->BoneCount), "Original bone count retained");
    for (std::size_t bone = 0; bone < skeleton->Bones().size(); ++bone)
    {
        const auto& original = model.info->Models[0]->Skeleton->Bones[bone];
        Check(skeleton->Bones()[bone].name == original.Name && skeleton->Bones()[bone].parent == original.ParentIndex, "Exact original bone names/order/parents");
    }
    auto skinData = SkinningDataAdapter::Extract(model.handle.Model(0));
    Check(bool(skinData), "Model skinning metadata");
    SkinSource body(model, model, skinData->skeleton);
    std::unique_ptr<File> hairFile;
    std::unique_ptr<SkinSource> hair;
    if (std::string(example.name).starts_with("Warrior"))
    {
        hairFile = std::make_unique<File>(base / "hair/hair_1_1.gr2");
        hair = std::make_unique<SkinSource>(*hairFile, model, skinData->skeleton);
    }
    for (const auto* clipName : example.clips)
    {
        File clip(base / clipName);
        const auto importStart = std::chrono::steady_clock::now();
        auto imported = GA::ImportAnimation(model.handle.Model(0), clip.handle.Animation(0), skeleton, diagnostic);
        if (!imported) throw std::runtime_error(std::string(example.name) + " " + clipName + " import: " + diagnostic);
        const auto importMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now()-importStart).count();
        Reference reference(model.info->Models[0], clip.info->Animations[0]);
        const auto bones = skeleton->Bones().size();
        AR::AnimationPose pose; pose.Prepare(bones);
        std::vector<AR::Matrix> worlds(bones), matrices(bones);
        Renderer::BonePalette expected, actual;
        expected.skeleton = actual.skeleton = skinData->skeleton;
        expected.matrices.resize(bones); actual.matrices.resize(bones);
        expected.ready = actual.ready = true;
        Errors errors, hairErrors;
        const float duration = clip.info->Animations[0]->Duration;
        const float epsilon = std::min(1e-4f, duration * .001f);
        const std::vector<float> times{0, duration*.1f, duration*.25f, duration*.5f, duration*.75f, duration*.9f,
            duration, duration-epsilon, duration+epsilon, duration*2+epsilon};
        for (std::size_t sample = 0; sample < times.size(); ++sample)
        {
            const float time = times[sample];
            const auto variant = time < duration ? 2u : 3u;
            Check(bool(imported->boundaryClips[variant]), "Independent boundary-specific clip data");
            reference.Sample(time);
            const auto before = AssetRuntime::referencePoseSamples.load();
            Check(AR::Sample(*skeleton, *imported->boundaryClips[variant], time, AR::TimeMode::Loop, pose), "Independent local sampling");
            Check(AR::Evaluate(*skeleton, pose, worlds), "Independent hierarchy evaluation");
            Check(AR::BuildPalette(*skeleton, worlds, matrices), "Independent composite palette generation");
            Check(AssetRuntime::referencePoseSamples.load() == before, "No provider Granny pose sampling in runtime frame");
            ComparePose(reference, pose, worlds, matrices, errors);
            const auto* native = GrannyGetWorldPoseComposite4x4Array(reference.world);
            std::memcpy(expected.matrices.data(), native, bones * sizeof(AR::Matrix));
            actual.matrices = matrices;
            CompareVertices(body, expected, actual, backend, sample == 2, errors);
            if (hair) CompareVertices(*hair, expected, actual, backend, sample == 2, hairErrors);
        }
        const AR::Matrix parent{1.3f,0,0,0,0,.7f,0,0,0,0,1.1f,0,7,-3,12,1};
        reference.Sample(duration*.25f, &parent);
        Check(AR::Sample(*skeleton, *imported->boundaryClips[2], duration*.25f, AR::TimeMode::Loop, pose), "Parented local pose");
        Check(AR::Evaluate(*skeleton, pose, worlds, &parent) && AR::BuildPalette(*skeleton, worlds, matrices), "Parented hierarchy and palette");
        ComparePose(reference, pose, worlds, matrices, errors);
        // Sampling measurements exclude one-time conversion and GPU readbacks.
        constexpr int repetitions = 64;
        const auto referenceStart = std::chrono::steady_clock::now();
        for (int i=0; i<repetitions; ++i) reference.Sample(duration*.37f, nullptr, false);
        const auto referenceUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now()-referenceStart).count()/repetitions;
        const auto runtimeStart = std::chrono::steady_clock::now();
        for (int i=0; i<repetitions; ++i)
        {
            Check(AR::Sample(*skeleton, *imported->boundaryClips[2], duration*.37f, AR::TimeMode::Loop, pose), "Sampling timing evaluation");
            Check(AR::Evaluate(*skeleton, pose, worlds) && AR::BuildPalette(*skeleton, worlds, matrices), "Timing hierarchy/palette evaluation");
        }
        const auto runtimeUs = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now()-runtimeStart).count()/repetitions;
        csv << example.name << ',' << clipName << ',' << bones << ',' << errors.samples << ',' << errors.translation << ',' << errors.rotation
            << ',' << errors.scale << ',' << errors.model << ',' << errors.palette << ',' << errors.position << ',' << errors.normal << ','
            << errors.vertices << ',' << errors.gpuVertices << ',' << referenceUs << ',' << runtimeUs << ',' << importMs << '\n';
        csv.flush();
        std::cout << "PARITY " << example.name << ' ' << clipName << " samples=" << errors.samples << " translation=" << errors.translation
            << " rotation=" << errors.rotation << " scale=" << errors.scale << " model=" << errors.model << " palette=" << errors.palette
            << " vertex=" << errors.position << " normal=" << errors.normal << " gpuVertices=" << errors.gpuVertices
            << " ignoredSourceTracks=" << imported->ignoredSourceTracks.size() << '\n';
        errors.Validate(std::string(example.name)+" "+clipName);
        if (hair)
        {
            hairErrors.Validate(std::string("Hair ")+clipName);
            csv << "Hair," << clipName << ',' << bones << ',' << times.size() << ",0,0,0,0,0," << hairErrors.position << ',' << hairErrors.normal
                << ',' << hairErrors.vertices << ',' << hairErrors.gpuVertices << ",0,0,0\n";
        }
    }
}
}

int main(int argc, char** argv)
{
    HWND window = nullptr;
    try
    {
        Check(argc == 3, "Real asset root and CSV output required");
        const std::filesystem::path output(argv[2]);
        std::filesystem::create_directories(output.parent_path());
        std::ofstream csv(output);
        Check(bool(csv), "Writable bounded parity evidence");
        csv << std::setprecision(10) << "asset,clip,bones,samples,translation,rotation_radians,scale,matrix,palette,position,normal,vertices,gpu_vertices,granny_us,ziinan_us,import_ms\n";
        Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        window = CreateWindowW(L"STATIC", L"F1-X animation parity", WS_OVERLAPPEDWINDOW, 0, 0, 64, 64, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        Renderer::DiligentD3D11Backend backend;
        Check(window && backend.Initialize({window,64,64}), "Unchanged Diligent backend available");
        const Example examples[]{
            {"Warrior", "PC/ymir work/pc/warrior", "warrior_novice.gr2", {"general/wait.gr2","general/walk.gr2","general/run.gr2","general/attack.gr2"}},
            {"WarriorLod3", "PC/ymir work/pc/warrior", "warrior_novice_lod_03.gr2", {"general/run.gr2"}},
            {"Wolf", "Monster/ymir work/monster/wolf", "wolf.gr2", {"00.gr2","02.gr2","03.gr2","20.gr2"}},
            {"Boss163", "Monster/ymir work/monster/misterious_diseased_bosshost", "misterious_diseased_bosshost.gr2", {"00.gr2","20.gr2"}},
            {"Horse", "NPC/ymir work/npc/horse", "horse_normal.gr2", {"00.gr2","02.gr2","03.gr2"}},
            {"ArmoredWarrior", "PC/ymir work/pc/warrior", "warrior_4-1.gr2", {"onehand_sword/run.gr2","onehand_sword/combo_01.gr2"}},
            {"StrayDog", "Monster/ymir work/monster/stray_dog", "stray_dog.gr2", {"03.gr2","20.gr2"}}
        };
        for (const auto& example : examples) RunExample(argv[1], example, backend, csv);
        PlaybackParity(argv[1]);
        FailedImportLifetime(argv[1]);
        ImportCacheLifetime(argv[1]);
        BackendTestAccess::Validate(backend); Check(warnings == 0, "No Diligent warnings");
        backend.Shutdown(); DestroyWindow(window); window = nullptr;
        ClearReleasedImports();
        const auto counts = AR::GetLifetimeCounts();
        Check(!counts.skeletons && !counts.clips && !AssetRuntime::liveDocuments && !AssetRuntime::liveAnimationInstances &&
            !Renderer::liveSkinMeshes && !Renderer::liveBoneRemaps && !Renderer::liveBonePalettes &&
            !AssetRuntime::retainedImportKeyBytes, "All runtime/provider/palette/cache owners released");
        std::cout << "PASS independent original-asset sampling/hierarchy/palette/GPU parity; runtime owners=0\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        if (window) DestroyWindow(window);
        std::cerr << "FAIL " << error.what() << '\n'; return 1;
    }
}
