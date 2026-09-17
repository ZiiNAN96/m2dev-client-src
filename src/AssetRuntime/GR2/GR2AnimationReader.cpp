#include "GR2Reader.h"
#include "EterBase/MapLoadTrace.h"
#include "GR2AssetProvider.h"
#include "GR2RuntimeTrace.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace AssetRuntime::GR2
{
namespace
{
Curve ReadCurve(Types& t,Object object,unsigned dimension,Contents& contents)
{
    Require(bool(object),"missing animation curve"); Curve curve; curve.dimension=dimension;
    std::string encoding="legacy-f32";
    if(t.Find(object,"CurveData")) object=t.Child(object,"CurveData");
    if(t.Find(object,"Degree")) curve.degree=static_cast<std::uint32_t>(t.Integer(object,"Degree"));
    else if(t.Find(object,"CurveDataHeader")) {
        auto header=t.Child(object,"CurveDataHeader"); const auto bytes=t.file.Bytes(header.data,2);
        const auto format=std::to_integer<unsigned>(bytes[0]); curve.degree=std::to_integer<unsigned>(bytes[1]);
        encoding="format-"+std::to_string(format);
        if(format!=1) Unsupported("animation curve "+encoding);
    } else Unsupported("animation curve type "+t.Describe(object.type));
    if(curve.degree>2) Unsupported("animation curve degree "+std::to_string(curve.degree));
    curve.knots=t.Reals(object,"Knots"); curve.controls=t.Reals(object,"Controls");
    Require(curve.knots.size()<=65536,"curve knot limit");
    Require(curve.controls.size()==Product(curve.knots.size(),dimension),"curve control dimension/count mismatch");
    Require(std::is_sorted(curve.knots.begin(),curve.knots.end()),"unordered animation knots");
    Require(curve.knots.empty()||curve.knots.front()>=0,"negative animation knot");
    if(curve.knots.empty()) Require(curve.degree==0,"nonconstant empty curve");
    ++contents.curveFormats[encoding+"/degree="+std::to_string(curve.degree)+"/dimension="+std::to_string(dimension)+
        "/"+(curve.knots.empty()?"identity":curve.knots.size()==1?"constant":"spline")];
    return curve;
}
template<std::size_t N> std::array<float,N> SampleCurve(const Curve& curve,float time,double duration,unsigned boundary)
{
    RuntimeTrace::Timer diagnostic(RuntimeTrace::Sampling);
    if(RuntimeTrace::current) ++RuntimeTrace::current->sampleCalls;
    std::array<float,N> result{};
    if(curve.knots.empty()) { if constexpr(N==4) result[3]=1; if constexpr(N==9) result[0]=result[4]=result[8]=1; return result; }
    const auto last=static_cast<std::ptrdiff_t>(curve.knots.size()-1);
    auto index=std::min<std::ptrdiff_t>(std::upper_bound(curve.knots.begin(),curve.knots.end(),time)-curve.knots.begin(),last);
    auto knot=[&](std::ptrdiff_t i) { if((boundary&1) && last>0 && i<0) return curve.knots[static_cast<std::size_t>(i+last)]-static_cast<float>(duration); if((boundary&2) && last>0 && i>last) return curve.knots[static_cast<std::size_t>(i-last)]+static_cast<float>(duration); return curve.knots[static_cast<std::size_t>(std::clamp(i,std::ptrdiff_t(0),last))]; };
    auto control=[&](std::ptrdiff_t i,std::size_t c) { if((boundary&1) && last>0 && i<0) i+=last; if((boundary&2) && last>0 && i>=last) i-=last; return curve.controls[static_cast<std::size_t>(std::clamp(i,std::ptrdiff_t(0),last))*N+c]; };
    auto ratio=[](double a,double b) { return b==0?0.0:a/b; };
    std::array<double,3> coefficients{0,0,1};
    if(curve.degree==1) { coefficients[2]=ratio(time-knot(index-1),knot(index)-knot(index-1)); coefficients[1]=1-coefficients[2]; }
    if(curve.degree==2) {
        const double u=ratio(double(time)-knot(index-1),double(knot(index))-knot(index-1));
        const double a=ratio(double(time)-knot(index-2),double(knot(index))-knot(index-2));
        const double b=ratio(double(time)-knot(index-1),double(knot(index+1))-knot(index-1));
        coefficients[2]=u*b; coefficients[0]=(1-u)*(1-a); coefficients[1]=1-coefficients[0]-coefficients[2];
    }
    for(std::size_t c=0;c<N;++c) { double sum=0; for(unsigned j=0;j<3;++j) sum+=coefficients[j]*control(index-2+j,c); result[c]=static_cast<float>(sum); }
    if constexpr(N==4) {
        double length=0; for(auto x:result) length+=double(x)*x; Require(length>1e-20,"zero animation quaternion");
        for(auto& x:result) x=static_cast<float>(x/std::sqrt(length));
    }
    return result;
}
template<std::size_t N> AnimationRuntime::Track<std::array<float,N>> ConvertCurve(const Curve& curve,double duration,std::size_t& total,unsigned boundary)
{
    AnimationRuntime::Track<std::array<float,N>> result;
    const bool firstUse=MapLoadTrace::FirstUseActive();
    auto* stats=RuntimeTrace::current;
    constexpr unsigned channel=N==3?0:N==4?1:2;
    if(stats) {
        stats->sourceKeys+=curve.knots.size(); stats->sourceByChannel[channel]+=curve.knots.size(); stats->temporaryLive=0;
        stats->identityChannels+=curve.knots.empty(); stats->constantChannels+=curve.knots.size()==1;
        stats->constantByChannel[channel]+=curve.knots.size()<=1;
        bool constant=true;
        for(std::size_t i=N;i<curve.controls.size();++i) constant &= curve.controls[i]==curve.controls[i%N];
        stats->constantControlChannels+=constant;
    }
    auto append=[&](double time,const auto& value) {
        const auto oldCapacity=result.keys.capacity();
        const bool growth=result.keys.size()==oldCapacity;
        RuntimeTrace::Timer diagnostic(growth?RuntimeTrace::Allocation:RuntimeTrace::Store);
        Require(++total<=2000000,"decoded animation key budget");
        if(firstUse && result.keys.size()==result.keys.capacity()) {
            MapLoadTrace::FirstUseScope allocation("keyframe allocations and relocation");
            result.keys.push_back({time,value});
            MapLoadTrace::Count("first-use-key-allocation",{},result.keys.capacity()*sizeof(result.keys.front()));
        } else result.keys.push_back({time,value});
        if(stats) {
            ++stats->runtimeKeys; ++stats->runtimeByChannel[channel];
            if(growth) {
                ++stats->allocations; stats->reallocations+=oldCapacity!=0;
                stats->allocatedBytes+=result.keys.capacity()*sizeof(result.keys.front());
                stats->relocatedBytes+=oldCapacity*sizeof(result.keys.front());
                stats->temporaryPeak=std::max(stats->temporaryPeak,stats->temporaryLive+oldCapacity*sizeof(result.keys.front()));
            }
        }
    };
    if(curve.knots.size()<=1) { append(0,SampleCurve<N>(curve,0,duration,boundary)); return result; }
    // Decode source splines once into the existing runtime's linear channels.
    // Every knot is a subdivision boundary; adaptive midpoint and quarter-point
    // checks bound the approximation without adding a second frame sampler.
    std::vector<double> times;
    {
        RuntimeTrace::Timer timeline(RuntimeTrace::Timeline);
        const auto push=[&](double value) {
            const auto old=times.capacity(); times.push_back(value);
            if(stats && times.capacity()!=old) {
                ++stats->allocations; stats->reallocations+=old!=0;
                stats->allocatedBytes+=times.capacity()*sizeof(double); stats->temporaryAllocated+=times.capacity()*sizeof(double);
                stats->relocatedBytes+=old*sizeof(double);
                stats->temporaryPeak=std::max(stats->temporaryPeak,(old+times.capacity())*sizeof(double));
            }
        };
        push(0); for(auto t:curve.knots) if(t>times.back() && t<duration) push(t); push(duration);
        if(stats) stats->temporaryLive=times.capacity()*sizeof(double);
    }
    auto deviation=[](auto first,auto second,auto sample,float weight) {
        RuntimeTrace::Timer diagnostic(RuntimeTrace::Refinement);
        std::array<float,N> interpolated{};
        if constexpr(N==4) { double dot=0; for(unsigned c=0;c<N;++c) dot+=double(first[c])*second[c]; if(dot<0) for(auto& v:second) v=-v; }
        for(unsigned c=0;c<N;++c) interpolated[c]=first[c]+(second[c]-first[c])*weight;
        if constexpr(N==4) {
            double len=0; for(auto v:interpolated) len+=double(v)*v;
            for(auto& v:interpolated) v=static_cast<float>(v/std::sqrt(len));
            double dot=0; for(unsigned c=0;c<N;++c) dot+=double(interpolated[c])*sample[c];
            if(dot<0) for(auto& v:sample) v=-v;
        }
        double error=0;
        for(unsigned c=0;c<N;++c) {
            const double rounding=2*std::numeric_limits<float>::epsilon()*std::max(std::abs(double(interpolated[c])),std::abs(double(sample[c])));
            error=std::max(error,std::abs(double(interpolated[c])-sample[c])-rounding);
        }
        return error;
    };
    constexpr double tolerance=N==3?1e-5:N==4?5e-7:5e-7;
    struct Probe { float time; std::array<float,N> value; };
    // A split already evaluated its midpoint and both child midpoints. Reuse
    // only at exactly identical float times: dyadic arithmetic may round
    // differently near a source knot. All three error checks remain unchanged.
    auto subdivide=[&](auto&& self,double a,double b,const auto& va,const auto& vb,unsigned depth,const Probe* knownMiddle)->void {
        bool split=false;
        std::array<Probe,3> probes;
        for(unsigned i=0;i<3;++i) {
            const float q=(i+1)*.25f;
            const float sampleTime=static_cast<float>(a+(b-a)*q);
            const float weight=static_cast<float>((double(sampleTime)-a)/(b-a));
            auto& probe=probes[i]; probe.time=sampleTime;
            if(i==1 && knownMiddle && knownMiddle->time==sampleTime) {
                probe.value=knownMiddle->value; if(stats) ++stats->reusedSamples;
            } else probe.value=SampleCurve<N>(curve,sampleTime,duration,boundary);
            if(deviation(va,vb,probe.value,weight)>tolerance) split=true;
        }
        const double middle=static_cast<float>((a+b)*.5);
        if(split && depth<16 && static_cast<float>(a)!=static_cast<float>(middle) && static_cast<float>(b)!=static_cast<float>(middle)) {
            if(stats) { ++stats->splitNodes; ++stats->potentialMidpointReuse; stats->maxDepth=std::max<std::uint64_t>(stats->maxDepth,depth+1); }
            auto vm=probes[1].time==static_cast<float>(middle)?probes[1].value:SampleCurve<N>(curve,static_cast<float>(middle),duration,boundary);
            if(stats && probes[1].time==static_cast<float>(middle)) ++stats->reusedSamples;
            self(self,a,middle,va,vm,depth+1,&probes[0]); self(self,middle,b,vm,vb,depth+1,&probes[2]);
        } else {
            if(stats) ++stats->leafNodes;
            if(split && b-a>=1e-6) Bad("native spline refinement limit dimension="+std::to_string(N)+" a="+std::to_string(a)+" b="+std::to_string(b));
            append(b,vb);
        }
    };
    append(0,SampleCurve<N>(curve,0,duration,boundary));
    for(std::size_t i=1;i<times.size();++i) if(times[i]>times[i-1]) {
        const auto a=SampleCurve<N>(curve,static_cast<float>(times[i-1]),duration,boundary),b=SampleCurve<N>(curve,static_cast<float>(times[i]),duration,boundary);
        subdivide(subdivide,times[i-1],times[i],a,b,0,nullptr);
    }
    if(stats) stats->temporaryLive=0;
    return result;
}
}
AnimationData ReadAnimation(Types& t,Object source,AnimationAsset& metadata,Contents& contents)
{
    MapLoadTrace::Scope p0lScope("Actors","GR2 animation curves","cpu");

    metadata.name=t.Text(source,"Name"); metadata.duration=t.Real(source,"Duration"); metadata.timeStep=t.Real(source,"TimeStep");
    Require(metadata.duration>=0 && metadata.duration<=600 && metadata.timeStep>0,"invalid animation time range");
    AnimationData result;
    const auto groups=t.Array(source,"TrackGroups"); Require(groups.size()<=1024,"track group limit");
    metadata.trackGroupCount=static_cast<std::uint32_t>(groups.size());
    for(auto sourceGroup:groups) {
        TrackGroup group; group.name=t.Text(sourceGroup,"Name");
        group.initialPlacement=ReadTransform(t.file,t.Field(sourceGroup,"InitialPlacement"));
        if(t.Find(sourceGroup,"AccumulationFlags")) group.accumulationFlags=t.Integer(sourceGroup,"AccumulationFlags");
        if(t.Find(sourceGroup,"LoopTranslation")) {
            const auto translation=t.Reals(sourceGroup,"LoopTranslation"); Require(translation.size()==3,"loop translation dimensions");
            std::copy(translation.begin(),translation.end(),group.loopTranslation.begin());
        }
        if(auto loop=t.Child(sourceGroup,"PeriodicLoop")) {
            PeriodicLoop value;value.radius=t.Real(loop,"Radius");value.dAngle=t.Real(loop,"dAngle");value.dZ=t.Real(loop,"dZ");
            auto vector=[&](const char* name,auto& out) {
                const auto data=t.Reals(loop,name);Require(data.size()==3,"periodic loop vector dimension");
                std::copy(data.begin(),data.end(),out.begin());
            };
            vector("BasisX",value.basisX);vector("BasisY",value.basisY);vector("Axis",value.axis);
            group.periodicLoop=value;
        }
        if(t.Child(sourceGroup,"RootMotion")) Unsupported("explicit root motion");
        if(!t.Array(sourceGroup,"ScalarTracks").empty() || !t.Array(sourceGroup,"VectorTracks").empty()) Unsupported("scalar/vector animation tracks");
        auto tracks=t.Array(sourceGroup,"TransformTracks"); Require(tracks.size()<=65536,"animation track limit");
        for(auto track:tracks) {
            TransformTrack value; value.name=t.Text(track,"Name"); Require(!value.name.empty(),"empty track target");
            value.translation=ReadCurve(t,t.Child(track,"PositionCurve"),3,contents);
            value.rotation=ReadCurve(t,t.Child(track,"OrientationCurve"),4,contents);
            value.scale=ReadCurve(t,t.Child(track,"ScaleShearCurve"),9,contents);
            for(auto* curve:{&value.translation,&value.rotation,&value.scale}) Require(curve->knots.empty() || curve->knots.back()<=metadata.duration+1e-4f,"animation knot beyond duration");
            group.tracks.push_back(std::move(value));
        }
        if(group.accumulationFlags&2) Require(std::is_sorted(group.tracks.begin(),group.tracks.end(),
            [](const auto& a,const auto& b){return CompareTrackNames(a.name,b.name)<0;}),"unsorted track group marked sorted");
        if(result.groups.empty()) for(auto textTrack:t.Array(sourceGroup,"TextTracks")) for(auto entry:t.Array(textTrack,"Entries")) {
            // Exporter text annotations can outlive a trimmed clip. They are
            // retained metadata, not sampled curve keys; finite/nonnegative
            // timestamps still apply, and curve duration checks stay strict.
            auto time=t.Real(entry,"TimeStamp"); Require(time>=0,"invalid animation text time");
            metadata.textEvents.push_back({t.Text(entry,"Text"),time});
        }
        result.groups.push_back(std::move(group));
    }
    return result;
}
bool RootMotion::Delta(float elapsed,std::array<float,3>& translation,std::array<float,3>& rotation) const
{
    if(!std::isfinite(elapsed)) return false;
    rotation={};
    if(periodic) {
        // These are per-second helix parameters, not per-animation displacement.
        // The half-angle form avoids cancellation for very large-radius loops.
        const auto& loop=*periodic;const double angle=double(loop.dAngle)*elapsed;
        const double sine=std::sin(angle),half=std::sin(angle*.5);
        for(unsigned i=0;i<3;++i) {
            translation[i]=static_cast<float>(double(loop.radius)*(sine*loop.basisY[i]-2*half*half*loop.basisX[i])+double(loop.dZ)*elapsed*loop.axis[i]);
            rotation[i]=static_cast<float>(angle*loop.axis[i]);
        }
    } else for(unsigned i=0;i<3;++i) translation[i]=velocity[i]*elapsed;
    return std::all_of(translation.begin(),translation.end(),[](float x){return std::isfinite(x);}) &&
        std::all_of(rotation.begin(),rotation.end(),[](float x){return std::isfinite(x);});
}
int CompareTrackNames(std::string_view a,std::string_view b) noexcept
{
    // GR2 sorts encoded names by signed 8-bit values, including the terminating
    // zero. Specify that ordering explicitly on both MSVC and LP64 platforms.
    auto byte=[](std::string_view text,std::size_t at) {
        const int value=at<text.size()?static_cast<unsigned char>(text[at]):0;
        return value<128?value:value-256;
    };
    for(std::size_t i=0;i<=std::min(a.size(),b.size());++i) {
        const auto left=byte(a,i),right=byte(b,i);
        if(left!=right) return left-right;
        if(left==0) return 0;
    }
    return 0;
}
const TransformTrack* FindTransformTrack(const TrackGroup& group,std::string_view name)
{
    // GR2's sorted flag selects midpoint search. Equal names are legal;
    // preserve the source order because the selected duplicate depends on it.
    if(group.accumulationFlags&2) {
        std::size_t first=0,last=group.tracks.size();
        while(first<last) {
            const auto middle=first+(last-first)/2;const auto& track=group.tracks[middle];
            const auto order=CompareTrackNames(name,track.name);
            if(order==0) return &track;
            if(order<0) last=middle;else first=middle+1;
        }
        return nullptr;
    }
    for(const auto& track:group.tracks) if(track.name==name) return &track;
    return nullptr;
}
std::shared_ptr<const AnimationRuntime::RuntimeAnimationClip> BindAnimation(const AnimationAsset& metadata,const AnimationData& data,
    const AnimationRuntime::RuntimeSkeleton& skeleton,std::string& error,unsigned boundary,std::string_view modelName)
{
    try {
        RuntimeTrace::ClipScope diagnostic(metadata.name,skeleton.BindingId(),boundary);
        auto* stats=RuntimeTrace::current;
        if(stats) {
            MapLoadTrace::Count("runtime-duration-ns",stats->id,static_cast<std::uint64_t>(metadata.duration*1e9));
            MapLoadTrace::Count("runtime-source-step-ns",stats->id,static_cast<std::uint64_t>(metadata.timeStep*1e9));
        }
        AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Import);
        AnimationStallAudit::ImportStarted(); ++nativeAnimationDecodes;
        const TrackGroup* selected=nullptr;
        for(const auto& group:data.groups) if(modelName.empty() || group.name==modelName) {
            Require(!selected,"ambiguous animation track group binding"); selected=&group;
        }
        Require(selected!=nullptr,"no matching animation track group");
        const auto& group=*selected;
        if(stats) {
            stats->sourceTracks=group.tracks.size();
            for(const auto& track:group.tracks) stats->sourceAllKeys+=track.translation.knots.size()+track.rotation.knots.size()+track.scale.knots.size();
        }
        std::vector<AnimationRuntime::AnimationTrack> tracks; std::size_t keys=0;
        for(std::size_t bone=0;bone<skeleton.Bones().size();++bone) {
            const auto* selectedTrack=[&]{ RuntimeTrace::Timer diagnostic(RuntimeTrace::Mapping); MapLoadTrace::FirstUseScope trace("track to bone mapping"); AnimationStallAudit::WorkScope mapping(AnimationStallAudit::Work::ClipBind); return FindTransformTrack(group,skeleton.Bones()[bone].name); }();
            if(!selectedTrack) continue;
            const auto& source=*selectedTrack;
            if(stats) {
                ++stats->tracks;
                stats->staticTracks+=source.translation.knots.size()<=1 && source.rotation.knots.size()<=1 && source.scale.knots.size()<=1;
            }
            AnimationStallAudit::WorkScope decode(AnimationStallAudit::Work::AnimationDecode);
            MapLoadTrace::FirstUseScope keyframes("spline to runtime keyframes");
            AnimationRuntime::AnimationTrack track; track.targetBone=static_cast<std::uint32_t>(bone);
            track.translation=ConvertCurve<3>(source.translation,metadata.duration,keys,boundary);
            track.rotation=ConvertCurve<4>(source.rotation,metadata.duration,keys,boundary);
            track.scaleShear=ConvertCurve<9>(source.scale,metadata.duration,keys,boundary);
            keyframes.Stop();
            {
                RuntimeTrace::Timer storage(RuntimeTrace::TrackStorage);
                const auto old=tracks.capacity(); tracks.push_back(std::move(track));
                if(stats && tracks.capacity()!=old) {
                    ++stats->allocations; stats->reallocations+=old!=0;
                    stats->allocatedBytes+=tracks.capacity()*sizeof(track); stats->relocatedBytes+=old*sizeof(track);
                    stats->temporaryPeak=std::max(stats->temporaryPeak,old*sizeof(track));
                }
            }
        }
        Require(!tracks.empty(),"no matching animation tracks");
        auto clip=std::make_shared<AnimationRuntime::RuntimeAnimationClip>();
        RuntimeTrace::Timer validation(RuntimeTrace::Validation);
        MapLoadTrace::FirstUseScope initialization("clip binding and sampler initialization");
        AnimationStallAudit::WorkScope binding(AnimationStallAudit::Work::ClipBind);
        if(!clip->Initialize(metadata.name,metadata.duration,true,std::move(tracks),skeleton,error)) return {};
        if(stats) {
            ++stats->allocations; stats->allocatedBytes+=sizeof(*clip);
            stats->runtimeBytes=sizeof(*clip)+clip->Tracks().capacity()*sizeof(AnimationRuntime::AnimationTrack);
            for(const auto& track:clip->Tracks()) {
                stats->runtimeBytes+=track.translation.keys.capacity()*sizeof(track.translation.keys.front());
                stats->runtimeBytes+=track.rotation.keys.capacity()*sizeof(track.rotation.keys.front());
                stats->runtimeBytes+=track.scaleShear.keys.capacity()*sizeof(track.scaleShear.keys.front());
            }
        }
        if(MapLoadTrace::FirstUseActive()) MapLoadTrace::Count("first-use-runtime-keys",metadata.name,keys);
        return clip;
    } catch(const std::exception& failure) { error=failure.what(); return {}; }
}
}



