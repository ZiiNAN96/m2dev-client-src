#include "EterGrnLib/StdAfx.h"
#include "AssetRuntime/GR2/GR2Reader.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include "AssetRuntime/Granny/GrannyAssetProvider.h"
#include "AssetRuntime/Granny/Native.h"
#include "AssetRuntime/Granny/GrannyInterop.h"
#include "AssetRuntime/Granny/GrannyAnimationAdapter.h"
#include "Renderer/DiligentD3D11BackendInternal.h"
#include "Renderer/GpuSkinningPrototype.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

using namespace AssetRuntime;
namespace AR=AnimationRuntime;
static void Check(bool value,const std::string& text) { if(!value) throw std::runtime_error(text); }
#include "../Renderer/SkinningGpuReadback.h"
static std::vector<std::byte> Bytes(const std::filesystem::path& path)
{
    std::ifstream input(path,std::ios::binary|std::ios::ate); Check(bool(input),"missing original fixture "+path.string());
    const auto size=input.tellg(); Check(size>0 && size<=std::streamoff(GR2::MaximumFileBytes),"fixture size");
    std::vector<std::byte> bytes(static_cast<std::size_t>(size)); input.seekg(0); input.read(reinterpret_cast<char*>(bytes.data()),size);
    Check(bool(input),"fixture read"); return bytes;
}
static void StaticParity(const std::filesystem::path& path)
{
    auto bytes=Bytes(path); auto own=GetGR2AssetProvider().Load(path.string(),bytes),ref=GetGrannyAssetProvider().Load(path.string(),bytes);
    Check(bool(own),own.diagnostic); Check(bool(ref),"reference provider");
    Check(own.asset.ModelCount()==ref.asset.ModelCount(),"model count");
    Check(own.asset.AnimationCount()==ref.asset.AnimationCount(),"animation count");
    GR2::File raw(bytes); const auto source=GR2::Read(raw);
    for(std::size_t i=0;i<own.asset.ModelCount();++i) {
        const auto& x=source.modelData[i].initialPlacement; const auto& y=GrannyInterop::GetModel(ref.asset.Model(i))->InitialPlacement;
        Check(x.flags==y.Flags && std::equal(x.position.begin(),x.position.end(),y.Position) && std::equal(x.orientation.begin(),x.orientation.end(),y.Orientation) && std::equal(x.scaleShear.begin(),x.scaleShear.end(),&y.ScaleShear[0][0]),"initial model placement");
    }
    for(std::size_t i=0;i<own.asset.ModelCount();++i) {
        const auto& a=*own.asset.Model(i).Get(); const auto& b=*ref.asset.Model(i).Get();
        Check(a.name==b.name && a.meshes.size()==b.meshes.size() && a.materials.size()==b.materials.size(),"model names/counts");
        Check(a.deformation==b.deformation,"model deformation");
        Check(bool(a.skeleton)==bool(b.skeleton),"skeleton presence");
        if(a.skeleton) {
            Check(a.skeleton->name==b.skeleton->name && a.skeleton->bones.size()==b.skeleton->bones.size(),"skeleton name/count");
            for(std::size_t j=0;j<a.skeleton->bones.size();++j) {
                const auto& x=a.skeleton->bones[j]; const auto& y=b.skeleton->bones[j];
                Check(x.name==y.name && x.id==y.id && x.parentIndex==y.parentIndex,"bone identity/order");
                Check(x.localBind.flags==y.localBind.flags && x.localBind.position==y.localBind.position && x.localBind.orientation==y.localBind.orientation &&
                    x.localBind.scaleShear==y.localBind.scaleShear && x.inverseBind==y.inverseBind,"bone bind transform");
            }
        }
        for(std::size_t j=0;j<a.materials.size();++j) {
            const auto& x=a.materials[j]; const auto& y=b.materials[j];
            Check(x.name==y.name,"material name");
            Check(x.textures==y.textures,"material textures: "+x.name+" native="+x.textures[1]+" ref="+y.textures[1]);
            Check(x.matchingTextures==y.matchingTextures && x.stage==y.stage && x.culling==y.culling,"material semantics");
        }
        for(std::size_t j=0;j<a.meshes.size();++j) {
            const auto& x=a.meshes[j]; const auto& y=b.meshes[j];
            Check(x.name==y.name && x.vertexCount==y.vertexCount && x.indexCount==y.indexCount && x.sourceVertexStride==y.sourceVertexStride,"mesh counts/stride "+x.name+" native="+std::to_string(x.vertexCount)+","+std::to_string(x.indexCount)+","+std::to_string(x.sourceVertexStride)+" ref="+std::to_string(y.vertexCount)+","+std::to_string(y.indexCount)+","+std::to_string(y.sourceVertexStride));
            Check(x.vertexLayout==y.vertexLayout && x.deformation==y.deformation && x.indexWidth==y.indexWidth,"mesh layout/classification "+x.name+" native="+std::to_string(int(x.vertexLayout))+","+std::to_string(int(x.deformation))+","+std::to_string(int(x.indexWidth))+" ref="+std::to_string(int(y.vertexLayout))+","+std::to_string(int(y.deformation))+","+std::to_string(int(y.indexWidth)));
            Check(x.materialBindings==y.materialBindings && x.materialGroups.size()==y.materialGroups.size(),"material bindings");
            Check(x.skin.boneNames==y.skin.boneNames && x.skin.meshToSkeleton==y.skin.meshToSkeleton,"mesh bone mapping");
            for(std::size_t k=0;k<x.materialGroups.size();++k) Check(x.materialGroups[k].materialIndex==y.materialGroups[k].materialIndex &&
                x.materialGroups[k].firstIndex==y.materialGroups[k].firstIndex && x.materialGroups[k].indexCount==y.materialGroups[k].indexCount,"triangle groups");
            for(auto layout:{VertexLayout::PositionNormalUV,VertexLayout::WeightedPositionNormalUV,VertexLayout::PositionNormalUV2}) {
                if(layout==VertexLayout::WeightedPositionNormalUV && x.vertexLayout!=layout) continue;
                if(layout==VertexLayout::PositionNormalUV2 && x.vertexLayout!=layout) continue;
                std::vector<std::byte> vx(x.vertexCount*VertexStride(layout)),vy(vx.size());
                Check(own.asset.Get()->CopyVertices(i,j,layout,vx)==AssetError::None && ref.asset.Get()->CopyVertices(i,j,layout,vy)==AssetError::None,"vertex copy");
                Check(vx==vy,"exact positions/normals/UV/weights/indices");
            }
            for(auto width:{IndexWidth::UInt16,IndexWidth::UInt32}) {
                std::vector<std::byte> ix(x.indexCount*std::size_t(width)),iy(ix.size());
                Check(own.asset.Get()->CopyIndices(i,j,width,ix)==AssetError::None && ref.asset.Get()->CopyIndices(i,j,width,iy)==AssetError::None && ix==iy,"index stream");
            }
        }
    }
    // Release the original input and upload buffers before checking lifetime.
    bytes.clear(); own.asset.ReleaseUploadData(); own.asset.Reset(); ref.asset.Reset();
    Check(GR2::liveReaderDocuments==0,"reader ownership leak");
    std::cout<<"STATIC PASS "<<path.filename().string()<<'\n';
}
struct Reference
{
    granny_file* modelFile{},*animationFile{}; granny_model_instance* instance{};
    granny_local_pose* pose{}; granny_world_pose* world{}; granny_control* control{};
    Reference(const std::filesystem::path& model,const std::filesystem::path& animation)
    {
        modelFile=GrannyReadEntireFile(model.string().c_str()); animationFile=GrannyReadEntireFile(animation.string().c_str());
        Check(modelFile&&animationFile,"reference read");
        auto* m=GrannyGetFileInfo(modelFile)->Models[0]; auto* a=GrannyGetFileInfo(animationFile)->Animations[0];
        instance=GrannyInstantiateModel(m); pose=GrannyNewLocalPose(m->Skeleton->BoneCount); world=GrannyNewWorldPose(m->Skeleton->BoneCount);
        control=GrannyPlayControlledAnimation(0,a,instance); Check(instance&&pose&&world&&control,"reference animation");
        GrannySetControlLoopCount(control,0); GrannySetControlEaseIn(control,false); GrannySetControlEaseOut(control,false);
    }
    ~Reference() { if(control)GrannyFreeControl(control); if(instance)GrannyFreeModelInstance(instance); if(pose)GrannyFreeLocalPose(pose); if(world)GrannyFreeWorldPose(world); if(modelFile)GrannyFreeFile(modelFile); if(animationFile)GrannyFreeFile(animationFile); }
};
static void AnimationParity(const std::filesystem::path& modelPath,const std::filesystem::path& clipPath,Renderer::DiligentD3D11Backend& backend,bool finite=false)
{
    const auto started=std::chrono::steady_clock::now();
    GR2::File modelFile(Bytes(modelPath)),clipFile(Bytes(clipPath)); auto model=GR2::Read(modelFile),clip=GR2::Read(clipFile);
    Check(model.modelData.size()==1 && clip.animations.size()==1,"animation fixture structure");
    const auto& skeleton=*model.modelData[0].skeleton; std::string error;
    auto runtime=GR2::BindAnimation(clip.animations[0],clip.animationData[0],skeleton,error,finite?0:3,model.models[0].name); Check(bool(runtime),error);
    const double loadMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-started).count();
    Reference reference(modelPath,clipPath);
    const auto* referenceAnimation=GrannyGetFileInfo(reference.animationFile)->Animations[0];
    Check(clip.animations[0].name==referenceAnimation->Name && clip.animations[0].duration==referenceAnimation->Duration && clip.animations[0].trackGroupCount==referenceAnimation->TrackGroupCount,"animation metadata");
    std::size_t event=0;
    if(referenceAnimation->TrackGroupCount) {
        const auto* first=referenceAnimation->TrackGroups[0];
        for(int t=0;t<first->TextTrackCount;++t) for(int e=0;e<first->TextTracks[t].EntryCount;++e) {
            const auto& entry=first->TextTracks[t].Entries[e];const auto& own=clip.animations[0].textEvents.at(event++);
            Check(own.time==entry.TimeStamp && own.text==entry.Text,"text annotation metadata/order");
        }
    }
    Check(event==clip.animations[0].textEvents.size(),"text annotation count");
    for(std::size_t g=0;g<clip.animationData[0].groups.size();++g) {
        const auto& own=clip.animationData[0].groups[g]; const auto* ref=referenceAnimation->TrackGroups[g];
        Check(own.name==ref->Name && own.tracks.size()==ref->TransformTrackCount,"track group metadata");
        for(std::size_t t=0;t<own.tracks.size();++t) Check(own.tracks[t].name==ref->TransformTracks[t].Name,"track target/order");
    }
    if(finite) GrannySetControlLoopCount(reference.control,1);
    AR::AnimationPose pose; pose.Prepare(skeleton.Bones().size()); std::vector<AR::Matrix> matrices(skeleton.Bones().size()),palette(matrices.size());
    double translation=0,rotation=0,scale=0,world=0,composite=0,positionError=0,normalError=0; std::string worst;
    std::vector<Renderer::SkinningVertex> vertices;
    for(std::size_t m=0;m<model.models[0].meshes.size();++m) {
        const auto& mesh=model.models[0].meshes[m]; if(mesh.deformation!=Deformation::Skinned) continue;
        for(const auto& source:model.modelData[0].meshes[m].vertices) {
            Renderer::SkinningVertex vertex;
            std::copy(source.position.begin(),source.position.end(),vertex.position); std::copy(source.normal.begin(),source.normal.end(),vertex.normal);
            std::copy(source.uv.begin(),source.uv.end(),vertex.uv); std::copy(source.weights.begin(),source.weights.end(),vertex.weights);
            for(unsigned k=0;k<4;++k) vertex.indices[k]=source.weights[k]?static_cast<std::uint8_t>(mesh.skin.meshToSkeleton.at(source.joints[k])):0;
            vertices.push_back(vertex);
        }
    }
    auto layout=std::make_shared<Renderer::SkeletonLayout>(); for(const auto& bone:skeleton.Bones()) { layout->names.push_back(bone.name); layout->parents.push_back(bone.parent); }
    Renderer::BonePalette ownPalette,referencePalette; ownPalette.skeleton=referencePalette.skeleton=layout; ownPalette.ready=referencePalette.ready=true;
    referencePalette.matrices.resize(matrices.size());
    const double duration=runtime->Duration();
    for(double time:{0.,.1*duration,.25*duration,.5*duration,.75*duration,.9*duration,duration,duration-.0001,duration+.0001,2*duration+.0001}) {
        const float clock=static_cast<float>(time); GrannySetModelClock(reference.instance,clock);
        GrannySampleModelAnimationsAccelerated(reference.instance,static_cast<int>(matrices.size()),nullptr,reference.pose,reference.world);
        GrannySampleModelAnimations(reference.instance,0,static_cast<int>(matrices.size()),reference.pose);
        Check(AR::Sample(skeleton,*runtime,clock,finite?AR::TimeMode::Clamp:AR::TimeMode::Loop,pose)&&AR::Evaluate(skeleton,pose,matrices)&&AR::BuildPalette(skeleton,matrices,palette),"native runtime sampling");
        for(std::size_t b=0;b<matrices.size();++b) {
            const auto* native=GrannyGetLocalPoseTransform(reference.pose,static_cast<int>(b)); const auto& local=pose.localTransforms[b];
            for(unsigned c=0;c<3;++c) {
                const double delta=std::abs(double(local.translation[c])-native->Position[c]);
                if(delta>translation) { translation=delta; worst=skeleton.Bones()[b].name+" t="+std::to_string(time)+" channel="+std::to_string(c)+" native="+std::to_string(local.translation[c])+" ref="+std::to_string(native->Position[c]); }
            }
            double dot=0,la=0,lb=0; for(unsigned c=0;c<4;++c) { dot+=double(local.rotation[c])*native->Orientation[c]; la+=double(local.rotation[c])*local.rotation[c]; lb+=double(native->Orientation[c])*native->Orientation[c]; }
            rotation=std::max(rotation,2*std::acos(std::clamp(std::abs(dot)/std::sqrt(la*lb),0.,1.)));
            for(unsigned c=0;c<9;++c) scale=std::max(scale,std::abs(double(local.scaleShear[c])-(&native->ScaleShear[0][0])[c]));
            const auto* wm=GrannyGetWorldPose4x4(reference.world,static_cast<int>(b)); const auto* cm=GrannyGetWorldPoseComposite4x4(reference.world,static_cast<int>(b));
            Check(std::all_of(wm,wm+16,[](float x){return std::isfinite(x);})&&std::all_of(cm,cm+16,[](float x){return std::isfinite(x);}),"nonfinite reference pose/palette");
            for(unsigned c=0;c<16;++c) { world=std::max(world,std::abs(double(matrices[b][c])-wm[c])); composite=std::max(composite,std::abs(double(palette[b][c])-cm[c])); }
            std::copy_n(cm,16,referencePalette.matrices[b].begin());
        }
        ownPalette.matrices=palette;
        auto skin=[](const auto& v,const auto& p) {
            std::array<float,6> result{};
            for(unsigned k=0;k<4;++k) if(v.weights[k]) { const auto& m=p.matrices.at(v.indices[k]); const float w=v.weights[k]/255.f;
                for(unsigned c=0;c<3;++c) { result[c]+=w*(v.position[0]*m[c]+v.position[1]*m[4+c]+v.position[2]*m[8+c]+m[12+c]); result[c+3]+=w*(v.normal[0]*m[c]+v.normal[1]*m[4+c]+v.normal[2]*m[8+c]); } }
            return result;
        };
        for(const auto& vertex:vertices) { const auto a=skin(vertex,ownPalette),b=skin(vertex,referencePalette);
            for(unsigned c=0;c<3;++c) { positionError=std::max(positionError,std::abs(double(a[c])-b[c])); normalError=std::max(normalError,std::abs(double(a[c+3])-b[c+3])); } }
        if(time==duration*.25 && !vertices.empty()) {
            const auto a=Renderer::BackendTestAccess::Skin(backend,vertices,ownPalette),b=Renderer::BackendTestAccess::Skin(backend,vertices,referencePalette);
            for(std::size_t v=0;v<vertices.size();++v) for(unsigned c=0;c<3;++c) { positionError=std::max(positionError,std::abs(double(a[v*2][c])-b[v*2][c])); normalError=std::max(normalError,std::abs(double(a[v*2+1][c])-b[v*2+1][c])); }
        }
    }
    auto ownedModel=GetGR2AssetProvider().Load(modelPath.string(),Bytes(modelPath)); auto ownedClip=GetGR2AssetProvider().Load(clipPath.string(),Bytes(clipPath));
    Check(bool(ownedModel)&&bool(ownedClip),"root motion assets");
    auto instance=ownedModel.asset.Get()->CreateAnimationInstance(ownedModel.asset.Model(0));
    Check(instance&&instance->SetMotion(ownedClip.asset.Animation(0),0,0,finite?1:0,1)==AssetError::None,"native motion control");
    double rootMotion=0;
    for(float clock:{0.f,float(duration*.25),float(duration+.0001),float(2*duration+.0001)}) for(float speed:{.5f,1.f,1.7f}) {
        GrannySetControlSpeed(reference.control,speed); GrannySetModelClock(reference.instance,clock);
        Check(instance->SetMotion(ownedClip.asset.Animation(0),0,0,finite?1:0,speed)==AssetError::None,"root motion speed"); instance->SetClock(clock);
        auto delta=AR::IdentityMatrix(); delta[0]=0; delta[1]=1; delta[4]=-1; delta[5]=0; delta[12]=19; delta[13]=-7;
        auto native=delta;
        GrannyUpdateModelMatrix(reference.instance,.1f,delta.data(),delta.data(),false); instance->UpdateTransform(.1f,native);
        for(unsigned c=0;c<16;++c) rootMotion=std::max(rootMotion,std::abs(double(delta[c])-native[c]));
    }
    Check(rootMotion<=2e-4,"native root motion parity");
    std::cout<<"ANIMATION "<<clipPath.string()<<" finite="<<finite<<" native_load_bind_ms="<<loadMs<<" translation="<<translation<<" rotation="<<rotation<<" scale="<<scale<<" world="<<world<<" palette="<<composite<<" root_motion="<<rootMotion<<" worst="<<worst<<'\n';
    std::cout<<"VERTEX cpu_pairs="<<vertices.size()*10<<" gpu_pairs="<<vertices.size()<<" position="<<positionError<<" normal="<<normalError<<'\n';
    Check(translation<=2e-4&&rotation<=2e-5&&scale<=2e-5&&world<=2e-3&&composite<=2e-3&&positionError<=5e-3&&normalError<=5e-5,"F1-X animation/vertex tolerances");
}
static void FirstUse(const std::filesystem::path& modelPath,const std::filesystem::path& clipPath)
{
    GrannyAnimationAdapter::ClearImportCache();
    const auto mb=Bytes(modelPath),cb=Bytes(clipPath);
    const auto begin=std::chrono::steady_clock::now();
    auto model=GetGR2AssetProvider().Load(modelPath.string(),mb),clip=GetGR2AssetProvider().Load(clipPath.string(),cb);
    Check(bool(model)&&bool(clip),"performance native load");
    auto instance=model.asset.Get()->CreateAnimationInstance(model.asset.Model(0));
    Check(instance&&instance->SetMotion(clip.asset.Animation(0),0,0,1,1)==AssetError::None,"performance native bind");
    const auto ownEnd=std::chrono::steady_clock::now();
    auto refModel=GetGrannyAssetProvider().Load(modelPath.string(),mb),refClip=GetGrannyAssetProvider().Load(clipPath.string(),cb);
    Check(bool(refModel)&&bool(refClip),"performance reference load");
    std::string error; auto skeleton=GrannyAnimationAdapter::ImportSkeleton(refModel.asset.Model(0),error); Check(bool(skeleton),error);
    auto imported=GrannyAnimationAdapter::ImportAnimation(refModel.asset.Model(0),refClip.asset.Animation(0),skeleton,error); Check(bool(imported),error);
    const auto end=std::chrono::steady_clock::now();
    std::cout<<"FIRST_USE "<<clipPath.string()<<" native_ms="<<std::chrono::duration<double,std::milli>(ownEnd-begin).count()<<" granny_import_ms="<<std::chrono::duration<double,std::milli>(end-ownEnd).count()<<'\n';
}
static void LookupParity()
{
    for(unsigned left=0;left<256;++left) for(unsigned right=0;right<256;++right) {
        const char a[]{static_cast<char>(left),0},b[]{static_cast<char>(right),0};
        Check(GR2::CompareTrackNames(a,b)==GrannyStringDifference(a,b),"signed encoded-name ordering parity");
        const std::string pa=std::string("prefix")+a,pb=std::string("prefix")+b;
        Check(GR2::CompareTrackNames(pa,pb)==GrannyStringDifference(pa.c_str(),pb.c_str()),"encoded suffix ordering parity");
    }
    for(int flags:{0,2}) for(int count=2;count<=64;++count) for(int duplicate=0;duplicate<count-1;++duplicate) {
        std::vector<granny_transform_track> tracks(count);std::vector<std::string> names(count);GR2::TrackGroup own;own.accumulationFlags=flags;
        for(int i=0;i<count;++i) {
            names[i]=std::string(1,char(' '+(i==duplicate+1?duplicate:i)));tracks[i].Name=names[i].c_str();
            GR2::TransformTrack track;track.name=names[i];own.tracks.push_back(track);
        }
        granny_track_group group{};group.TransformTrackCount=count;group.TransformTracks=tracks.data();group.Flags=flags;
        int found=-1;Check(GrannyFindTrackByName(&group,names[duplicate].c_str(),&found),"SDK synthetic track lookup");
        const auto* selected=GR2::FindTransformTrack(own,names[duplicate]);
        Check(selected && selected-own.tracks.data()==found,"synthetic duplicate lookup parity");
    }
    std::cout<<"PASS 4032 sorted/unsorted duplicate lookups and 131072 encoded-name comparisons\n";
}
static void CompatibilityParity(const std::filesystem::path& root,Renderer::DiligentD3D11Backend& backend)
{
    LookupParity();
    for(const char* model:{"metin2_patch_easter1/ymir work/pc/warrior/hair/hair_11_1.gr2",
        "patch2/ymir work/npc2/historian/historian.gr2","metin2_patch_eu4/ymir work/npc2/halloween1/halloween1.gr2",
        "season3_eu/ymir work/monster2/ch_officer/skipia_officer.gr2","season3_eu/ymir work/monster2/ch_officer/skipia_officer_lod_01.gr2",
        "metin2_patch_dragon_rock_mobs/ymir work/monster2/redthief2_soldier2/redthief2_soldier2_lod_01.gr2"}) StaticParity(root/model);
    AnimationParity(root/"metin2_patch_easter1/ymir work/pc/warrior/hair/hair_11_1.gr2",root/"PC/ymir work/pc/warrior/general/wait.gr2",backend);
    const std::array<std::array<const char*,3>,7> pairs{{
        {"NPC/ymir work/npc/doctor","doctor","die"},
        {"metin2_patch_dragon_rock_mobs/ymir work/monster2/ogre_boss2","ogre_boss2","run"},
        {"metin2_patch_dragon_rock_mobs/ymir work/monster2/crustacean_officer","crustacean_officer","run"},
        {"metin2_patch_xmas/ymir work/npc2/pig_young1","pig_young1","walk"},
        {"metin2_patch_eu4/ymir work/npc2/halloween1","halloween1","walk1"},
        {"patch2/ymir work/npc2/historian","historian","run"},
        {"season3_eu/ymir work/monster2/ch_officer","skipia_officer","37"}}};
    for(const auto& pair:pairs) AnimationParity(root/pair[0]/(std::string(pair[1])+".gr2"),root/pair[0]/(std::string(pair[2])+".gr2"),backend);
    AnimationParity(root/"Monster/ymir work/monster/skeleton_soldier_bow/skeleton_soldier_bow.gr2",root/"Monster/ymir work/monster/skeleton_soldier_bow/00.gr2",backend);
}
int main(int argc,char**argv)
{
    try {
        Check(argc>=2,"asset root required"); std::filesystem::path root=argv[1];
        HWND window=CreateWindowW(L"STATIC",L"F2-X vertex parity",WS_OVERLAPPEDWINDOW,0,0,128,128,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
        Renderer::DiligentD3D11Backend backend; Check(window&&backend.Initialize({window,128,128}),"Diligent parity backend");
        if(argc==3) {
            Check(std::string_view(argv[2])=="compatibility","unknown parity mode");CompatibilityParity(root,backend);
            backend.Shutdown();DestroyWindow(window);
            Check(liveDocuments==0&&GR2::liveReaderDocuments==0&&AR::GetLifetimeCounts().clips==0&&AR::GetLifetimeCounts().skeletons==0,"compatibility resources released");return 0;
        }
        if(argc>=4) {
            if(std::string_view(argv[2])=="static") StaticParity(root/argv[3]);
            else { Check(argc==5,"model and clip required");const std::string_view mode=argv[2];Check(mode=="animation"||mode=="animation-clamp","unknown animation mode");AnimationParity(root/argv[3],root/argv[4],backend,mode=="animation-clamp"); }
            backend.Shutdown();DestroyWindow(window);return 0;
        }
        for(const char* path:{"PC/ymir work/pc/warrior/warrior_novice.gr2","PC/ymir work/pc/warrior/hair/hair_1_1.gr2","item/ymir work/item/weapon/00010.gr2",
            "Monster/ymir work/monster/wolf/wolf.gr2","Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2",
            "NPC/ymir work/npc/horse/horse_normal.gr2","Zone/ymir work/zone/n/obj/snow.m/snow-004-house2.gr2","guild/ymir work/guild/facility/gongjakso/gongjakso.gr2"}) StaticParity(root/path);
        StaticParity(root/"item/ymir work/item/weapon/07200.gr2");
        StaticParity(root/"Zone/ymir work/zone/dungeon/deviltower2f/deviltower2f.gr2");
        StaticParity(root/"metin2_patch_dragon_rock_mobs/ymir work/monster2/gnoll_boss/gnoll_boss_lod_01.gr2");
        for(const char* clip:{"wait","walk","run","attack"}) AnimationParity(root/"PC/ymir work/pc/warrior/warrior_novice.gr2",root/(std::string("PC/ymir work/pc/warrior/general/")+clip+".gr2"),backend);
        for(const char* clip:{"00","02","03","20"}) AnimationParity(root/"Monster/ymir work/monster/wolf/wolf.gr2",root/(std::string("Monster/ymir work/monster/wolf/")+clip+".gr2"),backend);
        for(const char* clip:{"00","20"}) AnimationParity(root/"Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2",root/(std::string("Monster/ymir work/monster/misterious_diseased_bosshost/")+clip+".gr2"),backend);
        for(const char* clip:{"00","02","03"}) AnimationParity(root/"NPC/ymir work/npc/horse/horse_normal.gr2",root/(std::string("NPC/ymir work/npc/horse/")+clip+".gr2"),backend);
        AnimationParity(root/"PC/ymir work/pc/warrior/warrior_novice.gr2",root/"PC/ymir work/pc/warrior/general/attack.gr2",backend,true);
        FirstUse(root/"PC/ymir work/pc/warrior/warrior_novice.gr2",root/"PC/ymir work/pc/warrior/general/attack.gr2");
        FirstUse(root/"Monster/ymir work/monster/wolf/wolf.gr2",root/"Monster/ymir work/monster/wolf/20.gr2");
        FirstUse(root/"Monster/ymir work/monster/misterious_diseased_bosshost/misterious_diseased_bosshost.gr2",root/"Monster/ymir work/monster/misterious_diseased_bosshost/20.gr2");
        FirstUse(root/"NPC/ymir work/npc/horse/horse_normal.gr2",root/"NPC/ymir work/npc/horse/03.gr2");
        GrannyAnimationAdapter::ClearImportCache();
        backend.Shutdown(); DestroyWindow(window);
        Check(liveDocuments==0&&GR2::liveReaderDocuments==0&&AR::GetLifetimeCounts().clips==0&&AR::GetLifetimeCounts().skeletons==0,"all reader/runtime resources released");
        return 0;
    } catch(const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
}

