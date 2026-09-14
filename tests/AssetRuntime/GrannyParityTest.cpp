#include "AssetRuntime/AssetRuntime.h"
#include "AssetRuntime/Granny/GrannyAssetProvider.h"
#include "AssetRuntime/Granny/GrannyInterop.h"
#include "AssetRuntime/Granny/Native.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace AssetRuntime;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
static std::string Text(const char* value) { return value ? value : ""; }
static std::vector<std::byte> Read(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    Check(bool(input), "Required original fixture exists (never silently skipped)");
    const auto size = input.tellg();
    Check(size > 0, "Original fixture is nonempty");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    Check(bool(input), "Read complete original bytes");
    return bytes;
}
struct NativeFile
{
    granny_file* file{};
    granny_file_info* info{};
    explicit NativeFile(const std::filesystem::path& path)
    {
        file = GrannyReadEntireFile(path.string().c_str());
        Check(file != nullptr, "Independent original SDK reference load");
        info = GrannyGetFileInfo(file);
        Check(info != nullptr, "Reference file metadata");
    }
    ~NativeFile() { if (file) GrannyFreeFile(file); }
    NativeFile(const NativeFile&) = delete;
};

static bool TwoSided(const granny_material* material)
{
    if (!material) return false;
    granny_int32 value = 0;
    granny_data_type_definition field[] = {{GrannyInt32Member, "Two-sided"}, {GrannyEndMember}};
    granny_variant result{};
    if (GrannyFindMatchingMember(material->ExtendedData.Type, material->ExtendedData.Object, "Two-sided", &result) && result.Type)
        GrannyConvertSingleObject(result.Type, result.Object, field, &value, nullptr);
    return value == 1;
}
static void MaterialParity(const MaterialAsset& actual, granny_material* original)
{
    granny_texture* diffuse = nullptr;
    granny_texture* opacity = nullptr;
    if (original) {
        if (original->MapCount > 1 && original->Name && !_strnicmp(original->Name, "Blend", 5)) {
            diffuse = GrannyGetMaterialTextureByType(original->Maps[0].Material, GrannyDiffuseColorTexture);
            opacity = GrannyGetMaterialTextureByType(original->Maps[1].Material, GrannyDiffuseColorTexture);
        } else {
            diffuse = GrannyGetMaterialTextureByType(original, GrannyDiffuseColorTexture);
            opacity = GrannyGetMaterialTextureByType(original, GrannyOpacityTexture);
        }
    }
    Check(actual.name == (original ? Text(original->Name) : ""), "Original material name");
    Check(actual.textures[0] == (diffuse ? Text(diffuse->FromFileName) : "") &&
        actual.textures[1] == (opacity ? Text(opacity->FromFileName) : ""), "Legacy diffuse/opacity and Blend map translation");
    const bool blend = opacity != nullptr;
    Check(actual.stage == (blend ? MaterialStage::DiffuseOpacity : MaterialStage::Diffuse) && actual.blending == blend,
        "Legacy material stage and blend classification");
    Check(actual.culling == (TwoSided(original) ? Culling::None : Culling::Clockwise), "Legacy two-sided material culling");
    Check(actual.alphaTest && actual.depthWrite, "Neutral legacy alpha/depth defaults");
}

static void SkeletonParity(const SkeletonAsset& actual, granny_skeleton* original)
{
    Check(original && actual.name == Text(original->Name) && actual.bones.size() == original->BoneCount, "Skeleton name/count parity");
    for (std::size_t b = 0; b < actual.bones.size(); ++b) {
        const auto& bone = actual.bones[b];
        const auto& source = original->Bones[b];
        Check(bone.id == b && bone.name == Text(source.Name) && bone.parentIndex == source.ParentIndex,
            "Stable original bone ID, name, order and parent");
        Check(!std::memcmp(bone.inverseBind.data(), source.InverseWorld4x4, sizeof(Matrix4)), "Inverse bind matrices are byte-identical");
        Check(bone.localBind.flags == source.LocalTransform.Flags &&
            !std::memcmp(bone.localBind.position.data(), source.LocalTransform.Position, 12) &&
            !std::memcmp(bone.localBind.orientation.data(), source.LocalTransform.Orientation, 16) &&
            !std::memcmp(bone.localBind.scaleShear.data(), source.LocalTransform.ScaleShear, 36), "Local bind transform unchanged");
        granny_int32 index = -1;
        const bool found = GrannyFindBoneByName(original, source.Name, &index);
        Check(found && actual.FindBone(bone.name) == index, "Bone lookup agrees with SDK ordering");
        Check(ResolveAttachment(actual, bone.name, AttachmentKind::Weapon).bone == index, "Attachment bone resolves to original index");
    }
    Check(!ResolveAttachment(actual, "__D1X_missing_bone__", AttachmentKind::MountRider), "Invalid attachment bone stays invalid");
}

static std::size_t totalVertices{}, totalIndices{}, totalMeshes{}, totalBones{}, totalMaterials{};
static void IndexWidthFailure(const ModelHandle& model)
{
    // Change only a temporary in-process topology view; original files and loaded data stay intact.
    auto* native = GrannyInterop::GetMesh(model, 0);
    Check(native && native->PrimaryTopology, "Original topology for index-width boundary check");
    struct RestoreTopology {
        granny_mesh* mesh;
        granny_tri_topology* original;
        ~RestoreTopology() { mesh->PrimaryTopology = original; }
    } restore{native, native->PrimaryTopology};
    auto topology = *native->PrimaryTopology;
    const auto count = model.Get()->meshes[0].indexCount;
    std::vector<granny_int32> indices(count, 0);
    Check(!indices.empty(), "Nonempty original index fixture");
    indices[0] = 65536;
    topology.IndexCount = static_cast<granny_int32>(count);
    topology.Indices = indices.data();
    topology.Index16Count = 0;
    topology.Indices16 = nullptr;
    native->PrimaryTopology = &topology;
    std::vector<std::byte> narrow(count * 2, std::byte{0x5a});
    const auto unchanged = narrow;
    Check(model.GetDocument()->CopyIndices(model.Index(), 0, IndexWidth::UInt16, narrow) == AssetError::InvalidIndexWidth && narrow == unchanged,
        "32-bit index 65536 cannot silently truncate to 16 bits or partially write");
    std::vector<granny_int32> wide(count);
    Check(model.GetDocument()->CopyIndices(model.Index(), 0, IndexWidth::UInt32, std::as_writable_bytes(std::span(wide))) == AssetError::None && wide == indices,
        "Explicit 32-bit access preserves high index");
    indices[0] = -1;
    Check(model.GetDocument()->CopyIndices(model.Index(), 0, IndexWidth::UInt16, narrow) == AssetError::InvalidIndexWidth && narrow == unchanged,
        "Negative native index cannot silently narrow");
}

static void ModelParity(AssetDocument& document, std::size_t modelIndex, granny_model* original)
{
    const auto& actual = document.Models().at(modelIndex);
    Check(actual.name == Text(original->Name) && actual.meshes.size() == original->MeshBindingCount, "Model name and mesh count parity");
    Check(bool(actual.skeleton) == bool(original->Skeleton), "Optional skeleton presence parity");
    if (actual.skeleton) { SkeletonParity(*actual.skeleton, original->Skeleton); totalBones += actual.skeleton->bones.size(); }
    std::vector<granny_material*> materials;
    bool rigid = false, skinned = false;
    for (std::size_t m = 0; m < actual.meshes.size(); ++m) {
        const auto& mesh = actual.meshes[m];
        auto* source = original->MeshBindings[m].Mesh;
        Check(source && mesh.name == Text(source->Name), "Mesh name");
        Check(mesh.vertexCount == GrannyGetMeshVertexCount(source) && mesh.indexCount == GrannyGetMeshIndexCount(source), "Exact vertex/index counts");
        Check(mesh.indexWidth == static_cast<IndexWidth>(GrannyGetMeshBytesPerIndex(source)) && mesh.topology == PrimitiveTopology::TriangleList,
            "Index width and triangle topology");
        Check(mesh.sourceVertexStride == GrannyGetTotalObjectSize(GrannyGetMeshVertexType(source)), "Source vertex stride");
        const bool isRigid = GrannyMeshIsRigid(source);
        rigid |= isRigid;
        skinned |= !isRigid;
        Check(mesh.deformation == (isRigid ? Deformation::Rigid : Deformation::Skinned), "Rigid/skinned classification parity");
        Check(mesh.materialBindings.size() == source->MaterialBindingCount, "Mesh material binding count");
        for (std::size_t slot = 0; slot < mesh.materialBindings.size(); ++slot) {
            auto* material = source->MaterialBindings[slot].Material;
            auto where = std::find(materials.begin(), materials.end(), material);
            if (where == materials.end()) { materials.push_back(material); where = materials.end() - 1; }
            Check(mesh.materialBindings[slot] == static_cast<std::size_t>(where - materials.begin()), "Stable material deduplication and mesh association");
            MaterialParity(actual.materials.at(mesh.materialBindings[slot]), material);
        }
        Check(mesh.twoSided == (source->Name && !std::strncmp(source->Name, "2x", 2)), "Legacy mesh-name two-sided flag");
        const auto* groups = GrannyGetMeshTriangleGroups(source);
        Check(mesh.materialGroups.size() == GrannyGetMeshTriangleGroupCount(source), "Material triangle group count");
        for (std::size_t group = 0; group < mesh.materialGroups.size(); ++group) {
            Check(mesh.materialGroups[group].materialIndex == groups[group].MaterialIndex &&
                mesh.materialGroups[group].firstIndex == groups[group].TriFirst * 3 &&
                mesh.materialGroups[group].indexCount == groups[group].TriCount * 3, "Original material triangle ranges");
        }
        std::vector<granny_pnt332_vertex> vertices(mesh.vertexCount), copied(mesh.vertexCount);
        GrannyCopyMeshVertices(source, GrannyPNT332VertexType, vertices.data());
        Check(document.CopyVertices(modelIndex, m, VertexLayout::PositionNormalUV, std::as_writable_bytes(std::span(copied))) == AssetError::None,
            "Neutral vertex access succeeds");
        Check(!std::memcmp(vertices.data(), copied.data(), vertices.size() * sizeof(vertices[0])), "Vertex conversion bytes match independent SDK reference");
        if (!vertices.empty()) {
            std::array<float, 3> lo{vertices[0].Position[0], vertices[0].Position[1], vertices[0].Position[2]}, hi = lo;
            for (const auto& vertex : vertices) for (std::size_t c = 0; c < 3; ++c) {
                lo[c] = std::min(lo[c], vertex.Position[c]); hi[c] = std::max(hi[c], vertex.Position[c]);
            }
            Check(mesh.bounds.valid && mesh.bounds.min == lo && mesh.bounds.max == hi, "Original local-space position bounds");
        }
        for (auto width : {IndexWidth::UInt16, IndexWidth::UInt32}) {
            const auto bytes = mesh.indexCount * static_cast<std::size_t>(width);
            std::vector<std::byte> expected(bytes), indices(bytes);
            GrannyCopyMeshIndices(source, static_cast<granny_int32>(width), expected.data());
            Check(document.CopyIndices(modelIndex, m, width, indices) == AssetError::None && indices == expected,
                "Explicit 16/32-bit index access preserves SDK data");
        }
        Check(mesh.skin.boneNames.size() == source->BoneBindingCount && mesh.skin.boneBounds.size() == source->BoneBindingCount, "Skin bone metadata counts");
        for (std::size_t b = 0; b < mesh.skin.boneNames.size(); ++b) {
            Check(mesh.skin.boneNames[b] == Text(source->BoneBindings[b].BoneName), "Mesh binding bone order unchanged");
            Check(mesh.skin.boneBounds[b].valid &&
                !std::memcmp(mesh.skin.boneBounds[b].min.data(), source->BoneBindings[b].OBBMin, 12) &&
                !std::memcmp(mesh.skin.boneBounds[b].max.data(), source->BoneBindings[b].OBBMax, 12), "Bone binding OBB unchanged");
        }
        if (source->BoneBindingCount && original->Skeleton) {
            auto* binding = GrannyNewMeshBinding(source, original->Skeleton, original->Skeleton);
            Check(binding != nullptr, "Independent native self binding");
            const auto* indices = GrannyGetMeshBindingToBoneIndices(binding);
            Check(mesh.skin.validRemap && mesh.skin.meshToSkeleton.size() == source->BoneBindingCount, "Source remap complete");
            for (std::size_t b = 0; b < mesh.skin.meshToSkeleton.size(); ++b)
                Check(mesh.skin.meshToSkeleton[b] == indices[b], "Native source bone remap exact");
            GrannyFreeMeshBinding(binding);
        }
        if (mesh.vertexLayout == VertexLayout::WeightedPositionNormalUV) {
            Check(mesh.skin.normalizedByteWeights && mesh.skin.byteBoneIndices && mesh.skin.influencesPerVertex == 4 &&
                mesh.skin.weightOffset == 12 && mesh.skin.indexOffset == 16, "Phase-B byte weights/indices contract");
            std::vector<std::byte> weighted(mesh.vertexCount * 40);
            Check(document.CopyVertices(modelIndex, m, mesh.vertexLayout, weighted) == AssetError::None &&
                !std::memcmp(weighted.data(), GrannyGetMeshVertices(source), weighted.size()), "Original weighted vertex data unchanged");
        }
        totalVertices += mesh.vertexCount; totalIndices += mesh.indexCount; ++totalMeshes;
    }
    Check(actual.materials.size() == materials.size(), "Model material deduplication count");
    totalMaterials += materials.size();
    Check(actual.deformation == (rigid && skinned ? Deformation::Mixed : skinned ? Deformation::Skinned : Deformation::Rigid), "Model aggregate classification");
}

static void PoseParity(const std::filesystem::path& root)
{
    const auto body = root / "PC/ymir work/pc/warrior/warrior_novice.gr2";
    const auto animation = root / "PC/ymir work/pc/warrior/general/run.gr2";
    auto modelBytes = Read(body), clipBytes = Read(animation);
    auto modelLoad = LoadModel(body.string(), modelBytes, GetGrannyAssetProvider());
    auto clipLoad = LoadModel(animation.string(), clipBytes, GetGrannyAssetProvider());
    Check(modelLoad && clipLoad, "Separate model/clip provider loads");
    NativeFile source(body), clip(animation);
    Check(clip.info->AnimationCount > 0 && clipLoad.asset.Get()->Animations().size() == clip.info->AnimationCount, "Real animation count");
    for (std::size_t a = 0; a < clipLoad.asset.Get()->Animations().size(); ++a) {
        const auto& actual = clipLoad.asset.Get()->Animations()[a];
        const auto* native = clip.info->Animations[a];
        Check(actual.name == Text(native->Name) && actual.duration == native->Duration && actual.timeStep == native->TimeStep &&
            actual.trackGroupCount == native->TrackGroupCount, "Original animation name/duration/timestep/track count");
    }
    auto model = modelLoad.asset.Model(0);
    auto motion = clipLoad.asset.Animation(0);
    auto pose = modelLoad.asset.Get()->CreatePose(model);
    Check(bool(pose), "Provider owns a real pose evaluator");
    auto* nativeModel = source.info->Models[0];
    auto* instance = GrannyInstantiateModel(nativeModel);
    auto* local = GrannyNewLocalPose(nativeModel->Skeleton->BoneCount);
    auto* world = GrannyNewWorldPose(nativeModel->Skeleton->BoneCount);
    Check(instance && local && world, "Reference pose allocations");
    auto* control = GrannyPlayControlledAnimation(0, clip.info->Animations[0], instance);
    Check(control != nullptr, "Reference animation control");
    GrannySetControlLoopCount(control, 0);
    const Matrix4 transform{1.3f,0,0,0,0,.7f,0,0,0,0,1.1f,0,7,-3,12,1};
    for (const float time : {0.f, .37f}) {
        Check(pose->SetAnimation(motion, time) == AssetError::None, "Provider animation sampling clock");
        GrannySetModelClock(instance, time);
        GrannySampleModelAnimationsAccelerated(instance, nativeModel->Skeleton->BoneCount, transform.data(), local, world);
        auto evaluated = EvaluatePose(*pose, {transform});
        Check(evaluated.error == AssetError::None && evaluated.pose.Valid() && evaluated.pose.BoneCount() == nativeModel->Skeleton->BoneCount,
            "Pose boundary returns complete composite palette");
        const auto* reference = reinterpret_cast<const float*>(GrannyGetWorldPoseComposite4x4Array(world));
        for (std::size_t c = 0; c < evaluated.pose.values.size(); ++c)
            Check(std::isfinite(evaluated.pose.values[c]) && std::abs(evaluated.pose.values[c] - reference[c]) <= 1e-5f,
                "Native row-vector composite pose/multiplication parity");
    }
    // A pose owns the model and clip even after all external handles unload.
    modelLoad.asset.Reset(); clipLoad.asset.Reset(); model = {}; motion = {};
    Check(EvaluatePose(*pose).error == AssetError::None, "Pose retains model and animation lifetime");
    pose.reset();
    GrannyFreeControl(control); GrannyFreeWorldPose(world); GrannyFreeLocalPose(local); GrannyFreeModelInstance(instance);
}

// Independent reference follows the pre-D1-X ModelInstanceMotion control sequence.
struct ReferenceSession
{
    granny_model_instance* model{};
    granny_control* control{};
    granny_animation* animation{};
    granny_local_pose* local{};
    granny_world_pose* world{};
    int bones{};
    explicit ReferenceSession(granny_model* source) : bones(source->Skeleton->BoneCount)
    {
        model = GrannyInstantiateModel(source); local = GrannyNewLocalPose(bones); world = GrannyNewWorldPose(bones);
        Check(model && local && world, "Reference session allocations");
    }
    ~ReferenceSession() { GrannyFreeModelInstance(model); GrannyFreeWorldPose(world); GrannyFreeLocalPose(local); }
    void Set(granny_animation* clip, float time, float blend, int loops, float speed)
    {
        const bool first = !control;
        if (control) {
            GrannySetControlEaseOutCurve(control,time,time+blend,1,1,0,0);
            GrannySetControlEaseIn(control,false); GrannySetControlEaseOut(control,true);
            GrannyCompleteControlAt(control,time+blend); GrannyFreeControlIfComplete(control);
        }
        animation = clip; control = GrannyPlayControlledAnimation(time,clip,model);
        Check(control != nullptr, "Reference SetMotion control");
        GrannySetControlSpeed(control,speed); GrannySetControlLoopCount(control,loops);
        GrannySetControlEaseIn(control,!first); GrannySetControlEaseOut(control,false);
        if (!first && blend > 0) GrannySetControlEaseInCurve(control,time,time+blend,0,0,1,1);
        GrannyFreeControlOnceUnused(control);
    }
    void Change(granny_animation* clip, float startTime, int loops, float speed)
    {
        if (control) {
            GrannySetControlEaseIn(control,false); GrannySetControlEaseOut(control,false);
            GrannyCompleteControlAt(control,startTime); GrannyFreeControlIfComplete(control);
        }
        animation = clip; control = GrannyPlayControlledAnimation(startTime,clip,model);
        Check(control != nullptr, "Reference ChangeMotion control");
        GrannySetControlSpeed(control,speed); GrannySetControlLoopCount(control,loops);
        GrannySetControlEaseIn(control,false); GrannySetControlEaseOut(control,false);
        GrannyFreeControlOnceUnused(control);
    }
    void Copy(ReferenceSession& source, float time, bool freeSource)
    {
        if (control) GrannyFreeControl(control);
        animation = source.animation; control = GrannyPlayControlledAnimation(time,animation,model);
        Check(control != nullptr, "Reference CopyMotion control");
        GrannySetControlSpeed(control,GrannyGetControlSpeed(source.control));
        GrannySetControlLoopCount(control,GrannyGetControlLoopCount(source.control));
        GrannySetControlEaseIn(control,true); GrannySetControlEaseOut(control,false);
        GrannySetControlRawLocalClock(control,GrannyGetControlRawLocalClock(source.control));
        GrannyFreeControlOnceUnused(control);
        if (freeSource) { GrannyFreeControl(source.control); source.control = nullptr; }
    }
};
static void CompareSession(ReferenceSession& original, AnimationInstance& actual, float time)
{
    const Matrix4 parent{1.3f,0,0,0,0,.7f,0,0,0,0,1.1f,0,7,-3,12,1};
    GrannySetModelClock(original.model,time); actual.SetClock(time);
    GrannySampleModelAnimationsAccelerated(original.model,original.bones,parent.data(),original.local,original.world);
    auto result = EvaluatePose(actual,{parent});
    Check(result.error == AssetError::None && result.pose.BoneCount() == original.bones, "Session pose shape");
    const auto* expected = reinterpret_cast<const float*>(GrannyGetWorldPoseComposite4x4Array(original.world));
    for (std::size_t c = 0; c < result.pose.values.size(); ++c)
        Check(std::isfinite(result.pose.values[c]) && std::abs(result.pose.values[c]-expected[c]) <= 1e-5f,
            "Session blend/change/copy composite-pose parity");
    const auto* root = GrannyGetWorldPose4x4(original.world,0);
    const auto actualRoot = actual.BoneWorldMatrix(0);
    Check(actualRoot.size() == 16 && actual.BoneWorldMatrix(-1).empty() && actual.BoneWorldMatrix(original.bones).empty(),
        "Session attachment matrix bounds");
    for (std::size_t c = 0; c < actualRoot.size(); ++c)
        Check(std::abs(actualRoot[c]-root[c]) <= 1e-5f, "Session world-pose attachment parity");
    auto referenceTransform = parent, actualTransform = parent;
    GrannyUpdateModelMatrix(original.model,.03125f,referenceTransform.data(),referenceTransform.data(),false);
    actual.UpdateTransform(.03125f,actualTransform);
    for (std::size_t c = 0; c < actualTransform.size(); ++c)
        Check(std::abs(actualTransform[c]-referenceTransform[c]) <= 1e-5f, "Session root-motion matrix parity");
    Check(actual.IsPlaying() == (original.control && !GrannyControlIsComplete(original.control)), "Session playing state parity");
}
static void AnimationSessionParity(const std::filesystem::path& root)
{
    const auto base = root / "PC/ymir work/pc/warrior";
    NativeFile model(base / "warrior_novice.gr2"), run(base / "general/run.gr2"), wait(base / "general/wait.gr2");
    auto modelBytes = Read(base / "warrior_novice.gr2"), runBytes = Read(base / "general/run.gr2"), waitBytes = Read(base / "general/wait.gr2");
    auto loaded = LoadModel("fixture/session-body",modelBytes,GetGrannyAssetProvider());
    auto runClip = LoadModel("fixture/session-run",runBytes,GetGrannyAssetProvider());
    auto waitClip = LoadModel("fixture/session-wait",waitBytes,GetGrannyAssetProvider());
    Check(loaded && runClip && waitClip, "Original session model and clips");
    auto actual = loaded.asset.Get()->CreateAnimationInstance(loaded.asset.Model(0));
    auto copied = loaded.asset.Get()->CreateAnimationInstance(loaded.asset.Model(0));
    Check(actual && copied && actual->PreparePose() && copied->PreparePose(), "Provider session allocation");
    ReferenceSession reference(model.info->Models[0]), referenceCopy(model.info->Models[0]);
    Check(actual->SetMotion(runClip.asset.Animation(0),0,0,0,1.25f) == AssetError::None, "Session initial motion");
    reference.Set(run.info->Animations[0],0,0,0,1.25f);
    CompareSession(reference,*actual,.15f);
    Check(actual->SetMotion(waitClip.asset.Animation(0),.15f,.2f,0,.75f) == AssetError::None, "Session blend motion");
    reference.Set(wait.info->Animations[0],.15f,.2f,0,.75f);
    std::weak_ptr<AssetDocument> fadedClip = runClip.asset.Animation(0).GetDocument();
    runClip.asset.Reset();
    Check(!fadedClip.expired(), "Fading clip remains owned after resource cache unload");
    CompareSession(reference,*actual,.25f);
    CompareSession(reference,*actual,.5f);
    actual->FreeCompletedControls(); GrannyFreeCompletedModelControls(reference.model);
    Check(fadedClip.expired(), "Finished fade releases old clip owner");
    runClip = LoadModel("fixture/session-run",runBytes,GetGrannyAssetProvider());
    Check(actual->ChangeMotion(runClip.asset.Animation(0),.5f-.3f,0,1.1f) == AssetError::None, "Session motion change with legacy 0.3-second skip");
    reference.Change(run.info->Animations[0],.5f-.3f,0,1.1f);
    CompareSession(reference,*actual,.61f);
    Check(copied->CopyMotionFrom(*actual,.61f,false) == AssetError::None, "Session copy preserving source");
    referenceCopy.Copy(reference,.61f,false);
    CompareSession(referenceCopy,*copied,.74f);
    Check(copied->CopyMotionFrom(*actual,.74f,true) == AssetError::None, "Session copy releasing source");
    referenceCopy.Copy(reference,.74f,true);
    Check(!actual->IsPlaying(), "Copy/free clears source control");
    CompareSession(referenceCopy,*copied,.83f);
    copied->SetMotionAtEnd();
    GrannySetControlRawLocalClock(referenceCopy.control,GrannyGetControlLocalDuration(referenceCopy.control));
    CompareSession(referenceCopy,*copied,.83f);
    copied.reset(); actual.reset(); loaded.asset.Reset(); runClip.asset.Reset(); waitClip.asset.Reset();
    Check(liveDocuments == 0 && liveAnimationInstances == 0 && liveMeshBindings == 0, "Session/clip lifetime fully released");
    std::cout << "SESSION PASS blend/change/copy/end/root-motion/attachment/clip-lifetime\n";
}

static void UnmatchedWeaponClipParity(const std::filesystem::path& root)
{
    const auto weaponPath = root / "item/ymir work/item/weapon/00010.gr2";
    NativeFile weapon(weaponPath);
    auto weaponBytes = Read(weaponPath);
    auto loaded = LoadModel("fixture/weapon-motion-broadcast", weaponBytes, GetGrannyAssetProvider());
    Check(bool(loaded), "Original rigid weapon provider load for motion broadcast");
    auto bodyBytes = Read(root / "PC/ymir work/pc/warrior/warrior_novice.gr2");
    auto bodyLoad = LoadModel("fixture/body-motion-broadcast", bodyBytes, GetGrannyAssetProvider());
    Check(bool(bodyLoad), "Original body model for copying a playing motion");
    for (const char* name : {"wait", "run"}) {
        const auto clipPath = root / (std::string("PC/ymir work/pc/warrior/general/") + name + ".gr2");
        NativeFile clip(clipPath);
        auto clipBytes = Read(clipPath);
        auto clipLoad = LoadModel(clipPath.string(), clipBytes, GetGrannyAssetProvider());
        Check(bool(clipLoad), "Original actor clip provider load for motion broadcast");
        auto session = loaded.asset.Get()->CreateAnimationInstance(loaded.asset.Model(0));
        auto* native = GrannyInstantiateModel(weapon.info->Models[0]);
        Check(session && native, "Independent native and runtime rigid weapon sessions");
        granny_int32x group = -1;
        const bool matched = GrannyFindTrackGroupForModel(clip.info->Animations[0], weapon.info->Models[0]->Name, &group);
        auto* control = GrannyPlayControlledAnimation(0, clip.info->Animations[0], native);
        const auto result = session->SetMotion(clipLoad.asset.Animation(0), 0, 0, 0, 1);
        Check(!matched && !control && result == AssetError::NoMatchingTracks && !session->IsPlaying(),
            "Original actor clip has no rigid-weapon track group and SDK returns no control");
        auto* changedControl = GrannyPlayControlledAnimation(.2f, clip.info->Animations[0], native);
        const auto changed = session->ChangeMotion(clipLoad.asset.Animation(0), .2f, 0, 1);
        Check(!changedControl && changed == AssetError::NoMatchingTracks && !session->IsPlaying(),
            "Motion change preserves the SDK no-matching-tracks no-op");
        auto body = bodyLoad.asset.Get()->CreateAnimationInstance(bodyLoad.asset.Model(0));
        Check(body && body->SetMotion(clipLoad.asset.Animation(0), 0, 0, 0, 1) == AssetError::None && body->IsPlaying(),
            "Same clip successfully controls its original body model");
        auto* copiedControl = GrannyPlayControlledAnimation(.3f, clip.info->Animations[0], native);
        const auto copied = session->CopyMotionFrom(*body, .3f, true);
        Check(!copiedControl && copied == AssetError::NoMatchingTracks && !session->IsPlaying() && body->IsPlaying(),
            "Unmatched copy leaves the source playing, as the original early return did");
        std::cout << "WEAPON_MOTION model=" << Text(weapon.info->Models[0]->Name)
            << " clip=" << name << " matchingGroup=" << matched
            << " nativeControl=" << bool(control) << " start=" << ErrorName(result)
            << " change=" << ErrorName(changed) << " copy=" << ErrorName(copied) << '\n';
        if (control) GrannyFreeControl(control);
        GrannyFreeModelInstance(native);
    }
}

int main(int argc, char** argv)
{
    try {
        Check(argc == 2, "Explicit original asset root required");
        const std::filesystem::path root(argv[1]);
        Check(liveDocuments == 0, "No documents at start");
        const char* fixtures[] = {
            "PC/ymir work/pc/warrior/warrior_novice.gr2",
            "NPC/ymir work/npc/goods/goods.gr2",
            "Monster/ymir work/monster/wolf/wolf.gr2",
            "monster2/ymir work/monster2/fire_dragon/fire_dragon.gr2",
            "NPC/ymir work/npc/horse/horse_normal.gr2",
            "PC/ymir work/pc/warrior/hair/hair_1_1.gr2",
            "item/ymir work/item/weapon/00010.gr2",
            "Zone/ymir work/zone/n/obj/snow.m/snow-004-house2.gr2"
        };
        for (const auto* fixture : fixtures) {
            const auto path = root / fixture;
            NativeFile reference(path);
            auto bytes = Read(path);
            auto loaded = LoadModel(fixture, bytes, GetGrannyAssetProvider());
            Check(bool(loaded), "Provider loads original model");
            bytes.clear(); bytes.shrink_to_fit();
            Check(loaded.asset.Get()->Id() == fixture && loaded.asset.Get()->Models().size() == reference.info->ModelCount,
                "Provider owns loaded bytes and preserves file identity/count");
            for (std::size_t m = 0; m < loaded.asset.Get()->Models().size(); ++m)
                ModelParity(*loaded.asset.Get(), m, reference.info->Models[m]);
            Check(!loaded.asset.Model(std::numeric_limits<std::size_t>::max()), "Invalid model slot fails safely");
            auto model = loaded.asset.Model(0);
            IndexWidthFailure(model);
            auto* document = loaded.asset.Get();
            Check(document->CopyVertices(9999, 0, VertexLayout::PositionNormalUV, {}) == AssetError::InvalidHandle, "Invalid vertex handle fails");
            Check(document->CopyIndices(0, 9999, IndexWidth::UInt16, {}) == AssetError::InvalidHandle, "Invalid index handle fails");
            Check(document->CopyVertices(0, 0, VertexLayout::Unknown, {}) == AssetError::UnsupportedLayout, "Unknown vertex layout fails");
            Check(document->CopyIndices(0, 0, IndexWidth::Unknown, {}) == AssetError::InvalidIndexWidth, "Unknown index width fails");
            Check(document->CopyVertices(0, 0, VertexLayout::PositionNormalUV, {}) == AssetError::BufferTooSmall, "Small vertex destination rejected");
            Check(document->CopyIndices(0, 0, IndexWidth::UInt32, {}) == AssetError::BufferTooSmall, "Small index destination rejected");
            std::weak_ptr<AssetDocument> weak = model.GetDocument();
            const auto* metadata = model.Get();
            document->ReleaseUploadData(); document->ReleaseUploadData();
            Check(model.Get() == metadata && !model.Get()->meshes.empty(), "Release upload data preserves stable cached metadata");
            Check(document->CopyVertices(0, 0, VertexLayout::PositionNormalUV, {}) == AssetError::UploadDataReleased, "Explicit upload release never falls back");
            loaded.asset.Reset();
            Check(model && !weak.expired(), "Model handle retains its backing document");
            model = {};
            Check(weak.expired() && liveDocuments == 0, "Model unload releases all document resources");
            std::cout << "ASSET PASS " << fixture << '\n';
        }
        std::array<std::byte, 128> malformed{};
        auto bad = LoadModel("malformed.gr2", malformed, GetGrannyAssetProvider());
        Check(!bad && bad.error == AssetError::InvalidAsset && !bad.asset, "Malformed original bytes fail without a fallback handle");
        Check(LoadModel("missing.gr2", {}, GetGrannyAssetProvider()).error == AssetError::InvalidInput, "Missing bytes propagate failure");
        PoseParity(root);
        AnimationSessionParity(root);
        UnmatchedWeaponClipParity(root);
        Check(liveDocuments == 0, "All model, animation and pose ownership released");
        std::cout << "PASS eight original categories + animation/pose/failure/lifetime; meshes=" << totalMeshes
            << " vertices=" << totalVertices << " indices=" << totalIndices << " bones=" << totalBones
            << " materials=" << totalMaterials << " liveDocuments=" << liveDocuments << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL " << error.what() << '\n'; return 1; }
}
