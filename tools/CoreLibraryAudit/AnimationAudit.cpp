#include "AuditCommon.h"
#include <acl/compression/compress.h>
#include <acl/compression/transform_error_metrics.h>
#include <acl/core/ansi_allocator.h>
#include <acl/decompression/decompress.h>
#include <ozz/animation/offline/animation_builder.h>
#include <ozz/animation/offline/raw_animation.h>
#include <ozz/animation/offline/raw_skeleton.h>
#include <ozz/animation/offline/skeleton_builder.h>
#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/blending_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/memory/allocator.h>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <memory>
#include <cstring>
#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#include <DbgHelp.h>
#pragma comment(lib, "Dbghelp.lib")
// Keep a native failure diagnosable without attaching a debugger to the client.
static LONG WINAPI AuditException(EXCEPTION_POINTERS* failure) {
    HANDLE process=GetCurrentProcess();
    SymSetOptions(SYMOPT_LOAD_LINES|SYMOPT_UNDNAME);
    SymInitialize(process,nullptr,TRUE);
    CONTEXT context=*failure->ContextRecord;
    STACKFRAME64 frame{};
    frame.AddrPC={context.Rip,0,AddrModeFlat};
    frame.AddrFrame={context.Rbp,0,AddrModeFlat};
    frame.AddrStack={context.Rsp,0,AddrModeFlat};
    std::cerr<<"Native audit exception 0x"<<std::hex<<failure->ExceptionRecord->ExceptionCode<<std::dec<<'\n';
    for(int i=0;i<32;++i) {
        alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO)+MAX_SYM_NAME]{};
        auto* symbol=reinterpret_cast<SYMBOL_INFO*>(storage);symbol->SizeOfStruct=sizeof(SYMBOL_INFO);symbol->MaxNameLen=MAX_SYM_NAME;
        DWORD64 displacement{};IMAGEHLP_LINE64 line{};line.SizeOfStruct=sizeof(line);DWORD lineOffset{};
        std::cerr<<"0x"<<std::hex<<frame.AddrPC.Offset<<std::dec;
        if(SymFromAddr(process,frame.AddrPC.Offset,&displacement,symbol))std::cerr<<' '<<symbol->Name<<'+'<<displacement;
        if(SymGetLineFromAddr64(process,frame.AddrPC.Offset,&lineOffset,&line))std::cerr<<' '<<line.FileName<<':'<<line.LineNumber;
        std::cerr<<'\n';
        if(!StackWalk64(IMAGE_FILE_MACHINE_AMD64,process,GetCurrentThread(),&frame,&context,nullptr,SymFunctionTableAccess64,SymGetModuleBase64,nullptr))break;
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static std::atomic_uint64_t allocations{};
void* operator new(std::size_t size) { ++allocations; if(auto p=std::malloc(size?size:1)) return p; throw std::bad_alloc(); }
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }

using namespace CLib;
namespace OA=ozz::animation;
static double sampleHz=60;
static bool qualityOnly=false;
struct OzzAllocator : ozz::memory::Allocator {
    ozz::memory::Allocator* base{}; uint64_t calls{}, requested{};
    void* Allocate(size_t size,size_t alignment) override { ++calls;requested+=size;return base->Allocate(size,alignment); }
    void Deallocate(void* p) override {base->Deallocate(p);}
};
struct AclAllocator : acl::iallocator {
    acl::ansi_allocator base; uint64_t calls{}, live{}, peak{};
    bool poison=std::getenv("CLIB_POISON_ACL_ALLOCATIONS")!=nullptr;
    void* allocate(size_t size,size_t alignment) override {
        ++calls; live+=size;peak=std::max(peak,live);
        void* memory=base.allocate(size,alignment);
        // Large finite float bit patterns expose dependence on unwritten buffers.
        if(poison&&memory)std::memset(memory,0x7f,size);
        return memory;
    }
    void deallocate(void* p,size_t size) override {if(p)live-=size;base.deallocate(p,size);}
};
struct Writer : acl::track_writer {
    AR::AnimationPose* pose{};
    void RTM_SIMD_CALL write_rotation(uint32_t index,rtm::quatf_arg0 value) { rtm::quat_store(value,pose->localTransforms[index].rotation.data()); }
    void RTM_SIMD_CALL write_translation(uint32_t index,rtm::vector4f_arg0 value) { rtm::vector_store3(value,pose->localTransforms[index].translation.data()); }
    void RTM_SIMD_CALL write_scale(uint32_t index,rtm::vector4f_arg0 value) {
        float v[4];rtm::vector_store(value,v);pose->localTransforms[index].scaleShear={v[0],0,0,0,v[1],0,0,0,v[2]};
    }
};
struct CheckedMetric : acl::qvvf_transform_error_metric {
    bool enabled=std::getenv("CLIB_CHECK_METRIC_ORDER")!=nullptr;
    void Validate(const local_to_object_space_args& args) const {
        if(!enabled || args.num_dirty_transforms!=args.num_transforms)return;
        std::vector<bool> written(args.num_transforms);
        for(uint32_t i=0;i<args.num_dirty_transforms;++i){
            const auto bone=args.dirty_transform_indices[i],parent=args.parent_transform_indices[bone];
            if(parent!=acl::k_invalid_track_index&&!written.at(parent))
                throw std::runtime_error("ACL metric reads unwritten parent: bone="+std::to_string(bone)+" parent="+std::to_string(parent));
            written.at(bone)=true;
        }
    }
    void local_to_object_space(const local_to_object_space_args& args,void* output) const override {
        Validate(args);acl::qvvf_transform_error_metric::local_to_object_space(args,output);
    }
    void local_to_object_space_no_scale(const local_to_object_space_args& args,void* output) const override {
        Validate(args);acl::qvvf_transform_error_metric::local_to_object_space_no_scale(args,output);
    }
};
struct Error {
    double translation{},rotationDegrees{},scale{},worldPosition{},worldMatrix{};
    void Compare(const AR::RuntimeSkeleton& sk,const AR::AnimationPose& a,const AR::AnimationPose& b) {
        std::vector<AR::Matrix> wa(sk.Bones().size()),wb(wa.size());
        Check(AR::Evaluate(sk,a,wa)&&AR::Evaluate(sk,b,wb),"Error world evaluation");
        for(size_t j=0;j<wa.size();++j) {
            const auto& x=a.localTransforms[j];const auto& y=b.localTransforms[j];
            double td=0,wd=0,dot=0,nx=0,ny=0;
            for(int k=0;k<3;++k){td+=std::pow(double(x.translation[k])-y.translation[k],2);wd+=std::pow(double(wa[j][12+k])-wb[j][12+k],2);}
            translation=std::max(translation,std::sqrt(td));worldPosition=std::max(worldPosition,std::sqrt(wd));
            for(int k=0;k<4;++k){dot+=double(x.rotation[k])*y.rotation[k];nx+=double(x.rotation[k])*x.rotation[k];ny+=double(y.rotation[k])*y.rotation[k];}
            rotationDegrees=std::max(rotationDegrees,2*std::acos(std::clamp(std::abs(dot)/std::sqrt(nx*ny),0.,1.))*180/3.141592653589793);
            for(int k=0;k<9;++k)scale=std::max(scale,std::abs(double(x.scaleShear[k])-y.scaleShear[k]));
            for(int k=0;k<16;++k)worldMatrix=std::max(worldMatrix,std::abs(double(wa[j][k])-wb[j][k]));
        }
    }
    void Print(const char* name)const {std::cout<<",\""<<name<<"\":{\"translation\":"<<translation<<",\"rotation_degrees\":"<<rotationDegrees<<",\"scale_shear\":"<<scale<<",\"world_position\":"<<worldPosition<<",\"world_matrix\":"<<worldMatrix<<'}';}
};
static ozz::math::Transform ToOzz(const AR::LocalTransform& t) {
    return {{t.translation[0],t.translation[1],t.translation[2]},
        {t.rotation[0],t.rotation[1],t.rotation[2],t.rotation[3]}, {t.scaleShear[0],t.scaleShear[4],t.scaleShear[8]}};
}
struct OzzData {
    ozz::unique_ptr<OA::Skeleton> skeleton;
    ozz::unique_ptr<OA::Animation> clip;
    std::vector<size_t> order;
    uint64_t skeletonBytes{};
    OzzData(const AR::RuntimeSkeleton& sk,const AR::RuntimeAnimationClip& source,OzzAllocator& allocator) {
        OA::offline::RawSkeleton raw;
        std::function<void(int,OA::offline::RawSkeleton::Joint::Children&)> build=[&](int parent,auto& children){
            for(size_t i=0;i<sk.Bones().size();++i)if(sk.Bones()[i].parent==parent){
                children.emplace_back();auto& child=children.back();child.name=std::to_string(i).c_str();child.transform=ToOzz(sk.Bones()[i].localBind);build(int(i),child.children);
            }
        };
        build(-1,raw.roots);const auto before=allocator.requested;skeleton=OA::offline::SkeletonBuilder()(raw);skeletonBytes=allocator.requested-before;Check(bool(skeleton),"ozz skeleton build");
        for(const char* name:skeleton->joint_names())order.push_back(std::stoul(name));
        OA::offline::RawAnimation animation;animation.duration=float(source.Duration());animation.tracks.resize(order.size());
        for(size_t j=0;j<order.size();++j){
            const auto idx=order[j];auto& out=animation.tracks[j]; const auto base=ToOzz(sk.Bones()[idx].localBind);
            const AR::AnimationTrack* t=nullptr;for(const auto& candidate:source.Tracks())if(candidate.targetBone==idx)t=&candidate;
            if(t) {
                for(const auto& k:t->translation.keys)out.translations.push_back({float(k.time),{k.value[0],k.value[1],k.value[2]}});
                for(const auto& k:t->rotation.keys)out.rotations.push_back({float(k.time),{k.value[0],k.value[1],k.value[2],k.value[3]}});
                for(const auto& k:t->scaleShear.keys)out.scales.push_back({float(k.time),{k.value[0],k.value[4],k.value[8]}});
            }
            if(out.translations.empty())out.translations.push_back({0,base.translation});
            if(out.rotations.empty())out.rotations.push_back({0,base.rotation});
            if(out.scales.empty())out.scales.push_back({0,base.scale});
        }
        Check(animation.Validate(),"ozz raw clip validation");clip=OA::offline::AnimationBuilder()(animation);Check(bool(clip),"ozz clip build");
    }
};
struct OzzActor {
    OA::SamplingJob::Context context;
    ozz::vector<ozz::math::SoaTransform> pose,second,blend;
    ozz::vector<ozz::math::Float4x4> world;
    explicit OzzActor(const OzzData& data):context(data.clip->num_tracks()),pose(data.skeleton->num_soa_joints()),second(pose.size()),blend(pose.size()),world(data.order.size()){}
    bool Sample(const OzzData& data,double time) {
        OA::SamplingJob job;job.animation=data.clip.get();job.context=&context;job.ratio=float(time/data.clip->duration());job.output=ozz::make_span(pose);return job.Run();
    }
    bool Blend(const OzzData& data) {
        OA::BlendingJob::Layer layers[2];layers[0].weight=.63f;layers[0].transform=ozz::make_span(pose);layers[1].weight=.37f;layers[1].transform=ozz::make_span(second);
        OA::BlendingJob job;job.layers=ozz::make_span(layers);job.rest_pose=data.skeleton->joint_rest_poses();job.output=ozz::make_span(blend);return job.Run();
    }
    bool Evaluate(const OzzData& data) {OA::LocalToModelJob job;job.skeleton=data.skeleton.get();job.input=ozz::make_span(pose);job.output=ozz::make_span(world);return job.Run();}
    void Copy(const OzzData& data,AR::AnimationPose& out)const {
        for(size_t j=0;j<data.order.size();++j){const auto& src=pose[j/4];auto& dst=out.localTransforms[data.order[j]];const size_t lane=j%4;
            auto scalar=[&](ozz::math::SimdFloat4 value){float v[4];ozz::math::StorePtrU(value,v);return v[lane];};
            dst.translation={scalar(src.translation.x),scalar(src.translation.y),scalar(src.translation.z)};
            dst.rotation={scalar(src.rotation.x),scalar(src.rotation.y),scalar(src.rotation.z),scalar(src.rotation.w)};
            dst.scaleShear={scalar(src.scale.x),0,0,0,scalar(src.scale.y),0,0,0,scalar(src.scale.z)};
        }
    }
};
struct Actor {
    AR::AnimationPose pose,second,blend;
    std::vector<AR::Matrix> world;
    acl::decompression_context<acl::default_transform_decompression_settings> context;
    explicit Actor(size_t n):world(n){pose.Prepare(n);second.Prepare(n);blend.Prepare(n);}
};
static void Audit(const std::string& label,const AR::RuntimeSkeleton& sk,const AR::RuntimeAnimationClip& clip,double importUs,OzzAllocator& oa) {
    std::cerr<<"Audit begin "<<label<<'\n';
    Check(clip.Duration()>0,"Positive duration required"); const size_t joints=sk.Bones().size();
    AR::AnimationPose reference,decoded;reference.Prepare(joints);decoded.Prepare(joints);
    double shear=0;size_t step=0,slerp=0;
    for(const auto& t:clip.Tracks()) {step+=t.translation.interpolation==AR::Interpolation::Step||t.rotation.interpolation==AR::Interpolation::Step||t.scaleShear.interpolation==AR::Interpolation::Step;slerp+=t.rotation.interpolation==AR::Interpolation::SphericalLinear;}
    AclAllocator allocator;acl::track_array_qvvf raw(allocator,uint32_t(joints));
    // IndexedForest preserves public bone IDs; those IDs need not be a hierarchy
    // traversal. ACL's metric walks input tracks in index order. Reorder only
    // encoder input and preserve original IDs through output_index.
    const auto& aclToBone=sk.EvaluationOrder();
    Check(aclToBone.size()==joints,"ACL hierarchy order size");
    std::vector<uint32_t> boneToAcl(joints,acl::k_invalid_track_index);
    size_t reordered=0;
    for(uint32_t i=0;i<joints;++i){
        Check(aclToBone[i]<joints&&boneToAcl[aclToBone[i]]==acl::k_invalid_track_index,"ACL hierarchy permutation");
        boneToAcl[aclToBone[i]]=i;reordered+=aclToBone[i]!=i;
    }
    const uint32_t count=uint32_t(std::ceil(clip.Duration()*sampleHz))+1;const float rate=float((count-1)/clip.Duration());
    std::vector<AR::AnimationTrack> uniformTracks(joints);
    for(size_t j=0;j<joints;++j)uniformTracks[j].targetBone=uint32_t(j);
    auto setupStart=Clock::now();
    for(uint32_t i=0;i<joints;++i){const auto j=aclToBone[i];acl::track_desc_transformf desc;
        desc.output_index=j;desc.parent_index=sk.Bones()[j].parent<0?acl::k_invalid_track_index:boneToAcl[sk.Bones()[j].parent];
        Check(desc.parent_index==acl::k_invalid_track_index||desc.parent_index<i,"ACL requires parent-first encoder input");
        desc.precision=.001f;desc.shell_distance=100.f;raw[i]=acl::track_qvvf::make_reserve(desc,allocator,count,rate);}
    for(uint32_t frame=0;frame<count;++frame){Check(AR::Sample(sk,clip,frame/double(rate),AR::TimeMode::Clamp,reference),"ACL resample");
        for(size_t j=0;j<joints;++j){const auto& t=reference.localTransforms[j];for(int k:{1,2,3,5,6,7})shear=std::max(shear,std::abs(double(t.scaleShear[k])));
            auto& track=uniformTracks[j];const double sampleTime=std::min(clip.Duration(),frame/double(rate));
            track.translation.keys.push_back({sampleTime,t.translation});track.rotation.keys.push_back({sampleTime,t.rotation});
            track.scaleShear.keys.push_back({sampleTime,{t.scaleShear[0],0,0,0,t.scaleShear[4],0,0,0,t.scaleShear[8]}});
            raw[boneToAcl[j]][frame]={rtm::quat_set(t.rotation[0],t.rotation[1],t.rotation[2],t.rotation[3]),rtm::vector_set(t.translation[0],t.translation[1],t.translation[2]),rtm::vector_set(t.scaleShear[0],t.scaleShear[4],t.scaleShear[8])};}
    }
    const double resampleUs=Micros(setupStart);auto settings=acl::get_default_compression_settings();CheckedMetric metric;settings.error_metric=&metric;settings.level=acl::compression_level8::medium;
    AR::RuntimeAnimationClip uniform;std::string uniformError;
    Check(uniform.Initialize("uniform",clip.Duration(),false,std::move(uniformTracks),sk,uniformError),uniformError);
    AR::AnimationPose uniformPose;uniformPose.Prepare(joints);
    acl::compressed_tracks* compressed=nullptr;acl::output_stats stats;setupStart=Clock::now();
    auto result=acl::compress_track_list(allocator,raw,settings,compressed,stats);Check(result.empty(),result.c_str());const double compressUs=Micros(setupStart);
    Check(compressed!=nullptr,"ACL compressed output");const auto compressedBytes=compressed->get_size();
    setupStart=Clock::now();OzzData ozz(sk,clip,oa);const double ozzBuildUs=Micros(setupStart);
    OzzActor ozzQuality(ozz);Actor aclQuality(joints);Check(aclQuality.context.initialize(*compressed),"ACL initialize");Writer writer;writer.pose=&decoded;
    Error aclError,ozzError,resampleError,quantizationError;double ozzWorldMatrixError=0;
    const int errorSamples=std::max(257,int(count)*4);
    std::vector<double> qualityTimes;qualityTimes.reserve(errorSamples+1);
    for(int k=0;k<=errorSamples;++k)qualityTimes.push_back(clip.Duration()*k/errorSamples);
    for(const auto& track:clip.Tracks()){
        auto boundaries=[&](const auto& keys){for(const auto& key:keys)for(double delta:{-1e-6,0.,1e-6})qualityTimes.push_back(std::clamp(key.time+delta,0.,clip.Duration()));};
        boundaries(track.translation.keys);boundaries(track.rotation.keys);boundaries(track.scaleShear.keys);
    }
    std::sort(qualityTimes.begin(),qualityTimes.end());qualityTimes.erase(std::unique(qualityTimes.begin(),qualityTimes.end()),qualityTimes.end());
    // A/current, B/uncompressed bake and C/ACL must use exactly the same times.
    for(const double t:qualityTimes){Check(AR::Sample(sk,clip,t,AR::TimeMode::Clamp,reference),"Reference sample");
        aclQuality.context.seek(float(t),acl::sample_rounding_policy::none);aclQuality.context.decompress_tracks(writer);aclError.Compare(sk,reference,decoded);
        Check(AR::Sample(sk,uniform,t,AR::TimeMode::Clamp,uniformPose),"Uniform sample");resampleError.Compare(sk,reference,uniformPose);quantizationError.Compare(sk,uniformPose,decoded);
        Check(ozzQuality.Sample(ozz,t),"ozz quality sample");ozzQuality.Copy(ozz,decoded);ozzError.Compare(sk,reference,decoded);
        Check(ozzQuality.Evaluate(ozz),"ozz world validation");std::vector<AR::Matrix> expected(joints);Check(AR::Evaluate(sk,reference,expected),"Reference world");
        for(size_t j=0;j<joints;++j)for(int column=0;column<4;++column){float values[4];ozz::math::StorePtrU(ozzQuality.world[j].cols[column],values);for(int row=0;row<4;++row)ozzWorldMatrixError=std::max(ozzWorldMatrixError,std::abs(double(expected[ozz.order[j]][column*4+row])-values[row]));}
    }
    std::cout<<"{\"kind\":\"clip\",\"case\":\""<<label<<"\",\"joints\":"<<joints<<",\"duration\":"<<clip.Duration()<<",\"runtime_clip_bytes\":"<<ClipBytes(clip)<<",\"acl_bytes\":"<<compressedBytes<<",\"ozz_clip_bytes\":"<<ozz.clip->size()<<",\"ozz_skeleton_bytes\":"<<ozz.skeletonBytes<<",\"acl_encoder_peak_requested_bytes\":"<<allocator.peak<<",\"resample_hz\":"<<rate<<",\"resample_us\":"<<resampleUs<<",\"acl_compress_us\":"<<compressUs<<",\"ozz_build_us\":"<<ozzBuildUs<<",\"import_bind_us\":"<<importUs<<",\"max_source_shear\":"<<shear<<",\"step_tracks\":"<<step<<",\"slerp_tracks\":"<<slerp<<",\"quality_samples\":"<<qualityTimes.size();
    aclError.Print("acl_error");ozzError.Print("ozz_error");resampleError.Print("resample_error");quantizationError.Print("acl_quantization_error");std::cout<<",\"ozz_actual_world_matrix_error\":"<<ozzWorldMatrixError<<",\"acl_reordered_tracks\":"<<reordered<<"}\n"<<std::flush;
    if(qualityOnly){allocator.deallocate(compressed,compressedBytes);return;}
    for(int n:{1,20,100}) {
        std::vector<std::unique_ptr<Actor>> actors;std::vector<std::unique_ptr<OzzActor>> ozzActors;const auto ozzBefore=oa.requested;setupStart=Clock::now();
        for(int i=0;i<n;++i){auto a=std::make_unique<Actor>(joints);Check(a->context.initialize(*compressed),"Actor ACL init");Check(AR::Sample(sk,clip,clip.Duration()*.37,AR::TimeMode::Clamp,a->second),"Second pose");actors.push_back(std::move(a));}
        const auto currentSetup=Micros(setupStart);setupStart=Clock::now();
        for(int i=0;i<n;++i){auto a=std::make_unique<OzzActor>(ozz);Check(a->Sample(ozz,clip.Duration()*.37),"Second ozz pose");a->second=a->pose;ozzActors.push_back(std::move(a));}const double ozzSetup=Micros(setupStart);const auto ozzRequested=oa.requested-ozzBefore;
        uint64_t warmAllocations=0;
        auto measured=[&](auto fn){return Bench([&](int k){const auto before=allocations.load()+oa.calls+allocator.calls;for(int i=0;i<n;++i)fn(k,i);warmAllocations+=allocations.load()+oa.calls+allocator.calls-before;},n==100?8:32);};
        auto time=[&](int k,int i){return std::fmod((k*.0166666667+i*.031),clip.Duration());};
        auto sample=measured([&](int k,int i){if(!AR::Sample(sk,clip,time(k,i),AR::TimeMode::Loop,actors[i]->pose))std::abort();});
        auto acl=measured([&](int k,int i){auto& a=*actors[i];a.context.seek(float(time(k,i)),acl::sample_rounding_policy::none);Writer w;w.pose=&a.pose;a.context.decompress_tracks(w);});
        auto oz=measured([&](int k,int i){if(!ozzActors[i]->Sample(ozz,time(k,i)))std::abort();});
        auto blend=measured([&](int,int i){auto& a=*actors[i];if(!AR::Blend(a.pose,a.second,.37f,a.blend))std::abort();});
        auto ozblend=measured([&](int,int i){if(!ozzActors[i]->Blend(ozz))std::abort();});
        auto evaluate=measured([&](int,int i){auto& a=*actors[i];if(!AR::Evaluate(sk,a.pose,a.world))std::abort();});
        auto ozeval=measured([&](int,int i){if(!ozzActors[i]->Evaluate(ozz))std::abort();});
        std::cout<<"{\"kind\":\"warm\",\"case\":\""<<label<<"\",\"actors\":"<<n<<",\"repetitions\":31,\"batch_frames\":"<<(n==100?8:32)<<",\"warm_allocations\":"<<warmAllocations<<",\"current_actor_pose_world_bytes\":"<<n*joints*(3*sizeof(AR::LocalTransform)+sizeof(AR::Matrix))<<",\"acl_context_bytes\":"<<n*sizeof(aclQuality.context)<<",\"ozz_actor_requested_bytes\":"<<ozzRequested<<",\"current_plus_acl_setup_us\":"<<currentSetup<<",\"ozz_actor_setup_us\":"<<ozzSetup;
        PrintStats("current_sample",sample);PrintStats("acl_decode",acl);PrintStats("ozz_sample",oz);PrintStats("current_blend",blend);PrintStats("ozz_blend",ozblend);PrintStats("current_local_to_model",evaluate);PrintStats("ozz_local_to_model",ozeval);std::cout<<"}\n"<<std::flush;
    }
    allocator.deallocate(compressed,compressedBytes);
}
int main(int argc,char** argv) {
#ifdef _WIN32
    SetUnhandledExceptionFilter(AuditException);
#endif
    OzzAllocator oa;oa.base=ozz::memory::default_allocator();ozz::memory::SetDefaulAllocator(&oa);
    try {Check(argc>=3&&argc<=5,"CoreAnimationAudit asset-root manifest.tsv [acl-hz] [quality-only]");if(argc>=4)sampleHz=std::stod(argv[3]);Check(sampleHz>=1&&sampleHz<=1000,"Bounded sample rate");qualityOnly=argc==5;std::ifstream manifest(argv[2]);Check(bool(manifest),"Manifest missing");std::cout<<std::setprecision(10);std::string line;
        while(std::getline(manifest,line)){if(line.empty()||line[0]=='#')continue;std::vector<std::string> fields;size_t begin=0;for(size_t i=0;i<=line.size();++i)if(i==line.size()||line[i]=='\t'){fields.push_back(line.substr(begin,i-begin));begin=i+1;}Check(fields.size()==3,"Manifest: label model motion");auto start=Clock::now();
            const auto modelPath=std::filesystem::path(argv[1])/fields[1];auto modelBytes=Read(modelPath);
            if(modelPath.extension()==".glb") {auto loaded=AssetRuntime::GetGlTFAssetProvider().Load(modelPath.string(),modelBytes);Check(bool(loaded),loaded.diagnostic);const auto* doc=loaded.asset.Get();const auto* sk=doc->RuntimeSkeleton(0);Check(sk!=nullptr,"GLB skeleton");const auto index=std::stoul(fields[2]);const auto* clip=doc->RuntimeClip(index);Check(clip!=nullptr,"GLB clip");Audit(fields[0],*sk,*clip,Micros(start),oa);}
            else {auto contents=GR::Read(GR::File(modelBytes));Check(!contents.modelData.empty(),"Model data");auto animBytes=Read(std::filesystem::path(argv[1])/fields[2]);auto motions=GR::Read(GR::File(animBytes));Check(!motions.animations.empty(),"Motion data");auto sk=contents.modelData[0].skeleton;std::string error;auto clip=GR::BindAnimation(motions.animations[0],motions.animationData[0],*sk,error,3,contents.models[0].name);Check(bool(clip),error);Audit(fields[0],*sk,*clip,Micros(start),oa);}
        }
        const auto live=AR::GetLifetimeCounts();Check(!live.skeletons&&!live.clips&&!AssetRuntime::liveDocuments,"Audit resource lifetime");ozz::memory::SetDefaulAllocator(oa.base);return 0;
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';ozz::memory::SetDefaulAllocator(oa.base);return 1;}
}
