#include "GR2Reader.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <algorithm>

namespace AssetRuntime::GR2
{
LocalTransform ReadTransform(const File& f, Ref at)
{
    f.Bytes(at,68); LocalTransform result; result.flags=f.Uint(at);
    Require((result.flags&~7u)==0,"invalid transform flags");
    for(unsigned i=0;i<3;++i) result.position[i]=f.Float(f.Add(at,4+i*4));
    for(unsigned i=0;i<4;++i) result.orientation[i]=f.Float(f.Add(at,16+i*4));
    for(unsigned i=0;i<9;++i) result.scaleShear[i]=f.Float(f.Add(at,32+i*4));
    return result;
}
AnimationRuntime::LocalTransform RuntimeTransform(const LocalTransform& source)
{
    AnimationRuntime::LocalTransform result;
    if(source.flags&1) result.translation=source.position;
    if(source.flags&2) result.rotation=source.orientation;
    if(source.flags&4) result.scaleShear=source.scaleShear;
    return result;
}
SkeletonAsset ReadSkeleton(Types& t, Object source)
{
    SkeletonAsset result; result.name=t.Text(source,"Name");
    const auto bones=t.Array(source,"Bones"); Require(bones.size()<=65536,"invalid bone count");
    for(const auto& bone:bones) {
        BoneAsset value; value.id=static_cast<BoneId>(result.bones.size());
        value.name=t.Text(bone,"Name"); value.parentIndex=t.Integer(bone,"ParentIndex");
        value.localBind=ReadTransform(t.file,t.Field(bone,t.Find(bone,"Transform")?"Transform":"LocalTransform"));
        auto inverse=t.Reals(bone,t.Find(bone,"InverseWorldTransform")?"InverseWorldTransform":"InverseWorld4x4");
        Require(inverse.size()==16,"inverse bind matrix dimension"); std::copy(inverse.begin(),inverse.end(),value.inverseBind.begin());
        if(value.parentIndex==-1) result.rootIndex=value.id;
        result.bones.push_back(std::move(value));
    }
    return result;
}
Contents Read(const File& f)
{
    Types t(f); Contents result;
    const auto root=t.Root();
    auto models=t.Array(root,"Models"), animations=t.Array(root,"Animations");
    Require(models.size()<=1024 && animations.size()<=4096,"model/animation allocation limit");
    for(auto source:models) {
        ModelAsset model; ModelData data; model.name=t.Text(source,"Name");
        data.initialPlacement=ReadTransform(f,t.Field(source,"InitialPlacement"));
        if(auto skeleton=t.Child(source,"Skeleton")) {
            AnimationStallAudit::WorkScope skeletonAudit(AnimationStallAudit::Work::Skeleton);
            model.skeleton=ReadSkeleton(t,skeleton);
            std::vector<AnimationRuntime::SkeletonBone> bones;
            for(const auto& b:model.skeleton->bones) bones.push_back({b.name,b.parentIndex,RuntimeTransform(b.localBind),b.inverseBind});
            if(!bones.empty()) {
                auto runtime=std::make_shared<AnimationRuntime::RuntimeSkeleton>(); std::string error;
                const bool valid=runtime->Initialize(std::move(bones),error);
                if(!valid && (error.find("exactly one")!=std::string::npos || error.find("duplicate")!=std::string::npos)) Unsupported("runtime skeleton: "+error);
                Require(valid,"skeleton: "+error); data.skeleton=std::move(runtime);
            }
        }
        std::map<Ref,std::uint32_t> materials;
        bool rigid=false,skinned=false;
        for(auto binding:t.Array(source,"MeshBindings")) {
            auto mesh=t.Child(binding,"Mesh"); Require(bool(mesh),"null model mesh");
            MeshAsset metadata; auto meshData=ReadMesh(t,mesh,metadata,model,materials,result);
            rigid|=metadata.deformation==Deformation::Rigid; skinned|=metadata.deformation==Deformation::Skinned;
            model.meshes.push_back(std::move(metadata)); data.meshes.push_back(std::move(meshData));
        }
        model.deformation=skinned?(rigid?Deformation::Mixed:Deformation::Skinned):Deformation::Rigid;
        // Empty model records in animation files are valid legacy metadata.
        // Keep the adapter contract; zero meshes submit no geometry.
        bool narrow=true;
        for(const auto& mesh:data.meshes) for(auto index:mesh.indices) narrow&=index<=65535;
        if(narrow) model.preferredIndexWidth=IndexWidth::UInt16;
        else if(skinned) Unsupported("skinned indices exceed existing 16-bit renderer contract");
        for(std::uint32_t i=0;i<animations.size();++i) model.animations.push_back(i);
        result.models.push_back(std::move(model)); result.modelData.push_back(std::move(data));
    }
    for(auto source:animations) {
        AnimationAsset metadata; auto data=ReadAnimation(t,source,metadata,result);
        result.animations.push_back(std::move(metadata)); result.animationData.push_back(std::move(data));
    }
    return result;
}
}
