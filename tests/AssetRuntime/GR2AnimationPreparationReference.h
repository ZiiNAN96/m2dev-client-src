#pragma once
// Frozen pre-P0-L8 adaptive conversion oracle. Test only: never compiled into
// the client. Keep independent arithmetic and control flow for bitwise parity.
#include "AssetRuntime/GR2/GR2Reader.h"
#include "AssetRuntime/GR2/GR2AssetProvider.h"
#include "AssetRuntime/AnimationStallAudit.h"
#include "EterBase/MapLoadTrace.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace AssetRuntime::GR2::PreparationReference {
template<std::size_t N> std::array<float,N> SampleCurve(const Curve& curve,float time,double duration,unsigned boundary)
{
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
    auto append=[&](double time,const auto& value) {
        Require(++total<=2000000,"decoded animation key budget");
        if(firstUse && result.keys.size()==result.keys.capacity()) {
            MapLoadTrace::FirstUseScope allocation("keyframe allocations and relocation");
            result.keys.push_back({time,value});
            MapLoadTrace::Count("first-use-key-allocation",{},result.keys.capacity()*sizeof(result.keys.front()));
        } else result.keys.push_back({time,value});
    };
    if(curve.knots.size()<=1) { append(0,SampleCurve<N>(curve,0,duration,boundary)); return result; }
    // Decode source splines once into the existing runtime's linear channels.
    // Every knot is a subdivision boundary; adaptive midpoint and quarter-point
    // checks bound the approximation without adding a second frame sampler.
    std::vector<double> times{0};
    for(auto t:curve.knots) if(t>times.back() && t<duration) times.push_back(t);
    times.push_back(duration);
    auto deviation=[](auto first,auto second,auto sample,float weight) {
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
    auto subdivide=[&](auto&& self,double a,double b,const auto& va,const auto& vb,unsigned depth)->void {
        bool split=false;
        for(float q:{.25f,.5f,.75f}) {
            const float sampleTime=static_cast<float>(a+(b-a)*q);
            const float weight=static_cast<float>((double(sampleTime)-a)/(b-a));
            if(deviation(va,vb,SampleCurve<N>(curve,sampleTime,duration,boundary),weight)>tolerance) split=true;
        }
        const double middle=static_cast<float>((a+b)*.5);
        if(split && depth<16 && static_cast<float>(a)!=static_cast<float>(middle) && static_cast<float>(b)!=static_cast<float>(middle)) {
            auto vm=SampleCurve<N>(curve,static_cast<float>(middle),duration,boundary); self(self,a,middle,va,vm,depth+1); self(self,middle,b,vm,vb,depth+1);
        } else {
            if(split && b-a>=1e-6) Bad("native spline refinement limit dimension="+std::to_string(N)+" a="+std::to_string(a)+" b="+std::to_string(b));
            append(b,vb);
        }
    };
    append(0,SampleCurve<N>(curve,0,duration,boundary));
    for(std::size_t i=1;i<times.size();++i) if(times[i]>times[i-1]) {
        const auto a=SampleCurve<N>(curve,static_cast<float>(times[i-1]),duration,boundary),b=SampleCurve<N>(curve,static_cast<float>(times[i]),duration,boundary);
        subdivide(subdivide,times[i-1],times[i],a,b,0);
    }
    return result;
}
std::shared_ptr<const AnimationRuntime::RuntimeAnimationClip> BindAnimation(const AnimationAsset& metadata,const AnimationData& data,
    const AnimationRuntime::RuntimeSkeleton& skeleton,std::string& error,unsigned boundary,std::string_view modelName)
{
    try {
        AnimationStallAudit::WorkScope audit(AnimationStallAudit::Work::Import);
        AnimationStallAudit::ImportStarted(); ++nativeAnimationDecodes;
        const TrackGroup* selected=nullptr;
        for(const auto& group:data.groups) if(modelName.empty() || group.name==modelName) {
            Require(!selected,"ambiguous animation track group binding"); selected=&group;
        }
        Require(selected!=nullptr,"no matching animation track group");
        const auto& group=*selected;
        std::vector<AnimationRuntime::AnimationTrack> tracks; std::size_t keys=0;
        for(std::size_t bone=0;bone<skeleton.Bones().size();++bone) {
            const auto* selectedTrack=[&]{ MapLoadTrace::FirstUseScope trace("track to bone mapping"); AnimationStallAudit::WorkScope mapping(AnimationStallAudit::Work::ClipBind); return FindTransformTrack(group,skeleton.Bones()[bone].name); }();
            if(!selectedTrack) continue;
            const auto& source=*selectedTrack;
            AnimationStallAudit::WorkScope decode(AnimationStallAudit::Work::AnimationDecode);
            MapLoadTrace::FirstUseScope keyframes("spline to runtime keyframes");
            AnimationRuntime::AnimationTrack track; track.targetBone=static_cast<std::uint32_t>(bone);
            track.translation=ConvertCurve<3>(source.translation,metadata.duration,keys,boundary);
            track.rotation=ConvertCurve<4>(source.rotation,metadata.duration,keys,boundary);
            track.scaleShear=ConvertCurve<9>(source.scale,metadata.duration,keys,boundary);
            keyframes.Stop();
            tracks.push_back(std::move(track));
        }
        Require(!tracks.empty(),"no matching animation tracks");
        auto clip=std::make_shared<AnimationRuntime::RuntimeAnimationClip>();
        MapLoadTrace::FirstUseScope initialization("clip binding and sampler initialization");
        AnimationStallAudit::WorkScope binding(AnimationStallAudit::Work::ClipBind);
        if(!clip->Initialize(metadata.name,metadata.duration,true,std::move(tracks),skeleton,error)) return {};
        if(MapLoadTrace::FirstUseActive()) MapLoadTrace::Count("first-use-runtime-keys",metadata.name,keys);
        return clip;
    } catch(const std::exception& failure) { error=failure.what(); return {}; }
}
}
