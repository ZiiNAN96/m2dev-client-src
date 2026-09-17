#pragma once
#include "AssetRuntime.h"
#include "AnimationRuntimeMode.h"
#include "AnimationStallAudit.h"
#include "AnimationRuntime/AnimationRuntime.h"
#include "EterBase/MapLoadTrace.h"
#include <algorithm>
#include <cmath>

namespace AssetRuntime
{
namespace AR=AnimationRuntime;
// Shared production playback, crossfade, pose and palette path for native asset providers.
// All source decoding/binding is completed by the provider before this instance is created.
class IndexedBinding final : public MeshBinding
{
public:
    IndexedBinding(ModelHandle source,std::size_t mesh):owner_(std::move(source)),indices_(owner_.Get()->meshes.at(mesh).skin.meshToSkeleton) {}
    std::span<const std::int32_t> BoneIndices() const override { return indices_; }
    AssetError DeformVertices(std::span<std::byte>,std::span<const float>,bool) const override { return AssetError::UnsupportedLayout; }
private:
    ModelHandle owner_;
    std::vector<BoneId> indices_;
};
class RuntimeAnimationInstance : public AnimationInstance
{
public:
    RuntimeAnimationInstance(ModelHandle owner,std::shared_ptr<const AR::RuntimeSkeleton> skeleton,
        std::shared_ptr<const std::vector<std::shared_ptr<const AR::RuntimeAnimationClip>>> clips = {})
        : owner_(std::move(owner)),skeleton_(std::move(skeleton)), clips_(std::move(clips)) { PreparePose(); Evaluate({}); ++liveIndependentAnimationInstances; }
    ~RuntimeAnimationInstance() override { --liveIndependentAnimationInstances; }
    bool PreparePose() override
    {
        MapLoadTrace::FirstUseScope trace("pose buffer initialization");
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
    AssetError SetMotion(const AnimationHandle& handle,float time,float blend,int loops,float speed) override
    {
        if(!handle || handle.GetDocument()!=owner_.GetDocument()) return AssetError::ProviderMismatch;
        if(!clips_ || handle.Index()>=clips_->size()) return AssetError::InvalidHandle;
        if(!std::isfinite(time)||!std::isfinite(blend)||blend<0||loops<0||!std::isfinite(speed)||speed<=0)
            return AssetError::InvalidInput;
        auto clip=(*clips_)[handle.Index()];
        if(!clip || clip->BindingId()!=skeleton_->BindingId()) return AssetError::InvalidHandle;
        previous_=clip_; previousStart_=start_; previousSpeed_=speed_; previousLoops_=loops_;
        blendStart_=time; blendDuration_=blend; start_=time; speed_=speed; loops_=loops;
        variants_.fill(clip); clip_=std::move(clip); clipOwner_=handle; failed_=false;
        return AssetError::None;
    }
    AssetError ChangeMotion(const AnimationHandle& clip,float time,int loops,float speed) override { return SetMotion(clip,time,0,loops,speed); }
    AssetError CopyMotionFrom(AnimationInstance& source,float time,bool freeSource) override
    {
        auto* other=dynamic_cast<RuntimeAnimationInstance*>(&source); if(!other) return AssetError::ProviderMismatch;
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
    // Root translation stays in the pose; gameplay movement is a provider/character policy.
    void UpdateTransform(float,std::span<float,16>) const override {}
    std::span<const float> BoneWorldMatrix(BoneId bone) const override
    {
        return ready_ && bone>=0 && std::size_t(bone)<model_.size()?std::span<const float>(model_[bone]):std::span<const float>{};
    }
    PoseView CompositePose() const override { return ready_?PoseView{{palette_[0].data(),palette_.size()*16}}:PoseView{}; }
    std::unique_ptr<MeshBinding> CreateMeshBinding(const ModelHandle& source,std::size_t mesh) const override
    {
        if(!source || source.GetDocument()!=owner_.GetDocument() || source.Index()!=owner_.Index() || mesh>=source.Get()->meshes.size()) return {};
        return std::make_unique<IndexedBinding>(source,mesh);
    }
    PoseResult Evaluate(const PoseRequest& request) override
    {
        MapLoadTrace::FirstUseScope trace("pose evaluation");
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
                if(!AR::Sample(*skeleton_,*previous_,(clock_-previousStart_)*previousSpeed_,(clips_ && previousLoops_==1)?AR::TimeMode::Clamp:AR::TimeMode::Loop,scratch_) ||
                    !AR::Blend(scratch_,pose_,BlendWeight(),pose_)) return {{},AssetError::EvaluationFailed};
            }
        } else for(std::size_t i=0;i<pose_.localTransforms.size();++i) pose_.localTransforms[i]=skeleton_->Bones()[i].localBind;
        AR::Matrix parent;
        MapLoadTrace::FirstUseScope matrices("bone matrix generation");
        if(!request.attachmentMatrix.empty()) std::copy(request.attachmentMatrix.begin(),request.attachmentMatrix.end(),parent.begin());
        if(!AR::Evaluate(*skeleton_,pose_,model_,request.attachmentMatrix.empty()?nullptr:&parent)) return {{},AssetError::EvaluationFailed};
        {
            AnimationStallAudit::WorkScope paletteAudit(AnimationStallAudit::Work::Palette);
            if(!AR::BuildPalette(*skeleton_,model_,palette_)) return {{},AssetError::EvaluationFailed};
        }
        ++independentPoseSamples; ready_=true; return {CompositePose(),AssetError::None};
    }
protected:
    float BlendWeight() const { if(!previous_ || blendDuration_<=0) return 1; const float x=std::clamp((clock_-blendStart_)/blendDuration_,0.0f,1.0f); return x*x*(3-2*x); }
    ModelHandle owner_; AnimationHandle clipOwner_;
    std::shared_ptr<const AR::RuntimeSkeleton> skeleton_;
    std::shared_ptr<const AR::RuntimeAnimationClip> clip_,previous_;
    std::array<std::shared_ptr<const AR::RuntimeAnimationClip>,4> variants_;
    AR::AnimationPose pose_,scratch_; std::vector<AR::Matrix> model_,palette_;
    float clock_{},start_{},speed_=1,blendStart_{},blendDuration_{},previousStart_{},previousSpeed_=1;
    std::shared_ptr<const std::vector<std::shared_ptr<const AR::RuntimeAnimationClip>>> clips_;
    int previousLoops_{};
    int loops_{}; bool ready_{},failed_{};
};
}
