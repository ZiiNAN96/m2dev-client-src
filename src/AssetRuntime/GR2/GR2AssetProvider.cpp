#include "GR2AssetProvider.h"
#include "GR2Reader.h"
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
class Instance final : public AnimationInstance
{
public:
    Instance(ModelHandle owner,std::shared_ptr<const AR::RuntimeSkeleton> skeleton)
        : owner_(std::move(owner)),skeleton_(std::move(skeleton)) { PreparePose(); Evaluate({}); ++liveIndependentAnimationInstances; }
    ~Instance() override { --liveIndependentAnimationInstances; }
    bool PreparePose() override
    {
        if(!skeleton_) return false;
        const auto count=skeleton_->Bones().size(); pose_.Prepare(count); scratch_.Prepare(count);
        model_.resize(count); palette_.resize(count);
        return true;
    }
    AssetError SetAnimation(const AnimationHandle& clip,float time) override
    {
        if(clip && clipOwner_.GetDocument()==clip.GetDocument() && clipOwner_.Index()==clip.Index() && clip_) { SetClock(time); return AssetError::None; }
        const auto result=SetMotion(clip,0,0,0,1); SetClock(time); return result;
    }
    AssetError SetMotion(const AnimationHandle&,float,float,int,float) override;
    AssetError ChangeMotion(const AnimationHandle& clip,float time,int loops,float speed) override { return SetMotion(clip,time,0,loops,speed); }
    AssetError CopyMotionFrom(AnimationInstance& source,float time,bool freeSource) override
    {
        auto* other=dynamic_cast<Instance*>(&source); if(!other) return AssetError::ProviderMismatch;
        if(other==this || !other->IsPlaying()) return AssetError::InvalidInput;
        auto result=SetMotion(other->clipOwner_,time,0,other->loops_,other->speed_);
        if(result!=AssetError::None) return result;
        start_=time-(other->clock_-other->start_);
        if(freeSource) { other->clip_.reset(); other->clipOwner_={}; other->previous_.reset(); other->variants_={}; }
        return result;
    }
    bool IsPlaying() const override { return clip_ && (!loops_ || (clock_-start_)*speed_<clip_->Duration()*loops_); }
    void SetMotionAtEnd() override { if(clip_) { const auto duration=static_cast<float>(clip_->Duration()); if(SetMotion(clipOwner_,clock_,0,1,speed_)==AssetError::None) start_=clock_-duration/speed_; } }
    void SetClock(float time) override { clock_=time; }
    void FreeCompletedControls() override { if(clock_>=blendStart_+blendDuration_) previous_.reset(); }
    void UpdateTransform(float elapsed,std::span<float,16> matrix) const override
    {
        if(!clip_ || failed_ || !std::isfinite(elapsed)) return;
        const float blend=BlendWeight();
        for(unsigned c=0;c<3;++c) for(unsigned axis=0;axis<3;++axis)
            matrix[12+c]+=elapsed*matrix[axis*4+c]*(velocity_[axis]*speed_*blend+previousVelocity_[axis]*previousSpeed_*(1-blend));
    }
    std::span<const float> BoneWorldMatrix(BoneId bone) const override
    {
        return ready_ && bone>=0 && std::size_t(bone)<model_.size()?std::span<const float>(model_[bone]):std::span<const float>{};
    }
    PoseView CompositePose() const override { return ready_?PoseView{{palette_[0].data(),palette_.size()*16}}:PoseView{}; }
    std::unique_ptr<MeshBinding> CreateMeshBinding(const ModelHandle& source,std::size_t mesh) const override
    {
        if(!source || mesh>=source.Get()->meshes.size()) return {};
        try { return std::make_unique<Binding>(source,owner_,mesh); } catch(const GR2::Error&) { return {}; }
    }
    PoseResult Evaluate(const PoseRequest& request) override
    {
        AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Pose);
        ready_=false;
        if(!skeleton_ || failed_ || !std::isfinite(clock_) || (!request.attachmentMatrix.empty() && request.attachmentMatrix.size()!=16)) return {{},AssetError::EvaluationFailed};
        if(clip_) {
            const double time=(clock_-start_)*speed_;
            if(loops_>1) {
                const auto cycle=clip_->Duration()>0?std::floor(std::max(0.0,time)/clip_->Duration()):0;
                const unsigned boundary=(cycle>0?1u:0u)|(cycle<loops_-1?2u:0u);
                clip_=variants_[boundary];
                if(!clip_) return {{},AssetError::EvaluationFailed};
            }
            const auto mode=loops_==1 || (loops_>0 && time>=clip_->Duration()*loops_)?AR::TimeMode::Clamp:AR::TimeMode::Loop;
            if(!AR::Sample(*skeleton_,*clip_,loops_>1 && mode==AR::TimeMode::Clamp?clip_->Duration():time,mode,pose_)) return {{},AssetError::EvaluationFailed};
            if(previous_ && blendDuration_>0 && clock_<blendStart_+blendDuration_) {
                if(!AR::Sample(*skeleton_,*previous_,(clock_-previousStart_)*previousSpeed_,AR::TimeMode::Loop,scratch_) ||
                    !AR::Blend(scratch_,pose_,BlendWeight(),pose_)) return {{},AssetError::EvaluationFailed};
            }
        } else for(std::size_t i=0;i<pose_.localTransforms.size();++i) pose_.localTransforms[i]=skeleton_->Bones()[i].localBind;
        AR::Matrix parent;
        if(!request.attachmentMatrix.empty()) std::copy(request.attachmentMatrix.begin(),request.attachmentMatrix.end(),parent.begin());
        if(!AR::Evaluate(*skeleton_,pose_,model_,request.attachmentMatrix.empty()?nullptr:&parent)) return {{},AssetError::EvaluationFailed};
        {
            AnimationStallAudit::WorkScope paletteAudit(AnimationStallAudit::Work::Palette);
            if(!AR::BuildPalette(*skeleton_,model_,palette_)) return {{},AssetError::EvaluationFailed};
        }
        ++independentPoseSamples; ready_=true; return {CompositePose(),AssetError::None};
    }
private:
    float BlendWeight() const { if(!previous_ || blendDuration_<=0) return 1; const float x=std::clamp((clock_-blendStart_)/blendDuration_,0.0f,1.0f); return x*x*(3-2*x); }
    ModelHandle owner_; AnimationHandle clipOwner_;
    std::shared_ptr<const AR::RuntimeSkeleton> skeleton_;
    std::shared_ptr<const AR::RuntimeAnimationClip> clip_,previous_;
    std::array<std::shared_ptr<const AR::RuntimeAnimationClip>,4> variants_;
    AR::AnimationPose pose_,scratch_; std::vector<AR::Matrix> model_,palette_;
    float clock_{},start_{},speed_=1,blendStart_{},blendDuration_{},previousStart_{},previousSpeed_=1;
    std::array<float,3> velocity_{},previousVelocity_{};
    int loops_{}; bool ready_{},failed_{};
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
    previousVelocity_=velocity_;
    for(unsigned i=0;i<3;++i) velocity_[i]=handle.Get()->duration>0?group->loopTranslation[i]/handle.Get()->duration:0;
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
        try {
            AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Import);
            ++GR2::nativeFileReads;
            AnimationStallAudit::WorkScope containerAudit(AnimationStallAudit::Work::Container);
            GR2::File file(bytes); containerAudit.Stop();
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
