#if defined(_DEBUG)
#include <crtdbg.h>
#endif
#include "EterGrnLib/StdAfx.h"
#include "EterGrnLib/ModelInstance.h"
#include "EterGrnLib/Thing.h"
#include "AssetRuntime/GR2ReaderMode.h"
#include "AssetRuntime/Providers.h"
#include "EterLib/Camera.h"
#include "EterLib/ResourceManager.h"
#include "PackLib/PackManager.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"

#include "AnimationRuntime/AnimationRuntime.h"
#include "Renderer/DiligentActorRenderer.h"
#include "Renderer/DiligentStaticObjectRenderer.h"
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
struct LoadedThing : CGraphicThing { using CGraphicThing::CGraphicThing; using CGraphicThing::OnLoad; };
void EmptyAnimationModels(const std::filesystem::path& root)
{
    for(const char* name:{"skill/swaeryeong.gr2","skill/geongon.gr2","skill/geomhwan.gr2","onehand_sword/walk.gr2","onehand_sword/run.gr2","onehand_sword/combo_03.gr2","onehand_sword/combo_04.gr2"}) {
        const auto path=root/"PC/ymir work/pc/sura"/name;
        std::ifstream input(path,std::ios::binary|std::ios::ate); Check(bool(input),"Sura animation fixture");
        std::vector<char> bytes(static_cast<std::size_t>(input.tellg())); input.seekg(0); input.read(bytes.data(),bytes.size());
        LoadedThing thing(path.string().c_str());
        Check(thing.OnLoad(static_cast<int>(bytes.size()),bytes.data()) && thing.GetMotionCount()>0 && thing.GetMotionPointer(0),"empty model metadata must not discard animation");
    }
    Check(AssetRuntime::grannyFileReads==0,"native empty-model adapter has no SDK reads");
}
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
    auto loaded = AssetRuntime::LoadModel(path.string(), bytes);
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
void Players(const std::filesystem::path& root,DiligentD3D11Backend& backend,
    DiligentActorRenderer& renderer,const TerrainTexturePtr& texture)
{
    for(const char* pack:{"PC","PC2"}) for(const char* name:{"warrior","assassin","sura","shaman"}) {
        const auto base=root/pack/"ymir work"/(std::string(pack)=="PC"?"pc":"pc2")/name;
        for(const char* armor:{"novice","4-1"}) {
            Model model(base/(std::string(name)+"_"+armor+".gr2"));
            Model hairModel(base/"hair/hair_1_1.gr2");
            const bool assassin=std::string_view(name)=="assassin",shaman=std::string_view(name)=="shaman";
            Model weaponModel(root/"item/ymir work/item/weapon"/(assassin?"01000.gr2":shaman?"07000.gr2":"00010.gr2"));
            CGrannyModelInstance actor,hair,weapon,leftWeapon; actor.SetMainModelPointer(model.model,nullptr);
            CGrannyModelInstance* owner=&actor; hair.SetLinkedModelPointer(hairModel.model,nullptr,&owner,true);
            int hand=-1; Check(actor.GetBoneIndexByName("Bip01 R Hand",&hand),"class right-hand attachment");
            weapon.SetMainModelPointer(weaponModel.model,nullptr); weapon.SetParentModelInstance(&actor,hand);
            if(assassin) {
                Check(actor.GetBoneIndexByName("Bip01 L Hand",&hand),"assassin left-hand attachment");
                leftWeapon.SetMainModelPointer(weaponModel.model,nullptr); leftWeapon.SetParentModelInstance(&actor,hand);
            }
            // Collision/selection callers can query bones before the first frame.
            const auto* initial=actor.GetBoneMatrixPointer(0);
            Check(initial && std::all_of(initial,initial+16,[](float x){return std::isfinite(x);}),"initial bone matrix before first frame");
            unsigned step=0;
            const std::string mode=assassin?"dualhand_sword":shaman?"fan":"onehand_sword";
            for(const std::string& clip:std::vector<std::string>{"general/wait","general/walk","general/run","general/attack",
                mode+"/combo_01",mode+"/combo_02","general/damage","general/wait"}) {
                Motion motion(base/(clip+".gr2"));
                actor.SetMotionPointer(&motion.motion);
                const auto* preserved=actor.GetBoneMatrixPointer(0);
                Check(preserved && std::all_of(preserved,preserved+16,[](float x){return std::isfinite(x);}),"motion switch preserves collision pose");
                Frame(backend,renderer); actor.SetLocalTime(.11f+step*.01f); actor.Update(120);
                Deform(actor,ActorPart::Body,ActorCategory::Player);
                Deform(hair,ActorPart::Hair,ActorCategory::Player);
                Draw(actor,renderer,texture,ActorPart::Body,ActorCategory::Player);
                Draw(hair,renderer,texture,ActorPart::Hair,ActorCategory::Player);
                Deform(weapon,ActorPart::Weapon,ActorCategory::Player);
                Draw(weapon,renderer,texture,ActorPart::Weapon,ActorCategory::Player);
                if(assassin) {
                    Deform(leftWeapon,ActorPart::Weapon,ActorCategory::Player);
                    Draw(leftWeapon,renderer,texture,ActorPart::Weapon,ActorCategory::Player);
                }
                Finish(backend,renderer); ++step;
            }
            std::cout<<"PLAYER "<<pack<<' '<<name<<' '<<armor<<" hair/weapon, initial collision pose, idle/walk/run/attack/combo_01/combo_02/hit/transitions PASS\n";
        }
    }
}
void StaticWorld(const std::filesystem::path& root,DiligentD3D11Backend& backend)
{
    DiligentStaticObjectRenderer renderer(backend); Check(renderer.Initialize(),"Production static renderer initializes");
    staticObjectRenderer=&renderer;
    StaticObjectLoadScope scope;
    auto loaded=Load(root/"guild/ymir work/guild/facility/gongjakso/gongjakso.gr2");
    auto* model=new CGrannyModel;
    Check(model->CreateFromAsset(loaded.Model(0)) && model->CaptureStaticObjectSource() && model->GetStaticObjectSource(),"Native GR2 static source");
    {
        CGrannyModelInstance instance; instance.SetMainModelPointer(model,nullptr);
        Math::Matrix world; Math::MatrixIdentity(&world); instance.DeformNoSkin(&world);
        Math::Vector3 minimum,maximum; instance.GetBoundBox(&minimum,&maximum);
        const auto center=(minimum+maximum)*.5f; const auto span=maximum-minimum;
        const float radius=std::max({span.x,span.y,span.z,1.f});
        const Math::Vector3 eye=center+Math::Vector3(radius,-radius*1.5f,radius),up(0,0,1);
        Math::Matrix view,projection; Math::MatrixLookAtRH(&view,&eye,&center,&up); Math::MatrixOrthoRH(&projection,radius*2.5f,radius*2.5f,1,radius*10);
        auto geometry=renderer.UploadGeometry(*model->GetStaticObjectSource()); Check(bool(geometry),"Existing static geometry upload");
        const std::uint8_t pixel[]{180,210,220,255}; TerrainTextureData textureData{1,1,TerrainTextureFormat::RGBA8,{{pixel,4,4}}};
        auto texture=renderer.UploadTexture(textureData); Check(bool(texture),"Static diagnostic texture");
        for(auto width:{256u,320u}) {
            Check(backend.Resize(width,256)&&backend.BeginFrame(),"Static resize/frame"); renderer.ResetFrame(); backend.Clear({true,ClearColor{0,0,0,1}});
            for(auto type:{CGrannyMaterial::TYPE_DIFFUSE_PNT,CGrannyMaterial::TYPE_BLEND_PNT})
                for(auto* node=model->GetMeshNodeList(CGrannyMesh::TYPE_RIGID,type);node;node=node->pNextMeshNode)
                    for(auto* group=node->pMesh->GetTriGroupNodeList(type);group;group=group->pNextTriGroupNode) {
                        StaticObjectDraw draw; std::memcpy(draw.matrices.world.data(),instance.GetStaticObjectWorldMatrix(node->iMesh),64);
                        std::memcpy(draw.matrices.view.data(),&view,64); std::memcpy(draw.matrices.projection.data(),&projection,64);
                        draw.normalTransform=AnimationRuntime::IdentityMatrix(); draw.ambient={1,1,1,1}; draw.diffuse={0,0,0,1}; draw.cull=StaticObjectCull::None;
                        draw.firstIndex=group->idxPos; draw.indexCount=group->triCount*3; draw.baseVertex=node->pMesh->GetVertexBasePosition(); draw.vertexCount=node->pMesh->GetVertexCount();
                        renderer.Draw(geometry,texture,draw);
                    }
            Check(!renderer.Failed()&&renderer.DrawCount()>0,"Real building uses production static draws");
            std::vector<std::uint8_t> image;std::uint32_t w{},h{};Check(backend.CaptureRGB(image,w,h),"Static frame readback");
            Check(std::count_if(image.begin(),image.end(),[](auto c){return c>32;})>300,"Building produces visible pixels");
            backend.EndFrame();backend.Present();renderer.ReleaseBindings();
        }
        Check(backend.Resize(0,0)&&backend.Resize(256,256),"Static suspend/restore");
    }
    model->Release(); loaded.Reset(); renderer.ReleaseBindings();
    Check(renderer.LiveGeometryCount()==0&&renderer.LiveTextureCount()==0,"Static GPU resources zero");
    staticObjectRenderer=nullptr;
    std::cout<<"STATIC WORLD native GR2 -> AssetRuntime -> production Diligent; resize/suspend/restore PASS\n";
}
}

int main(int argc,char**argv)
{
#if defined(_DEBUG)
    _CrtSetReportMode(_CRT_ASSERT,_CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT,_CRTDBG_FILE_STDERR);
#endif
    std::cout << std::unitbuf;
    HWND window=nullptr;
    try
    {
        Check(argc==2 || (argc==3 && std::string_view(argv[2])=="--lod-quick"),"Real asset root and optional --lod-quick required");
        const bool lodOnly=argc==3;
        Check(AssetRuntime::startupAnimationRuntime==AssetRuntime::AnimationRuntimeMode::ZiiNAN,"Production animation default is ZiiNAN");
        Check(AssetRuntime::startupGR2Reader==AssetRuntime::GR2ReaderMode::ZiiNAN,"Production reader default is ZiiNAN");
        Check(startupSkinningMode==PrototypeSkinningMode::GPU,"Production GPU skinning remains enabled");
        CPackManager packs; CResourceManager resources;
        Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        window=CreateWindowW(L"STATIC",L"F2-X native GR2 actors",WS_OVERLAPPEDWINDOW,0,0,256,256,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend; Check(window&&backend.Initialize({window,256,256}),"Native Diligent initializes");
        {
            DiligentActorRenderer renderer(backend); Check(renderer.Initialize(),"Production actor renderer initializes");
            actorRenderer=&renderer;actorWorldFrame=true;
            const std::uint8_t pixel[]{220,180,100,255};
            TerrainTextureData data{1,1,TerrainTextureFormat::RGBA8,{{pixel,4,4}}};
            auto texture=renderer.UploadTexture(data);Check(bool(texture),"Diagnostic actor texture");
            Warrior(argv[1],backend,renderer,texture);
            if(!lodOnly) {
            Players(argv[1],backend,renderer,texture);
            Actor(argv[1],"Monster/ymir work/monster/wolf","wolf.gr2","03.gr2",ActorCategory::Mob,backend,renderer,texture);
            Actor(argv[1],"Monster/ymir work/monster/misterious_diseased_bosshost","misterious_diseased_bosshost.gr2","20.gr2",ActorCategory::Mob,backend,renderer,texture);
            Actor(argv[1],"NPC/ymir work/npc/doctor","doctor.gr2","wait.gr2",ActorCategory::Npc,backend,renderer,texture);
            Actor(argv[1],"NPC/ymir work/npc/horse","horse_normal.gr2","03.gr2",ActorCategory::Mount,backend,renderer,texture);
            StaticWorld(argv[1],backend);
            EmptyAnimationModels(argv[1]);
            }
            const std::array<std::byte,16> invalid{};
            auto rejected=AssetRuntime::LoadModel("production-invalid.gr2",invalid);
            Check(!rejected && rejected.error!=AssetRuntime::AssetError::None && !rejected.diagnostic.empty() && AssetRuntime::grannyFileReads==0,"default rejects invalid GR2 with diagnostic and no reference fallback");
            Check(AssetRuntime::independentPoseSamples>0 && AssetRuntime::referencePoseSamples==0 && AssetRuntime::animationRuntimeFailures==0,
                "Production actor frames use own sampling without hidden fallback");
            Check(skinningCpuCalls==0 && skinningFallbacks==0,"GPU actors have zero CPU deformation/fallbacks");
            texture.reset();renderer.ReleaseBindings();
            Check(!renderer.LiveGeometryCount()&&!renderer.LiveTextureCount()&&!livePrototypeGeometry&&!livePrototypePalettes&&!livePrototypeStaticMeshes,"GPU owners zero after actor cleanup");
            actorRenderer=nullptr;actorWorldFrame=false;actorDeformTargets={};
        }
        resources.Destroy();BackendTestAccess::Validate(backend);Check(warnings==0,"No Diligent warnings");
        backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(!AssetRuntime::liveDocuments&&!AssetRuntime::liveAnimationInstances&&!AssetRuntime::liveMeshBindings&&
            !AssetRuntime::liveIndependentAnimationInstances,"Persistent neutral cache retains no actor/source owners");
        Check(AssetRuntime::importPoseSamples==0 && AssetRuntime::GR2::liveReaderDocuments==0,"No reference import or reader document leak");
        const auto counts=AnimationRuntime::GetLifetimeCounts();
        Check(!counts.skeletons&&!counts.clips&&!AssetRuntime::liveIndependentAnimationInstances&&!AssetRuntime::liveDocuments&&
            !AssetRuntime::liveAnimationInstances&&!AssetRuntime::liveMeshBindings&&!liveSkinMeshes&&!liveBoneRemaps&&!liveBonePalettes&&!AssetRuntime::retainedImportKeyBytes,"All animation/provider/skin/cache owners zero");
        std::cout<<"PASS real actor production path, GPU no CPU deformation/fallback, native shutdown owners=0\n";return 0;
    }
    catch(const std::exception&error){std::cerr<<"FAIL "<<error.what()<<'\n';if(window)DestroyWindow(window);return 1;}
}

