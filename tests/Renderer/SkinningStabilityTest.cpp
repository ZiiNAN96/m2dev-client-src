// ZiiNAN: GPU skinning parity validation
// Reuse B4-X's real-asset fixture and unchanged numerical/raster tolerances.
#define main B4CoverageMain
#include "SkinningCoverageTest.cpp"
#undef main
#include "EterImageLib/ScreenshotJPEG.h"

static size_t paletteChecks=0, pixelChannels=0, pixelChanges=0, lifecycleCount=0;
static uint64_t pixelAbsolute=0;
static unsigned pixelMaximum=0;
static std::filesystem::path evidence;
static std::ofstream pixelLog;

static void PixelParity(Pair& pair,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,
    const TerrainTexturePtr& texture,const std::string& label,ActorPart part,ActorCategory category,bool save)
{
    auto draw=Camera(pair);uint32_t ac=0,bc=0;
    auto a=Image(pair.cpu,backend,renderer,texture,draw,part,category,ac);
    auto b=Image(pair.gpu,backend,renderer,texture,draw,part,category,bc);
    Check(a.size()==b.size() && ac && ac==bc,"B5 pixel image and draw sizes");
    if(pair.gpu.GetActorRenderData().gpuPrototype && !pair.gpu.GetModel()->GetRigidVertexCount()) {
        BackendTestAccess::ValidateUploadedPalette(backend,*pair.gpu.GetSkinningPalette());++paletteChecks;
    }
    uint64_t sum=0;unsigned maximum=0;size_t changed=0,large=0,visible=0;
    std::vector<uint8_t> difference(a.size());
    for(size_t i=0;i<a.size();++i) {
        const auto delta=unsigned(std::abs(int(a[i])-int(b[i])));
        sum+=delta;maximum=std::max(maximum,delta);changed+=delta!=0;large+=delta>2;
        difference[i]=static_cast<uint8_t>(std::min(255u,delta*16));
        visible+=a[i]!=a[i%3];
    }
    Check(visible>100,"Pixel comparison includes visible geometry, not two empty frames");
    Check(changed<=a.size()/100 && large<=a.size()/10000,"Unchanged B3/B4 raster tolerance");
    pixelChannels+=a.size();pixelAbsolute+=sum;pixelChanges+=changed;pixelMaximum=std::max(pixelMaximum,maximum);
    pixelLog<<label<<','<<double(sum)/a.size()<<','<<maximum<<','<<changed<<','<<large<<','<<a.size()<<'\n';pixelLog.flush();
    if(save) {
        const auto side=uint32_t(std::sqrt(double(a.size()/3)));Check(size_t(side)*side*3==a.size(),"Square comparison viewport");
        for(const auto& output:std::initializer_list<std::pair<const char*,const std::vector<uint8_t>*>>{{"cpu",&a},{"gpu",&b},{"diff-x16",&difference}}) {
            const auto path=evidence/(label+"-"+output.first+".jpg");
            Check(SaveScreenshotJPEG(path.c_str(),*output.second,side,side),"Saved B5 comparison image");
        }
    }
}

static void Released(DiligentActorRenderer& renderer,const char* transition)
{
    renderer.ReleaseBindings();
    Check(!livePrototypeGeometry && !livePrototypeStaticMeshes && !livePrototypePalettes,"Transition releases GPU mesh and palette owners");
    Check(!renderer.LiveGeometryCount() && !renderer.AttachmentGeometryCount() && !renderer.MountGeometryCount(),"Transition releases CPU/GPU actor geometry");
    std::cout<<"LIFETIME "<<transition<<" geometry=0 staticMeshes=0 palettes=0 attachments=0 mounts=0\n";
}

static void Players(const std::string& root,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,const TerrainTexturePtr& texture)
{
    for(const std::string folder:{"PC/ymir work/pc/","pc2/ymir work/pc2/"})
    for(const std::string race:{"warrior","assassin","sura","shaman"}) {
        const auto base=root+"/"+folder+race+"/";
        Asset body(base+race+"_novice.gr2"),a(base+race+"_4-1.gr2"),b(base+race+"_lord.gr2"),c(base+race+"_marry_01.gr2");
        Asset hairA(base+"hair/hair_1_1.gr2"),hairB(base+"hair/hair_2_1.gr2");
        const auto prefix=std::string(folder[0]=='P'?"pc-":"pc2-")+race;
        for(unsigned cycle=0;cycle<3;++cycle) {
            Pair actor,hair;unsigned shape=0;
            std::vector<std::unique_ptr<Clip>> clips;
            for(auto* asset:{&body,&a,&b,&c,&body}) {
                hair.cpu.Clear();hair.gpu.Clear();actor.Set(*asset);
                for(const std::string motion:{"wait","walk","run","attack","damage","dead"}) {
                    if(cycle && motion!="run" && motion!="attack") continue;
                    const auto path=base+"general/"+motion+".gr2";
                    if(!std::filesystem::exists(path)) { std::cout<<"UNAVAILABLE "<<prefix<<' '<<motion<<'\n';continue; }
                    clips.push_back(std::make_unique<Clip>(path));Frame(backend,renderer);
                    actor.Motion(*clips.back(),.19f+.13f*cycle);actor.Deform(World());
                    const auto label=prefix+"-cycle"+std::to_string(cycle)+"-shape"+std::to_string(shape)+"-"+motion;
                    Compare(actor,backend,renderer,texture,label,ActorPart::Body,ActorCategory::Player,cycle==0 && motion=="wait");
                    PixelParity(actor,backend,renderer,texture,label,ActorPart::Body,ActorCategory::Player,cycle==0 && shape<2 && motion=="wait");
                    if(motion=="run") for(auto* style:{&hairA,&hairB,&hairA}) {
                        hair.Set(*style,&actor);hair.Deform(World(),ActorPart::Hair);
                        Check(livePrototypePalettes==1,"Body and linked hair share the same current pose owner");
                        Compare(hair,backend,renderer,texture,label+"-hair",ActorPart::Hair,ActorCategory::Player,false);
                        PixelParity(hair,backend,renderer,texture,label+"-hair",ActorPart::Hair,ActorCategory::Player,false);
                    }
                    Finish(backend,renderer);
                }
                ++shape;
            }
            hair.cpu.Clear();hair.gpu.Clear();actor.cpu.Clear();actor.gpu.Clear();
            Released(renderer,"player-shape-cycle");++lifecycleCount;
        }
    }
}

static void ActorChurn(const std::string& root,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer,const TerrainTexturePtr& texture)
{
    struct Example {const char* path;const char* label;ActorCategory category;};
    for(const auto& example:std::initializer_list<Example>{
        {"NPC/ymir work/npc/goods/goods.gr2","goods",ActorCategory::Npc},
        {"NPC/ymir work/npc/blacksmith/blacksmith.gr2","blacksmith",ActorCategory::Npc},
        {"NPC/ymir work/npc/doctor/doctor.gr2","doctor",ActorCategory::Npc},
        {"NPC/ymir work/npc/sinseon/sinseon.gr2","sinseon",ActorCategory::Npc},
        {"Monster/ymir work/monster/wolf/wolf.gr2","wolf",ActorCategory::Mob},
        {"Monster/ymir work/monster/orc_soldier/orc_soldier.gr2","orc",ActorCategory::Mob},
        {"Monster/ymir work/monster/barbarian_bow/barbarian_bow.gr2","mixed-barbarian",ActorCategory::Mob},
        {"monster2/ymir work/monster2/fire_dragon/fire_dragon.gr2","dragon",ActorCategory::Mob},
        {"Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2","boss163",ActorCategory::Mob},
        {"NPC/ymir work/npc/horse/horse_normal.gr2","horse",ActorCategory::Mount},
        {"patch2/ymir work/npc/boar/boar.gr2","boar",ActorCategory::Mount},
        {"patch2/ymir work/npc/lion_white/lion_white.gr2","lion",ActorCategory::Mount},
        {"metin2_patch_halloween/ymir work/npc/horse_halloween1/horse_halloween1.gr2","halloween",ActorCategory::Mount},
        {"metin2_patch_pet1/ymir work/npc/dinosaur/dinosaur_3.gr2","dinosaur",ActorCategory::Mount},
        {"item/ymir work/item/weapon/00010.gr2","rigid-weapon",ActorCategory::Player}}) {
        Asset asset(root+"/"+example.path);const auto base=std::filesystem::path(asset.path).parent_path();
        std::vector<std::unique_ptr<Clip>> clips;std::vector<std::string> names;
        for(const auto* motion:{"wait","walk","run","attack","attack1","damage","dead","00","20","30","31"}) {
            const auto path=base/(std::string(motion)+".gr2");
            if(std::filesystem::exists(path)) { clips.push_back(std::make_unique<Clip>(path.string()));names.emplace_back(motion); }
        }
        if(clips.empty()) names.emplace_back("bind-only");
        const auto part=std::string(example.label)=="rigid-weapon"?ActorPart::Weapon:ActorPart::Body;
        for(unsigned cycle=0;cycle<8;++cycle) {
            Pair actor,other;actor.Set(asset);other.Set(asset);
            for(size_t motion=0;motion<names.size();++motion) {
                Frame(backend,renderer);
                if(!clips.empty()) { actor.Motion(*clips[motion],.17f+.03f*cycle);other.Motion(*clips[motion],.73f+.02f*cycle); }
                auto world=World();actor.Deform(world,part,example.category);world._41+=9;other.Deform(world,part,example.category);
                const auto label=std::string(example.label)+"-cycle"+std::to_string(cycle)+"-"+names[motion];
                if(cycle==0 || cycle==7) {
                    Compare(actor,backend,renderer,texture,label,part,example.category,cycle==0 && motion==0);
                    PixelParity(actor,backend,renderer,texture,label,part,example.category,cycle==0 && motion==0);
                }
                uint32_t count;const auto draw=Camera(actor);
                const auto before=Image(actor.gpu,backend,renderer,texture,draw,part,example.category,count);
                Image(other.gpu,backend,renderer,texture,draw,part,example.category,count);
                const auto after=Image(actor.gpu,backend,renderer,texture,draw,part,example.category,count);
                Check(before==after,"A B A instance order never reuses another actor's palette/SRB");
                if(actor.gpu.GetActorRenderData().gpuPrototype && !asset.model->GetRigidVertexCount()) {
                    BackendTestAccess::ValidateUploadedPalette(backend,*actor.gpu.GetSkinningPalette());++paletteChecks;
                }
                Finish(backend,renderer);
            }
            actor.cpu.Clear();actor.gpu.Clear();other.cpu.Clear();other.gpu.Clear();
            Released(renderer,example.label);lifecycleCount+=2;
        }
    }
}

static void InvalidInputs(Asset& asset,DiligentD3D11Backend& backend,DiligentActorRenderer& renderer)
{
    Frame(backend,renderer);Pair actor;actor.Set(asset);actor.Deform(World(),ActorPart::Body,ActorCategory::Mob);
    const auto original=asset.model->GetSkinningData();const auto palette=actor.gpu.GetSkinningPalette();
    auto reject=[&](const SkinningModelData& data,const std::vector<std::shared_ptr<const BoneRemap>>& remaps,const BonePalette& pose,const char* label) {
        StaticObjectGeometryPtr handle;
        Check(!renderer.PreparePrototype(handle,data,remaps,pose) && !handle,"Invalid input never publishes an uninitialized GPU handle");
        std::cout<<"REJECT "<<label<<'\n';
    };
    auto remaps=actor.gpu.GetSkinningRemaps();
    auto bad=std::make_shared<BoneRemap>();bad->destination=palette->skeleton;bad->meshToSkeleton=remaps[0]->meshToSkeleton;
    bad->meshToSkeleton.assign(bad->meshToSkeleton.size(),uint16_t(palette->matrices.size()));
    auto badRemaps=remaps;badRemaps[0]=bad;reject(*original,badRemaps,*palette,"remap-oob");
    reject(*original,{},*palette,"missing-remap");
    BonePalette invalid;invalid.ready=true;invalid.skeleton=palette->skeleton;invalid.matrices=palette->matrices;
    invalid.matrices[0][0]=std::numeric_limits<float>::quiet_NaN();reject(*original,remaps,invalid,"nan-palette");
    auto huge=std::make_shared<SkeletonLayout>();huge->names.resize(164);invalid.skeleton=huge;invalid.matrices.assign(164,{});
    reject(*original,remaps,invalid,"palette-164-oob");
    for(unsigned fault=0;fault<4;++fault) {
        auto data=*original;
        auto mesh=std::make_shared<StaticSkinnedMeshData>();const auto& src=*data.meshes[0];
        mesh->vertices=src.vertices;mesh->indices=src.indices;mesh->meshBoneCount=src.meshBoneCount;mesh->deformVertexOffset=src.deformVertexOffset;
        mesh->meshToSourceSkeleton=src.meshToSourceSkeleton;mesh->groups=src.groups;
        if(fault==0) { mesh->vertices[0].weights[0]=255;mesh->vertices[0].indices[0]=uint8_t(mesh->meshBoneCount); }
        if(fault==1) std::fill(std::begin(mesh->vertices[0].weights),std::end(mesh->vertices[0].weights),0);
        if(fault==2) { std::fill(std::begin(mesh->vertices[0].weights),std::end(mesh->vertices[0].weights),0);mesh->vertices[0].weights[0]=254; }
        data.meshes[0]=fault==3?nullptr:mesh;
        reject(data,remaps,*palette,(std::string("mesh-fault-")+std::to_string(fault)).c_str());
    }
    actor.cpu.Clear();actor.gpu.Clear();Finish(backend,renderer);Released(renderer,"invalid-data");
}

int main(int argc,char** argv)
{
    HWND window=nullptr;
    try {
        Check(argc==4,"B5 asset root, evidence directory and parity/stress mode required");
        const std::string root=argv[1],mode=argv[3];Check(mode=="parity" || mode=="stress","Known B5 test mode");
        evidence=argv[2];std::filesystem::create_directories(evidence);
        pixelLog.open(evidence/"pixels.csv");Check(bool(pixelLog),"Pixel evidence log");
        pixelLog<<"case,meanAbsoluteChannelDifference,maxChannelDifference,changedChannels,channelsAbove2,totalChannels\n";
        Check(startupSkinningMode==PrototypeSkinningMode::GPU,"B6 production default; explicit CPU reference retained");
        CPackManager packs;CResourceManager resources;Diligent::GetEngineFactoryD3D11()->SetMessageCallback(Message);
        window=CreateWindowW(L"STATIC",L"B5-X isolated parity",WS_OVERLAPPEDWINDOW,0,0,512,512,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        DiligentD3D11Backend backend;Check(window && backend.Initialize({window,512,512}),"B5 backend");
        startupSkinningMode=PrototypeSkinningMode::GPUPrototype;
        {
            DiligentActorRenderer renderer(backend);Check(renderer.Initialize(),"Unchanged actor pipelines");actorRenderer=&renderer;actorWorldFrame=true;
            const uint8_t checker[]={220,180,100,255,90,170,230,32,110,210,110,128,240,220,160,255};
            TerrainTextureData data{2,2,TerrainTextureFormat::RGBA8,{{checker,sizeof(checker),8}}};auto texture=renderer.UploadTexture(data);Check(bool(texture),"Parity material fixture");
            if(mode=="parity") { Players(root,backend,renderer,texture);ActorChurn(root,backend,renderer,texture); }
            else {
                for(unsigned cycle=0;cycle<3;++cycle) { LinkedLods(root,backend,renderer,texture);Released(renderer,"hair-lod-cycle"); }
                Asset mob(root+"/Monster/ymir work/monster/wolf/wolf.gr2");
                for(unsigned cycle=0;cycle<5;++cycle) { Crowd(mob,backend,renderer,texture);Fallback(mob,backend,renderer); }
                InvalidInputs(mob,backend,renderer);
            }
            texture.reset();Released(renderer,"shutdown");Check(!renderer.LiveTextureCount(),"All actor textures released");
            actorRenderer=nullptr;actorWorldFrame=false;actorDeformTargets={};
        }
        resources.Destroy();BackendTestAccess::Validate(backend);backend.Shutdown();DestroyWindow(window);window=nullptr;
        Check(!warnings && !liveSkinMeshes && !liveBoneRemaps && !liveBonePalettes && !skinSidecarFailures,"Source owners and validation errors zero");
        std::cout<<"PASS B5-X mode="<<mode<<" cases="<<cases<<" vertices="<<samples<<" maxPosition="<<maxPosition<<" maxNormal="<<maxNormal
            <<" maxNativePoseDelta="<<maxNativePoseDelta<<" paletteReadbacks="<<paletteChecks<<" lifecycles="<<lifecycleCount
            <<" pixelMean="<<(pixelChannels?double(pixelAbsolute)/pixelChannels:0)<<" pixelMax="<<pixelMaximum<<" pixelChanged="<<pixelChanges
            <<" errors=0 resources=0\n";return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL B5-X "<<error.what()<<'\n';if(window) DestroyWindow(window);return 1;}
}
