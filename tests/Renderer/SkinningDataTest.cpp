#include "EterGrnLib/SkinningDataAdapter.h"
#include "AssetRuntime/Granny/Native.h"
#include "EterGrnLib/Deform.h"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <limits>

using namespace Renderer;
static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
struct Asset
{
    granny_file* file{};
    granny_model* model{};
    std::shared_ptr<const SkinningModelData> data;
    explicit Asset(const std::string& path,int modelIndex=0)
    {
        file=GrannyReadEntireFile(path.c_str());
        Check(file!=nullptr,"Real GR2 asset missing/unreadable (not skipped)");
        auto* info=GrannyGetFileInfo(file);
        Check(info && info->ModelCount>modelIndex,"GR2 model missing");
        model=info->Models[modelIndex]; data=SkinningDataAdapter::Extract(*model);
    }
    ~Asset() { data.reset(); if(file) GrannyFreeFile(file); }
    Asset(const Asset&)=delete;
};
struct Pose
{
    granny_model_instance* instance;
    granny_local_pose* local;
    granny_world_pose* world;
    granny_file* animation{};
    granny_control* control{};
    explicit Pose(granny_model* model,const std::string& clip):
        instance(GrannyInstantiateModel(model)),local(GrannyNewLocalPose(model->Skeleton->BoneCount)),
        world(GrannyNewWorldPose(model->Skeleton->BoneCount))
    {
        Check(instance && local && world,"Granny pose allocation");
        if(!clip.empty()) {
            animation=GrannyReadEntireFile(clip.c_str());
            Check(animation!=nullptr,"Animation asset missing (not skipped)");
            auto* info=GrannyGetFileInfo(animation); Check(info && info->AnimationCount,"Animation missing");
            control=GrannyPlayControlledAnimation(0,info->Animations[0],instance); Check(control!=nullptr,"Animation control");
        }
    }
    ~Pose()
    {
        if(control) GrannyFreeControl(control);
        GrannyFreeModelInstance(instance); GrannyFreeLocalPose(local); GrannyFreeWorldPose(world);
        if(animation) GrannyFreeFile(animation);
    }
};

static size_t comparedVertices{}, poseCases{}, originalVertices{}, floatRoundoff{};
static size_t maxBones{}, maxMeshBones{}, maxInfluences{};
static float maxPositionError{}, maxNormalError{}, maxSdkPositionError{}, maxSdkNormalError{};
static std::array<size_t,5> influences{};
static bool Close(float a,float b,bool normal)
{
    const float tolerance=normal?1e-5f:1e-4f+2e-6f*std::max(1.f,std::abs(b));
    return std::isfinite(a) && std::isfinite(b) && std::abs(a-b)<=tolerance;
}
static granny_pnt332_vertex Reference(const SkinningVertex& v,const BoneRemap& remap,const BonePalette& palette)
{
    granny_pnt332_vertex out{};
    for(int k=0;k<4;++k) if(v.weights[k]) {
        const auto& matrix=palette.matrices.at(remap.meshToSkeleton.at(v.indices[k]));
        const float weight=v.weights[k]*(1.f/255.f);
        for(int c=0;c<3;++c) {
            out.Position[c]+=weight*(v.position[0]*matrix[c]+v.position[1]*matrix[4+c]+v.position[2]*matrix[8+c]+matrix[12+c]);
            out.Normal[c]+=weight*(v.normal[0]*matrix[c]+v.normal[1]*matrix[4+c]+v.normal[2]*matrix[8+c]);
        }
    }
    std::memcpy(out.UV,v.uv,sizeof(v.uv)); return out;
}

static void CheckOriginal(const Asset& asset,const std::string& label)
{
    size_t verts=0, meshBones=0, maxInf=0, nonIdentity=0, rigid=0, empty=0;
    for(size_t m=0;m<asset.data->meshes.size();++m) {
        auto status=asset.data->status[m];
        if(status==SkinDataStatus::Rigid) { ++rigid; continue; }
        if(status==SkinDataStatus::Empty) { ++empty; continue; }
        if(status!=SkinDataStatus::Ready) std::cerr<<label<<" mesh="<<m<<" status="<<SkinDataStatusName(status)<<'\n';
        Check(status==SkinDataStatus::Ready,"Real mesh extraction status");
        const auto& data=*asset.data->meshes[m];
        auto* mesh=asset.model->MeshBindings[m].Mesh;
        Check(data.meshIndex==m && data.vertices.size()==GrannyGetMeshVertexCount(mesh),"Mesh slot/count");
        Check(data.deformVertexOffset==verts,"Original deform offset");
        Check(!std::memcmp(data.vertices.data(),GrannyGetMeshVertices(mesh),data.vertices.size()*sizeof(SkinningVertex)),"Original position/normal/UV/indices/weights byte parity");
        std::vector<uint16_t> indices(GrannyGetMeshIndexCount(mesh)); GrannyCopyMeshIndices(mesh,2,indices.data());
        Check(data.indices==indices,"Original topology parity");
        auto* groups=GrannyGetMeshTriangleGroups(mesh);
        Check(data.groups.size()==GrannyGetMeshTriangleGroupCount(mesh),"Original material group count");
        for(size_t g=0;g<data.groups.size();++g) Check(data.groups[g].materialIndex==groups[g].MaterialIndex &&
            data.groups[g].firstIndex==groups[g].TriFirst*3 && data.groups[g].indexCount==groups[g].TriCount*3,"Material range parity");
        auto* binding=GrannyNewMeshBinding(mesh,asset.model->Skeleton,asset.model->Skeleton);
        auto remap=SkinningDataAdapter::ExtractRemap(data,binding,asset.data->skeleton,status);
        Check(remap && status==SkinDataStatus::Ready,"Self binding extraction");
        const auto* native=GrannyGetMeshBindingToBoneIndices(binding);
        for(size_t b=0;b<remap->meshToSkeleton.size();++b) {
            Check(remap->meshToSkeleton[b]==native[b],"Exact native remap");
            Check(data.meshToSourceSkeleton[b]==native[b],"Exact original source skeleton mapping");
            nonIdentity+=native[b]!=b;
        }
        auto shared=SkinningDataAdapter::ExtractRemap(data,binding,asset.data->skeleton,status);
        Check(shared==remap,"Remap shared for identical mesh/binding/destination");
        GrannyFreeMeshBinding(binding);
        verts+=data.vertices.size(); meshBones=std::max(meshBones,size_t(data.meshBoneCount));
        maxInf=std::max(maxInf,size_t(data.diagnostics.maxInfluences));
        Check(data.diagnostics.exactWeightSums==data.vertices.size(),"Exact byte sum 255");
        floatRoundoff+=data.diagnostics.floatWeightRoundoff;
        for(size_t i=0;i<5;++i) influences[i]+=data.diagnostics.influences[i];
    }
    originalVertices+=verts;
    maxBones=std::max(maxBones,asset.data->skeleton->names.size()); maxMeshBones=std::max(maxMeshBones,meshBones);
    maxInfluences=std::max(maxInfluences,maxInf);
    std::cout<<"ASSET "<<label<<" skeleton="<<asset.data->skeleton->names.size()<<" meshBones="<<meshBones
        <<" palette="<<(verts?asset.data->skeleton->names.size():0)<<" maxInfluences="<<maxInf<<" remappedEntries="<<nonIdentity
        <<" vertices="<<verts<<" rigid="<<rigid<<" empty="<<empty<<'\n';
}

static void CheckPose(const Asset& source,const Asset& destination,const std::string& clip)
{
    Pose pose(destination.model,clip);
    std::vector<granny_mesh_binding*> bindings(source.data->meshes.size());
    std::vector<std::shared_ptr<const BoneRemap>> remaps(bindings.size());
    for(size_t m=0;m<bindings.size();++m) if(source.data->meshes[m]) {
        bindings[m]=GrannyNewMeshBinding(source.model->MeshBindings[m].Mesh,source.model->Skeleton,destination.model->Skeleton);
        SkinDataStatus status;
        remaps[m]=SkinningDataAdapter::ExtractRemap(*source.data->meshes[m],bindings[m],destination.data->skeleton,status);
        Check(remaps[m] && status==SkinDataStatus::Ready,"Destination binding extraction");
    }
    SkinningMatrix parent{};
    parent[0]=1.3f;parent[5]=.7f;parent[10]=1.1f;parent[15]=1;parent[12]=7;parent[13]=-3;parent[14]=12;
    BonePalette palette;
    for(int mode=0;mode<2;++mode) for(float time:{0.f,.37f,.83f}) {
        GrannySetModelClock(pose.instance,time);
        GrannySampleModelAnimationsAccelerated(pose.instance,destination.model->Skeleton->BoneCount,mode?parent.data():nullptr,pose.local,pose.world);
        Check(SkinningDataAdapter::CapturePose(palette,destination.data->skeleton,pose.world)==SkinDataStatus::Ready,"Current composite palette");
        const auto* matrices=GrannyGetWorldPoseComposite4x4Array(pose.world);
        Check(!std::memcmp(palette.matrices.data(),matrices,palette.matrices.size()*64),"Palette byte/matrix layout parity");
        for(size_t m=0;m<bindings.size();++m) if(remaps[m]) {
            const auto& data=*source.data->meshes[m];
            const auto* mesh=source.model->MeshBindings[m].Mesh;
            const auto* native=GrannyGetMeshBindingToBoneIndices(bindings[m]);
            const int count=static_cast<int>(data.vertices.size());
            std::vector<granny_pnt332_vertex> sse(count), sdk(count);
            DeformPWNT3432toGrannyPNGBT33332(count,GrannyGetMeshVertices(mesh),sse.data(),native,matrices,40,32);
            auto* deformer=GrannyNewMeshDeformer(GrannyGetMeshVertexType(mesh),GrannyPNT332VertexType,GrannyDeformPositionNormal,GrannyAllowUncopiedTail);
            Check(deformer!=nullptr,"SDK deformer");
            GrannyDeformVertices(deformer,native,(float*)matrices,count,GrannyGetMeshVertices(mesh),sdk.data());
            GrannyFreeMeshDeformer(deformer);
            // All vertices includes first/middle/last and every available 1/2/3/4-influence vertex.
            for(int v=0;v<count;++v) {
                auto reference=Reference(data.vertices[v],*remaps[m],palette);
                for(int c=0;c<3;++c) {
                    maxPositionError=std::max(maxPositionError,std::abs(reference.Position[c]-sse[v].Position[c]));
                    maxNormalError=std::max(maxNormalError,std::abs(reference.Normal[c]-sse[v].Normal[c]));
                    maxSdkPositionError=std::max(maxSdkPositionError,std::abs(reference.Position[c]-sdk[v].Position[c]));
                    maxSdkNormalError=std::max(maxSdkNormalError,std::abs(reference.Normal[c]-sdk[v].Normal[c]));
                    Check(Close(reference.Position[c],sse[v].Position[c],false),"Scalar vs existing SSE position");
                    Check(Close(reference.Normal[c],sse[v].Normal[c],true),"Scalar vs existing SSE normal");
                    Check(Close(reference.Position[c],sdk[v].Position[c],false),"Scalar vs SDK position");
                    Check(Close(reference.Normal[c],sdk[v].Normal[c],true),"Scalar vs SDK normal");
                }
                Check(!std::memcmp(reference.UV,sse[v].UV,8),"UV unchanged");
            }
            comparedVertices+=count;
        }
        ++poseCases;
    }
    for(auto* binding:bindings) if(binding) GrannyFreeMeshBinding(binding);
}

static void InvalidDataTests()
{
    SkinningVertex vertex{}; vertex.weights[0]=255; SkinDataDiagnostics diagnostics;
    auto validate=[&]{ return ValidateSkinVertices({&vertex,1},1,diagnostics); };
    Check(validate()==SkinDataStatus::Ready,"Valid byte input");
    vertex.weights[0]=0; Check(validate()==SkinDataStatus::ZeroWeights,"Zero weights diagnosed");
    vertex.weights[0]=254; Check(validate()==SkinDataStatus::WeightSum && vertex.weights[0]==254,"Bad sum rejected without correction");
    vertex.weights[0]=255;vertex.indices[0]=1; Check(validate()==SkinDataStatus::InvalidBoneIndex,"Active index out of bounds");
    vertex.indices[0]=0;vertex.indices[1]=255;
    Check(validate()==SkinDataStatus::Ready && diagnostics.ignoredZeroWeightIndices==1,"Unused lane never dereferenced, recorded");
    vertex.normal[0]=std::numeric_limits<float>::quiet_NaN(); Check(validate()==SkinDataStatus::NonFiniteAttribute,"NaN attribute diagnosed");
    vertex.normal[0]=0;
    Check(ValidateSkinVertices({&vertex,1},257,diagnostics)==SkinDataStatus::PaletteTooLarge,"Prepared capacity overflow");
    Check(ValidateSkinVertices({},1,diagnostics)==SkinDataStatus::Empty,"Empty input");
    auto skeleton=std::make_shared<SkeletonLayout>(); skeleton->names={"root"};skeleton->parents={-1};
    StaticSkinnedMeshData mesh;mesh.meshBoneCount=1; SkinDataStatus status;
    int negative=-1,tooLarge=1,valid=0;
    Check(!AcquireBoneRemap(mesh,skeleton,{&negative,1},status) && status==SkinDataStatus::InvalidRemap,"Negative native remap");
    Check(!AcquireBoneRemap(mesh,skeleton,{&tooLarge,1},status),"Remap out of palette");
    Check(!AcquireBoneRemap(mesh,skeleton,{},status),"Incomplete remap");
    auto first=AcquireBoneRemap(mesh,skeleton,{&valid,1},status);std::weak_ptr<const BoneRemap> lifetime=first;
    first.reset();Check(lifetime.expired(),"Weak cache does not retain remap resources");
    auto second=AcquireBoneRemap(mesh,skeleton,{&valid,1},status);Check(mesh.remapCache.size()==1,"Expired cache entries pruned");
    BonePalette palette;SkinningMatrix matrix{}; matrix[0]=matrix[5]=matrix[10]=matrix[15]=1;
    Check(CaptureBonePalette(palette,skeleton,{&matrix,1})==SkinDataStatus::Ready,"Finite palette");
    matrix[0]=std::numeric_limits<float>::infinity();
    Check(CaptureBonePalette(palette,skeleton,{&matrix,1})==SkinDataStatus::InvalidMatrix && !palette.ready,"Invalid matrix invalidates old ready palette");
    Check(SkinningDataAdapter::CapturePose(palette,skeleton,nullptr)==SkinDataStatus::MissingData,"Missing pose");
    Check(productionSkinningMode==SkinningMode::GPU,"B6 GPU production policy; direct native CPU numeric reference remains available");
}

static void AdapterDiagnostics(const Asset& original)
{
    // Corrupt only local copies in the test, never the GR2 file or the original loaded mesh.
    auto model=*original.model;
    auto mesh=*model.MeshBindings[0].Mesh;
    auto binding=model.MeshBindings[0];binding.Mesh=&mesh;
    model.MeshBindingCount=1;model.MeshBindings=&binding;
    auto vertexData=*mesh.PrimaryVertexData;mesh.PrimaryVertexData=&vertexData;
    auto result=[&]{ auto data=SkinningDataAdapter::Extract(model);Check(!data->meshes[0],"Invalid adapter data not exposed as ready");return data->status[0]; };
    vertexData.VertexType=nullptr;
    Check(result()==SkinDataStatus::UnsupportedLayout,"Missing real vertex layout diagnosed");
    vertexData=*original.model->MeshBindings[0].Mesh->PrimaryVertexData;
    vertexData.Vertices=nullptr;
    Check(result()==SkinDataStatus::MissingData,"Missing original vertex storage diagnosed");
    vertexData=*original.model->MeshBindings[0].Mesh->PrimaryVertexData;
    mesh.PrimaryTopology=nullptr;
    Check(result()==SkinDataStatus::InvalidTopology,"Missing real topology diagnosed");
    mesh.PrimaryTopology=original.model->MeshBindings[0].Mesh->PrimaryTopology;
    auto topology=*mesh.PrimaryTopology;mesh.PrimaryTopology=&topology;
    auto group=topology.Groups[0]; group.TriFirst=-1;topology.GroupCount=1;topology.Groups=&group;
    Check(result()==SkinDataStatus::InvalidTopology,"Invalid real material range diagnosed");
    Check(!std::memcmp(original.data->meshes[0]->vertices.data(),GrannyGetMeshVertices(original.model->MeshBindings[0].Mesh),
        original.data->meshes[0]->vertices.size()*sizeof(SkinningVertex)),"Diagnostic corruption never mutates original data");
}

int main(int argc,char** argv)
{
    try {
        Check(argc==2,"Explicit real asset root required");const std::string root=argv[1];
        InvalidDataTests();
        for(const auto& folder:{"PC/ymir work/pc/","pc2/ymir work/pc2/"})
            for(const auto& race:{"warrior","assassin","sura","shaman"}) {
                const std::string base=std::string(folder)+race+"/";
                Asset body(root+"/"+base+race+"_novice.gr2");
                CheckOriginal(body,base+race+"_novice");
                CheckPose(body,body,root+"/"+base+"general/run.gr2");
                Asset hairA(root+"/"+base+"hair/hair_1_1.gr2"),hairB(root+"/"+base+"hair/hair_2_1.gr2");
                CheckOriginal(hairA,base+"hairA");CheckOriginal(hairB,base+"hairB");
                for(const Asset* hair:{&hairA,&hairB,&hairA}) CheckPose(*hair,body,root+"/"+base+"general/wait.gr2");
            }
        {
            const std::string base=root+"/PC/ymir work/pc/warrior/";
            Asset body(base+"warrior_novice.gr2"),armorA(base+"warrior_4-1.gr2"),armorB(base+"warrior_nahan.gr2");
            Asset hairA(base+"hair/hair_1_1.gr2"),hairB(base+"hair/hair_2_1.gr2");
            AdapterDiagnostics(body);
            CheckOriginal(armorA,"warrior armor A");CheckOriginal(armorB,"warrior armor B");
            const int expectedHeads[]={67,66,68,67};int phase=0;
            const size_t expectedChanges[]={72,40,72,72};
            for(const Asset* destination:{&body,&armorA,&armorB,&body}) {
                CheckPose(*destination,*destination,base+"general/attack.gr2");
                for(const Asset* hair:{&hairA,&hairB,&hairA}) CheckPose(*hair,*destination,base+"general/run.gr2");
                const auto& mesh=*hairA.data->meshes[0];
                auto* binding=GrannyNewMeshBinding(hairA.model->MeshBindings[0].Mesh,hairA.model->Skeleton,destination->model->Skeleton);
                SkinDataStatus status; auto remap=SkinningDataAdapter::ExtractRemap(mesh,binding,destination->data->skeleton,status);
                Check(remap && mesh.vertices[0].indices[0]==5 && remap->meshToSkeleton[5]==expectedHeads[phase],"B1 proven hair local 5 -> head 67/66/68/67");
                Check(mesh.meshToSourceSkeleton[5]==66,"B1 hair original source skeleton index 66 retained");
                size_t changes=0;
                for(size_t b=0;b<mesh.meshBoneCount;++b) changes+=mesh.meshToSourceSkeleton[b]!=remap->meshToSkeleton[b];
                Check(changes==expectedChanges[phase],"B1 proven complete hair remap change counts 72/40/72/72");
                Check(destination->data->skeleton->names[remap->meshToSkeleton[5]]=="Bip01 Head","B1 exact destination bone name");
                std::cout<<"B1_HAIR phase="<<phase<<" local=5 target="<<remap->meshToSkeleton[5]<<" changed="<<changes<<'\n';
                GrannyFreeMeshBinding(binding);++phase;
            }
        }
        for(const char* path:{
            "NPC/ymir work/npc/doctor/doctor.gr2","NPC/ymir work/npc/sinseon/sinseon.gr2","NPC/ymir work/npc/blacksmith/blacksmith.gr2",
            "Monster/ymir work/monster/wolf/wolf.gr2","Monster/ymir work/monster/orc_soldier/orc_soldier.gr2",
            "monster2/ymir work/monster2/fire_dragon/fire_dragon.gr2",
            "Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2",
            "metin2_patch_dragon_rock_mobs/ymir work/monster2/ent_boss2/ent_boss2.gr2",
            "NPC/ymir work/npc/horse/horse_normal.gr2","patch2/ymir work/npc/boar/boar.gr2",
            "patch2/ymir work/npc/lion_white/lion_white.gr2",
            "metin2_patch_halloween/ymir work/npc/horse_halloween1/horse_halloween1.gr2",
            "metin2_patch_pet1/ymir work/npc/dinosaur/dinosaur_3.gr2",
            "item/ymir work/item/weapon/00010.gr2"}) {
            Asset asset(root+"/"+path);CheckOriginal(asset,path);CheckPose(asset,asset,"");
        }
        for(int model=0;model<3;++model) {
            Asset asset(root+"/metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief2_soldier2/redthief2_soldier2_lod_01.gr2",model);
            CheckOriginal(asset,"empty-multimodel-"+std::to_string(model));CheckPose(asset,asset,"");
        }
        Check(influences[1] && influences[2] && influences[3] && influences[4],"All four influence categories tested");
        Check(maxBones==163 && maxInfluences==4,"B1 observed maximum represented");
        Check(liveSkinMeshes==0 && liveBoneRemaps==0 && liveBonePalettes==0,"All CPU skin data owners released");
        std::cout<<"PASS originalVertices="<<originalVertices<<" weightFloatRoundoff="<<floatRoundoff<<" poses="<<poseCases
            <<" comparedVertices="<<comparedVertices<<" maxBones="<<maxBones<<" maxMeshBones="<<maxMeshBones<<" maxInfluences="<<maxInfluences
            <<" positionError="<<maxPositionError<<" normalError="<<maxNormalError<<" sdkPositionError="<<maxSdkPositionError
            <<" sdkNormalError="<<maxSdkNormalError<<" SkinMeshes="<<liveSkinMeshes<<" BoneRemaps="<<liveBoneRemaps<<" BonePalettes="<<liveBonePalettes<<'\n';
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n';return 1; }
}
