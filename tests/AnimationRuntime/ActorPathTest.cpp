#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterLib/Camera.h"
#include "EterLib/ResourceManager.h"
#include "PackLib/PackManager.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AssetRuntime/Granny/GrannyAssetProvider.h"
#include "AssetRuntime/Granny/GrannyAnimationAdapter.h"
#include "AnimationRuntime/AnimationRuntime.h"
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Renderer/SkinningBenchmark.h"
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

float CCamera::CAMERA_MAX_DISTANCE = 2500.f;
static void Check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
#include "../Renderer/SkinningGpuReadback.h"
using namespace Renderer;

namespace
{
unsigned warnings = 0;
void DILIGENT_CALL_TYPE Message(Diligent::DEBUG_MESSAGE_SEVERITY severity, const char* message,
    const char*, const char*, int)
{
    if (severity >= Diligent::DEBUG_MESSAGE_SEVERITY_WARNING) { ++warnings; std::cerr << message << '\n'; }
}
AssetRuntime::AssetHandle Load(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    Check(bool(input), "Real actor asset exists");
    const auto count = input.tellg(); Check(count > 0, "Real actor asset nonempty");
    std::vector<std::byte> bytes(static_cast<std::size_t>(count));
    input.seekg(0); input.read(reinterpret_cast<char*>(bytes.data()), count); Check(bool(input), "Real actor asset read");
    auto loaded = AssetRuntime::LoadModel(path.string(), bytes, AssetRuntime::GetGrannyAssetProvider());
    Check(bool(loaded), "Real asset loaded through production provider"); return std::move(loaded.asset);
}
struct Model
{
    CGrannyModel* model{};
    explicit Model(const std::filesystem::path& path)
    {
        auto loaded = Load(path); model = new CGrannyModel;
        Check(model->CreateFromAsset(loaded.Model(0)), "Production model consumer accepts GR2 provider");
        Check(model->CaptureActorSource(), "Production actor source capture");
    }
    ~Model() { if (model) model->Release(); }
    Model(const Model&) = delete;
};
struct Motion
{
    CGrannyMotion motion;
    explicit Motion(const std::filesystem::path& path)
    {
        auto loaded = Load(path); Check(motion.BindAsset(loaded.Animation(0)), "Production motion consumer accepts clip");
    }
};

void Deform(CGrannyModelInstance& actor, ActorPart part, ActorCategory category)
{
    Math::Matrix world; Math::MatrixIdentity(&world);
    ActorInstanceSet targets; targets.instances[static_cast<unsigned>(part)] = &actor;
    targets.category = category; targets.gpuSkinning = true;
    const auto referenceBefore = AssetRuntime::referencePoseSamples.load();
    const auto importsBefore = AssetRuntime::importPoseSamples.load();
    { ActorDeformScope scope(targets); actor.Deform(&world); }
    Check(AssetRuntime::referencePoseSamples.load() == referenceBefore, "Real opt-in actor frame has no Granny pose sampling");
    Check(AssetRuntime::importPoseSamples.load() == importsBefore, "Real actor frame performs no import/resampling conversion");
    Check(actor.GetActorRenderData().ready, "Real actor current render data ready");
    const bool skinned = actor.GetModel()->GetActorSource()->deformVertexCount != 0;
    Check(actor.GetActorRenderData().gpuPrototype == skinned, "Existing GPU deformation chosen for every deformable part");
    Check(actor.GetActorRenderData().vertices.empty(), "GPU actor produces no CPU deformation copy");
}

void Draw(CGrannyModelInstance& actor, DiligentActorRenderer& renderer, const TerrainTexturePtr& texture,
    ActorPart part, ActorCategory category)
{
    auto* model = actor.GetModel(); auto& data = actor.GetActorRenderData();
    const auto& source = *model->GetActorSource();
    if (!data.geometry) data.geometry = renderer.CreateGeometry(source, part, category);
    StaticObjectDraw draw;
    const Math::Vector3 eye(0,-500,200), center(0,0,90), up(0,0,1);
    Math::Matrix view, projection;
    Math::MatrixLookAtRH(&view, &eye, &center, &up);
    Math::MatrixOrthoRH(&projection, 400, 400, 1, 1500);
    std::memcpy(draw.matrices.view.data(), &view, 64); std::memcpy(draw.matrices.projection.data(), &projection, 64);
    draw.ambient = {1,1,1,1}; draw.diffuse = {0,0,0,1}; draw.cull = StaticObjectCull::None;
    draw.normalizeNormals = true;
    const auto before = renderer.DrawCount();
    for (int m=0; m<model->GetMeshCount(); ++m)
    {
        const auto* mesh = model->GetMeshPointer(m);
        const auto* world = actor.GetStaticObjectWorldMatrix(m);
        std::memcpy(draw.matrices.world.data(), world, 64);
        Math::Matrix normal = *world * view;
        Check(Math::MatrixInverse(&normal, nullptr, &normal) != nullptr, "Attachment normal matrix invertible");
        Math::MatrixTranspose(&normal, &normal); std::memcpy(draw.normalTransform.data(), &normal, 64);
        draw.baseVertex = mesh->GetVertexBasePosition() + (mesh->CanDeformPNTVertices() ? 0 : source.deformVertexCount);
        draw.vertexCount = mesh->GetVertexCount();
        for (auto type : {CGrannyMaterial::TYPE_DIFFUSE_PNT, CGrannyMaterial::TYPE_BLEND_PNT})
            for (auto* group = mesh->GetTriGroupNodeList(type); group; group=group->pNextTriGroupNode)
            {
                draw.firstIndex = group->idxPos; draw.indexCount = group->triCount * 3;
                renderer.Draw(&actor, data.geometry, texture, draw, category, part);
            }
    }
    Check(!renderer.Failed() && renderer.DrawCount() > before, "Real actor submitted existing GPU material draws");
}

void Frame(DiligentD3D11Backend& backend, DiligentActorRenderer& renderer)
{
    Check(backend.BeginFrame(), "Native frame begins"); renderer.ResetFrame(); ++actorFrameSerial;
    backend.Clear({true, ClearColor{0,0,0,1}});
}
void Finish(DiligentD3D11Backend& backend, DiligentActorRenderer& renderer)
{
    std::vector<std::uint8_t> image; std::uint32_t width{}, height{};
    Check(backend.CaptureRGB(image,width,height), "Native rendered frame readback");
    Check(std::count_if(image.begin(),image.end(),[](std::uint8_t c){return c>32;})>300, "Real actor renders nonempty pixels");
    backend.EndFrame(); backend.Present(); renderer.ReleaseBindings();
}

void Warrior(const std::filesystem::path& root, DiligentD3D11Backend& backend,
    DiligentActorRenderer& renderer, const TerrainTexturePtr& texture)
{
    const auto base = root / "PC/ymir work/pc/warrior";
    Model full(base/"warrior_novice.gr2"), reduced(base/"warrior_novice_lod_03.gr2"), hairModel(base/"hair/hair_1_1.gr2");
    Model weaponModel(root/"item/ymir work/item/weapon/00010.gr2");
    Motion motion(base/"general/run.gr2");
    CGrannyModelInstance actor, farActor, hair, weapon;
    actor.SetMainModelPointer(full.model,nullptr); farActor.SetMainModelPointer(reduced.model,nullptr);
    actor.SetMotionPointer(&motion.motion); farActor.SetMotionPointer(&motion.motion);
    CGrannyModelInstance* owner = &actor;
    hair.SetLinkedModelPointer(hairModel.model,nullptr,&owner,true);
    weapon.SetMainModelPointer(weaponModel.model,nullptr);
    int hand = -1; Check(actor.GetBoneIndexByName("Bip01 R Hand",&hand) && hand>=0, "Weapon attachment bone resolves in original order");
    weapon.SetParentModelInstance(&actor,hand);
    std::vector<std::shared_ptr<const BoneRemap>> firstRemap;
    unsigned step=0;
    for (auto* current : {&actor,&farActor,&actor})
    {
        Frame(backend,renderer);
        owner=current; current->SetLocalTime(.17f+step*.1f); current->Update(120);
        Deform(*current,ActorPart::Body,ActorCategory::Player);
        const auto palette = current->GetSkinningPalette(); Check(bool(palette), "Opt-in actor publishes current GPU palette");
        const auto poseCount = AssetRuntime::independentPoseSamples.load();
        Deform(hair,ActorPart::Hair,ActorCategory::Player);
        Check(AssetRuntime::independentPoseSamples.load()==poseCount && hair.GetSkinningPalette()==palette, "Hair reuses current body pose without duplicate sampling");
        if (step==0) firstRemap=hair.GetSkinningRemaps();
        if (step==2) Check(hair.GetSkinningRemaps()==firstRemap, "Near-far-near restores original hair bone mapping");
        Draw(*current,renderer,texture,ActorPart::Body,ActorCategory::Player);
        BackendTestAccess::ValidateUploadedPalette(backend,*palette);
        Draw(hair,renderer,texture,ActorPart::Hair,ActorCategory::Player);
        if (current==&actor)
        {
            Deform(weapon,ActorPart::Weapon,ActorCategory::Player);
            Draw(weapon,renderer,texture,ActorPart::Weapon,ActorCategory::Player);
            const auto* matrix=actor.GetBoneMatrixPointer(hand); Check(matrix!=nullptr,"Animated attachment matrix available");
            for (unsigned c=0;c<16;++c) Check(std::isfinite(matrix[c]),"Animated attachment matrix finite");
        }
        Finish(backend,renderer); ++step;
    }
    std::cout<<"ACTOR Warrior run: real provider -> independent pose -> existing GPU draw; hair near-far-near; weapon attachment PASS\n";
}

void Actor(const std::filesystem::path& root, const char* directory, const char* modelName,
    const char* clipName, ActorCategory category, DiligentD3D11Backend& backend,
    DiligentActorRenderer& renderer, const TerrainTexturePtr& texture)
{
    const auto base=root/directory; Model model(base/modelName); Motion motion(base/clipName);
    CGrannyModelInstance actor; actor.SetMainModelPointer(model.model,nullptr); actor.SetMotionPointer(&motion.motion);
    for(float time:{.1f,.37f,.7f})
    {
        Frame(backend,renderer); actor.SetLocalTime(time); actor.Update(120);
        Deform(actor,ActorPart::Body,category); Draw(actor,renderer,texture,ActorPart::Body,category);
        BackendTestAccess::ValidateUploadedPalette(backend,*actor.GetSkinningPalette());
        Finish(backend,renderer);
    }
    std::cout<<"ACTOR "<<modelName<<' '<<clipName<<" independent pose and production GPU draw PASS\n";
}
}

int main(int argc,char**argv)
{
    HWND window=nullptr;
    try
    {
        Check(argc==2,"Real asset root required");
        Check(AssetRuntime::startupAnimationRuntime==AssetRuntime::AnimationRuntimeMode::Granny,"Production animation default remains Granny");
        Check(startupSkinningMode==PrototypeSkinningMode::GPU,"Production GPU skinning remains enabled");
        AssetRuntime::startupAnimationRuntime=AssetRuntime::AnimationRuntimeMode::ZiiNAN;
        CPackManager packs; CResourceManager resources;
        Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        window=CreateWindowW(L"STATIC",L"F1-X opt-in real actors",WS_OVERLAPPEDWINDOW,0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend; Check(window&&backend.Initialize({window,256,256}),"Native Diligent initializes");
        {
            DiligentActorRenderer renderer(backend); Check(renderer.Initialize(),"Production actor renderer initializes");
            actorRenderer=&renderer;actorWorldFrame=true;
            const std::uint8_t pixel[]{220,180,100,255};
            TerrainTextureData data{1,1,TerrainTextureFormat::RGBA8,{{pixel,4,4}}};
            auto texture=renderer.UploadTexture(data);Check(bool(texture),"Diagnostic actor texture");
            Warrior(argv[1],backend,renderer,texture);
            Actor(argv[1],"Monster/ymir work/monster/wolf","wolf.gr2","03.gr2",ActorCategory::Mob,backend,renderer,texture);
            Actor(argv[1],"NPC/ymir work/npc/horse","horse_normal.gr2","03.gr2",ActorCategory::Mount,backend,renderer,texture);
            Check(AssetRuntime::independentPoseSamples>0 && AssetRuntime::referencePoseSamples==0 && AssetRuntime::animationRuntimeFailures==0,
                "Opt-in actor frames use own sampling without hidden fallback");
            Check(skinningCpuCalls==0 && skinningFallbacks==0,"GPU actors have zero CPU deformation/fallbacks");
            texture.reset();renderer.ReleaseBindings();
            Check(!renderer.LiveGeometryCount()&&!renderer.LiveTextureCount()&&!livePrototypeGeometry&&!livePrototypePalettes&&!livePrototypeStaticMeshes,"GPU owners zero after actor cleanup");
            actorRenderer=nullptr;actorWorldFrame=false;actorDeformTargets={};
        }
        resources.Destroy();BackendTestAccess::Validate(backend);Check(warnings==0,"No Diligent warnings");
        backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(!AssetRuntime::liveDocuments&&!AssetRuntime::liveAnimationInstances&&!AssetRuntime::liveMeshBindings&&
            !AssetRuntime::liveIndependentAnimationInstances,"Persistent neutral cache retains no actor/source owners");
        AssetRuntime::GrannyAnimationAdapter::ClearImportCache();
        const auto counts=AnimationRuntime::GetLifetimeCounts();
        Check(!counts.skeletons&&!counts.clips&&!AssetRuntime::liveIndependentAnimationInstances&&!AssetRuntime::liveDocuments&&
            !AssetRuntime::liveAnimationInstances&&!AssetRuntime::liveMeshBindings&&!liveSkinMeshes&&!liveBoneRemaps&&!liveBonePalettes&&!AssetRuntime::retainedImportKeyBytes,"All animation/provider/skin/cache owners zero");
        std::cout<<"PASS real actor opt-in path, GPU no CPU deformation/fallback, native shutdown owners=0\n";return 0;
    }
    catch(const std::exception&error){std::cerr<<"FAIL "<<error.what()<<'\n';if(window)DestroyWindow(window);return 1;}
}
