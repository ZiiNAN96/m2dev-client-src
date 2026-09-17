#include "GR2AssetProvider.h"
#include "EterBase/MapLoadTrace.h"
#include "GR2Reader.h"
#include "AssetRuntime/RuntimeAnimationInstance.h"
#include "AssetRuntime/AnimationRuntimeMode.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace AssetRuntime
{
namespace
{
namespace AR=AnimationRuntime;
class Document;
class Binding final : public MeshBinding
{
public:
    Binding(ModelHandle source,ModelHandle destination,std::size_t mesh) : source_(std::move(source)),destination_(std::move(destination))
    {
        const auto& skin=source_.Get()->meshes.at(mesh).skin;
        const auto& skeleton=destination_.Get()->skeleton;
        if(!skeleton) { indices_.push_back(0); return; }
        for(const auto& name:skin.boneNames) {
            auto bone=skeleton->FindBone(name);
            GR2::Require(bone>=0,"destination mesh binding bone missing: "+name); indices_.push_back(bone);
        }
        if(indices_.empty()) indices_.push_back(0);
    }
    std::span<const std::int32_t> BoneIndices() const override { return indices_; }
    AssetError DeformVertices(std::span<std::byte>,std::span<const float>,bool) const override { return AssetError::UnsupportedLayout; }
private:
    ModelHandle source_,destination_;
    std::vector<std::int32_t> indices_;
};
class Instance final : public RuntimeAnimationInstance
{
public:
    Instance(ModelHandle owner,std::shared_ptr<const AR::RuntimeSkeleton> skeleton)
        : RuntimeAnimationInstance(std::move(owner),std::move(skeleton)) {}
    AssetError SetMotion(const AnimationHandle&,float,float,int,float) override;
    void UpdateTransform(float elapsed,std::span<float,16> matrix) const override
    {
        if(!clip_ || failed_ || !std::isfinite(elapsed)) return;
        const float blend=BlendWeight();
        if(!motion_.periodic && !previousMotion_.periodic) {
            for(unsigned c=0;c<3;++c) for(unsigned axis=0;axis<3;++axis)
                matrix[12+c]+=elapsed*matrix[axis*4+c]*(motion_.velocity[axis]*speed_*blend+previousMotion_.velocity[axis]*previousSpeed_*(1-blend));
            return;
        }
        std::array<float,3> translation{},rotation{},previousTranslation{},previousRotation{};
        if(!motion_.Delta(elapsed*speed_,translation,rotation) ||
            !previousMotion_.Delta(elapsed*previousSpeed_,previousTranslation,previousRotation)) return;
        AR::LocalTransform delta;
        for(unsigned i=0;i<3;++i) {
            delta.translation[i]=translation[i]*blend+previousTranslation[i]*(1-blend);
            rotation[i]=rotation[i]*blend+previousRotation[i]*(1-blend);
        }
        const double angle=std::sqrt(double(rotation[0])*rotation[0]+double(rotation[1])*rotation[1]+double(rotation[2])*rotation[2]);
        if(angle>0) {
            for(unsigned i=0;i<3;++i) delta.rotation[i]=static_cast<float>(rotation[i]*std::sin(angle*.5)/angle);
            delta.rotation[3]=static_cast<float>(std::cos(angle*.5));
        }
        AR::Matrix original;std::copy(matrix.begin(),matrix.end(),original.begin());
        const auto updated=AR::Multiply(AR::LocalMatrix(delta),original);
        if(std::all_of(updated.begin(),updated.end(),[](float x){return std::isfinite(x);})) std::copy(updated.begin(),updated.end(),matrix.begin());
    }
    std::unique_ptr<MeshBinding> CreateMeshBinding(const ModelHandle& source,std::size_t mesh) const override
    {
        if(!source || mesh>=source.Get()->meshes.size()) return {};
        try { return std::make_unique<Binding>(source,owner_,mesh); } catch(const GR2::Error&) { return {}; }
    }
private:
    GR2::RootMotion motion_,previousMotion_;
};
class Document final : public AssetDocument
{
public:
    Document(AssetId id,GR2::Contents contents) : AssetDocument(std::move(id)),data_(std::move(contents.modelData)),clips_(std::move(contents.animationData))
    {
        models_=std::move(contents.models); animations_=std::move(contents.animations); ++GR2::liveReaderDocuments;
    }
    ~Document() override { --GR2::liveReaderDocuments; }
    const GR2::AnimationData* Clip(std::size_t index) const { return index<clips_.size()?&clips_[index]:nullptr; }
    const AR::RuntimeSkeleton* Skeleton(std::size_t index) const { return index<data_.size()?data_[index].skeleton.get():nullptr; }
    std::shared_ptr<const AR::RuntimeAnimationClip> BoundClip(std::size_t index,const AR::RuntimeSkeleton& skeleton,
        std::string_view modelName,unsigned boundary,std::string& error) const
    {
        if(index>=clips_.size() || boundary>3) { error="invalid animation binding"; return {}; }
        const auto& groups=clips_[index].groups;
        const auto group=std::find_if(groups.begin(),groups.end(),[&](const auto& item){return item.name==modelName;});
        if(group==groups.end()) { error="animation track group not found"; return {}; }
        const std::array<std::uint64_t,3> key{index,skeleton.BindingId(),static_cast<std::uint64_t>(group-groups.begin())};
        const auto found=boundClips_.find(key);
        if(found!=boundClips_.end() && found->second[boundary]) { ++GR2::boundClipHits; AnimationStallAudit::InstanceHit(); return found->second[boundary]; }
        auto clip=GR2::BindAnimation(animations_[index],clips_[index],skeleton,error,boundary,modelName);
        if(!clip) return {};
        std::size_t bytes=sizeof(*clip)+clip->Tracks().size()*sizeof(AR::AnimationTrack);
        for(const auto& track:clip->Tracks()) {
            bytes+=track.translation.keys.capacity()*sizeof(track.translation.keys.front());
            bytes+=track.rotation.keys.capacity()*sizeof(track.rotation.keys.front());
            bytes+=track.scaleShear.keys.capacity()*sizeof(track.scaleShear.keys.front());
        }
        // Normal asset ownership, shared by instances/LODs using this document.
        // No process cache, fingerprinting, or retention beyond document life.
        constexpr std::size_t limit=64u*1024u*1024u;
        if((found!=boundClips_.end() || boundClips_.size()<64) && bytes<=limit-boundBytes_) {
            boundClips_[key][boundary]=clip; boundBytes_+=bytes;
        } else ++GR2::boundClipBypasses;
        return clip;
    }
    AssetError CopyVertices(std::size_t model,std::size_t mesh,VertexLayout layout,std::span<std::byte> destination) const override
    {
        if(model>=data_.size() || mesh>=data_[model].meshes.size()) return AssetError::InvalidHandle;
        if(released_) return AssetError::UploadDataReleased;
        const auto stride=VertexStride(layout); if(!stride) return AssetError::UnsupportedLayout;
        const auto& vertices=data_[model].meshes[mesh].vertices;
        if(destination.size()<vertices.size()*stride) return AssetError::BufferTooSmall;
        for(std::size_t i=0;i<vertices.size();++i) {
            const auto& v=vertices[i]; auto* out=destination.data()+i*stride;
            std::memcpy(out,v.position.data(),12); std::size_t offset=12;
            if(layout==VertexLayout::WeightedPositionNormalUV) { std::memcpy(out+12,v.weights.data(),4); std::memcpy(out+16,v.joints.data(),4); offset=20; }
            std::memcpy(out+offset,v.normal.data(),12); std::memcpy(out+offset+12,v.uv.data(),8);
            if(layout==VertexLayout::PositionNormalUV2) std::memcpy(out+offset+20,v.uv1.data(),8);
        }
        return AssetError::None;
    }
    AssetError CopyIndices(std::size_t model,std::size_t mesh,IndexWidth width,std::span<std::byte> destination) const override
    {
        if(model>=data_.size() || mesh>=data_[model].meshes.size()) return AssetError::InvalidHandle;
        if(released_) return AssetError::UploadDataReleased;
        if(width!=IndexWidth::UInt16 && width!=IndexWidth::UInt32) return AssetError::InvalidIndexWidth;
        const auto stride=static_cast<std::size_t>(width); const auto& indices=data_[model].meshes[mesh].indices;
        if(destination.size()<indices.size()*stride) return AssetError::BufferTooSmall;
        if(width==IndexWidth::UInt16 && std::any_of(indices.begin(),indices.end(),[](auto i){return i>65535;})) return AssetError::InvalidIndexWidth;
        for(std::size_t i=0;i<indices.size();++i) if(stride==4) std::memcpy(destination.data()+i*4,&indices[i],4);
            else { auto value=static_cast<std::uint16_t>(indices[i]); std::memcpy(destination.data()+i*2,&value,2); }
        return AssetError::None;
    }
    void ReleaseUploadData() override
    {
        if(released_) return;
        for(auto& model:data_) for(auto& mesh:model.meshes) { std::vector<GR2::Vertex>().swap(mesh.vertices); std::vector<std::uint32_t>().swap(mesh.indices); }
        released_=true;
    }
    std::unique_ptr<PoseEvaluator> CreatePose(const ModelHandle& model) const override { return CreateAnimationInstance(model); }
    std::unique_ptr<AnimationInstance> CreateAnimationInstance(const ModelHandle& model) const override
    {
        if(!model || model.GetDocument().get()!=this || model.Index()>=data_.size() || !data_[model.Index()].skeleton) return {};
        return std::make_unique<Instance>(model,data_[model.Index()].skeleton);
    }
private:
    std::vector<GR2::ModelData> data_; std::vector<GR2::AnimationData> clips_; bool released_{};
    mutable std::map<std::array<std::uint64_t,3>,std::array<std::shared_ptr<const AR::RuntimeAnimationClip>,4>> boundClips_;
    mutable std::size_t boundBytes_{};
};
AssetError Instance::SetMotion(const AnimationHandle& handle,float time,float blend,int loops,float speed)
{
    // SetMotionAtEnd may pass our own clipOwner_; retain it through error logging.
    const auto keepAlive=handle.GetDocument();
    const auto* document=dynamic_cast<const Document*>(keepAlive.get());
    auto fail=[&](AssetError error) { ready_=false; failed_=true; clip_.reset(); previous_.reset(); variants_={}; clipOwner_={}; return error; };
    if(!handle || !document) return fail(AssetError::ProviderMismatch);
    if(!std::isfinite(time)||!std::isfinite(blend)||blend<0||loops<0||!std::isfinite(speed)||speed<=0) return fail(AssetError::InvalidInput);
    const auto* data=document->Clip(handle.Index()); if(!data) return fail(AssetError::InvalidHandle);
    const GR2::TrackGroup* group=nullptr;
    for(const auto& item:data->groups) if(item.name==owner_.Get()->name) {
        if(group) return fail(AssetError::InvalidAsset); group=&item;
    }
    if(!group || std::none_of(group->tracks.begin(),group->tracks.end(),[&](const auto& t){return skeleton_->FindBone(t.name)>=0;})) {
        const bool previousReady=ready_;
        fail(AssetError::NoMatchingTracks); failed_=false; ready_=previousReady; return AssetError::NoMatchingTracks;
    }
    std::string error;
    std::array<std::shared_ptr<const AR::RuntimeAnimationClip>,4> variants;
    if(clipOwner_.GetDocument()==handle.GetDocument() && clipOwner_.Index()==handle.Index()) variants=variants_;
    const unsigned boundary=loops==1?0u:loops>1?2u:3u;
    auto clip=variants[boundary];
    if(!clip) clip=document->BoundClip(handle.Index(),*skeleton_,owner_.Get()->name,boundary,error); variants[boundary]=clip;
    if(clip && loops>1) for(unsigned b:{1u,3u}) {
        if(!variants[b]) variants[b]=document->BoundClip(handle.Index(),*skeleton_,owner_.Get()->name,b,error);
        if(!variants[b]) { clip.reset(); break; }
    }
    if(!clip) {
        fail(AssetError::EvaluationFailed); ++animationRuntimeFailures;
        if(animationRuntimeErrorSink) animationRuntimeErrorSink(("GR2 animation: "+error+" file="+document->Id()).c_str());
        return AssetError::EvaluationFailed;
    }
    previous_=clip_; previousStart_=start_; previousSpeed_=speed_;
    previousMotion_=motion_;motion_.periodic=group->periodicLoop;
    for(unsigned i=0;i<3;++i) motion_.velocity[i]=handle.Get()->duration>0?group->loopTranslation[i]/handle.Get()->duration:0;
    blendStart_=time; blendDuration_=blend; start_=time; speed_=speed; loops_=loops;
    // Collision/attachment queries can occur before the next pose evaluation.
    // A successful control change preserves the last valid pose.
    clip_=std::move(clip); variants_=std::move(variants); clipOwner_=handle; failed_=false; return AssetError::None;
}
class Provider final : public AssetProvider
{
public:
    LoadResult Load(AssetId id,std::span<const std::byte> bytes) override
    {
    MapLoadTrace::Scope p0lScope("Assets","GR2 parse","cpu");
    MapLoadTrace::Count("gr2-parse",id,bytes.size(),true);

        try {
            AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Import);
            ++GR2::nativeFileReads;
            AnimationStallAudit::WorkScope containerAudit(AnimationStallAudit::Work::Container);
            MapLoadTrace::Scope containerTrace("Assets","GR2 container");
            GR2::File file(bytes); containerTrace.Stop(); containerAudit.Stop();
            AnimationStallAudit::WorkScope parseAudit(AnimationStallAudit::Work::Parse);
            auto content=GR2::Read(file); parseAudit.Stop();
            return {AssetHandle(std::make_shared<Document>(std::move(id),std::move(content))),AssetError::None};
        } catch(const std::exception& error) { return {{},AssetError::InvalidAsset,"GR2 reader: "+std::string(error.what())}; }
    }
};
}
AssetProvider& GetGR2AssetProvider() { static Provider provider; return provider; }
AssetError PrepareGR2Animation(const ModelHandle& model,const AnimationHandle& animation,unsigned boundaryMask)
{
    if(!model || !animation || !boundaryMask || boundaryMask>15) return AssetError::InvalidInput;
    const auto* modelDocument=dynamic_cast<const Document*>(model.GetDocument().get());
    const auto* animationDocument=dynamic_cast<const Document*>(animation.GetDocument().get());
    if(!modelDocument || !animationDocument) return AssetError::ProviderMismatch;
    const auto* skeleton=modelDocument->Skeleton(model.Index());
    const auto* source=animationDocument->Clip(animation.Index());
    if(!source || !skeleton) return AssetError::NoMatchingTracks;
    const auto group=std::find_if(source->groups.begin(),source->groups.end(),[&](const auto& g){return g.name==model.Get()->name;});
    if(group==source->groups.end() || std::none_of(group->tracks.begin(),group->tracks.end(),[&](const auto& t){return skeleton->FindBone(t.name)>=0;}))
        return AssetError::NoMatchingTracks;
    ++GR2::prewarmRequests;
    for(unsigned boundary=0;boundary<4;++boundary) if(boundaryMask&(1u<<boundary)) {
        std::string error;
        if(!animationDocument->BoundClip(animation.Index(),*skeleton,model.Get()->name,boundary,error)) {
            ++GR2::prewarmFailures;
            if(animationRuntimeErrorSink) animationRuntimeErrorSink(("GR2 prewarm: "+error+" boundary="+std::to_string(boundary)+" file="+animationDocument->Id()).c_str());
            return AssetError::EvaluationFailed;
        }
    }
    return AssetError::None;
}
}
