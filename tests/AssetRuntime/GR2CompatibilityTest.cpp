#include "GR2Fixtures.h"
#include "AssetRuntime/GR2/GR2Reader.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <cmath>

using namespace AssetRuntime::GR2;
namespace AR=AnimationRuntime;
using GR2Fixtures::Builder;
static void Check(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
static std::uint32_t TypeWith(Builder& b,const std::string& field)
{ for(const auto& [type,fields]:b.fields) if(fields.contains(field)) return type;throw std::runtime_error("fixture type"); }
static void Copy(Builder& b,std::uint32_t source,std::uint32_t destination,std::uint32_t size)
{
    const auto fixups=b.fixups;
    std::copy_n(b.data.begin()+source,size,b.data.begin()+destination);
    for(auto [at,target]:fixups) if(at>=source&&at<source+size) b.Pointer(destination+at-source,target);
}
static std::uint32_t Group(Builder& b)
{ const auto animation=b.fixups.at(b.fixups.at(b.root+12));return b.fixups.at(b.fixups.at(animation+16)); }
static Contents Read(Builder& b) { const File file(b.Bytes());return AssetRuntime::GR2::Read(file); }
static void Rejected(Builder& b,const char* message)
{
    bool rejected=false;try { Read(b); } catch(const Error& error) { rejected=error.kind==Failure::Malformed; }
    Check(rejected,message);
}
static void ConstantPosition(Builder& b,std::uint32_t track,float x)
{
    const auto knot=b.Allocate(4),controls=b.Allocate(12);b.Float(controls,x);
    b.Array(track+4,4,1,knot);b.Array(track+4,12,3,controls);
}
int main()
{
    try {
        Builder forest;const auto bones=forest.Allocate(3*140);
        for(unsigned i=0;i<3;++i) Copy(forest,forest.bone,bones+i*140,140);
        forest.Value(bones+140+4,0);forest.Array(forest.skeleton,4,3,bones);
        auto document=Read(forest);const auto& skeleton=*document.modelData[0].skeleton;
        Check(skeleton.Bones().size()==3&&skeleton.Bones()[1].parent==0&&skeleton.Bones()[2].parent==-1,"indexed forest preserves bone order");
        AR::RuntimeSkeleton strict;std::string error;
        Check(!strict.Initialize(skeleton.Bones(),error),"default importer remains strict");
        AR::AnimationPose pose;pose.Prepare(3);std::vector<AR::Matrix> world(3);
        for(unsigned i=0;i<3;++i) pose.localTransforms[i]=skeleton.Bones()[i].localBind;
        Check(AR::Evaluate(skeleton,pose,world),"forest evaluation");
        forest.Value(bones+4,1);Rejected(forest,"forest cycle rejected");
        forest.Value(bones+4,UINT32_MAX);forest.Value(bones+140+4,3);Rejected(forest,"forest parent bound rejected");

        Builder duplicate;const auto tracks=duplicate.Allocate(128);
        Copy(duplicate,duplicate.track,tracks,64);Copy(duplicate,duplicate.track,tracks+64,64);
        ConstantPosition(duplicate,tracks,1);ConstantPosition(duplicate,tracks+64,7);
        duplicate.Array(Group(duplicate),4,2,tracks);duplicate.Value(Group(duplicate)+80,2);
        auto animation=Read(duplicate);
        auto clip=BindAnimation(animation.animations[0],animation.animationData[0],skeleton,error,0,"root");
        Check(bool(clip)&&clip->Tracks().size()==3,"duplicate names bind all original bone indices");
        Check(AR::Sample(skeleton,*clip,0,AR::TimeMode::Clamp,pose),"duplicate track sample");
        for(const auto& local:pose.localTransforms) Check(local.translation[0]==7,"sorted midpoint duplicate selection");
        duplicate.Value(Group(duplicate)+80,0);animation=Read(duplicate);
        Check(FindTransformTrack(animation.animationData[0].groups[0],"root")->translation.controls[0]==1,"unsorted first duplicate selection");
        duplicate.Name(tracks+64,"");Rejected(duplicate,"empty track remains invalid");
        Builder encoded;
        const auto encodedTracks=encoded.Allocate(128);Copy(encoded,encoded.track,encodedTracks,64);Copy(encoded,encoded.track,encodedTracks+64,64);
        encoded.Name(encodedTracks,std::string(1,static_cast<char>(200)));encoded.Array(Group(encoded),4,2,encodedTracks);encoded.Value(Group(encoded)+80,2);
        auto encodedDocument=Read(encoded);
        Check(CompareTrackNames(std::string(1,static_cast<char>(200)),"root")<0&&FindTransformTrack(encodedDocument.animationData[0].groups[0],"root"),"signed encoded-name order is portable");

        Builder empty;const auto mesh=empty.fixups.at(empty.fixups.at(empty.model+80));
        const auto vertexData=empty.fixups.at(mesh+4),topology=empty.fixups.at(mesh+8);
        empty.Value(vertexData+4,0);empty.Value(topology,0);empty.Value(topology+8,0);
        auto blank=Read(empty);Check(blank.models[0].meshes[0].vertexCount==0&&blank.modelData[0].meshes[0].indices.empty(),"empty geometry supported");
        empty.Value(topology+8,3);Rejected(empty,"indices without vertices rejected");

        Builder periodic;
        const auto loopType=periodic.Type({{10,"Radius"},{10,"dAngle"},{10,"dZ"},{10,"BasisX",0,3},{10,"BasisY",0,3},{10,"Axis",0,3}});
        const auto groupType=periodic.Type({{8,"Name"},{3,"TransformTracks",TypeWith(periodic,"PositionCurve")},{9,"InitialPlacement"},{19,"AccumulationFlags"},{2,"PeriodicLoop",loopType}});
        const auto group=periodic.Object(groupType),oldGroup=Group(periodic),loop=periodic.Object(loopType);
        Copy(periodic,oldGroup,group,84);periodic.Pointer(group+84,loop);
        periodic.Pointer(TypeWith(periodic,"TrackGroups")+3*32+8,groupType);
        const auto animationObject=periodic.fixups.at(periodic.fixups.at(periodic.root+12));
        periodic.Pointer(periodic.fixups.at(animationObject+16),group);
        periodic.Float(loop,20);periodic.Float(loop+4,.2f);periodic.Float(loop+8,1);
        periodic.Float(loop+12,1);periodic.Float(loop+28,1);periodic.Float(loop+44,1);
        auto parsed=Read(periodic);RootMotion motion;motion.periodic=parsed.animationData[0].groups[0].periodicLoop;
        std::array<float,3> translation,rotation;Check(motion.Delta(.5f,translation,rotation),"periodic delta finite");
        Check(std::abs(translation[0]-20*(std::cos(.1)-1))<1e-6&&std::abs(translation[1]-20*std::sin(.1))<1e-6&&translation[2]==.5f&&std::abs(rotation[2]-.1f)<1e-7,"periodic parameters use elapsed seconds");
        periodic.Float(loop,std::numeric_limits<float>::quiet_NaN());Rejected(periodic,"nonfinite periodic data rejected");
        Builder text;
        const auto entryType=text.Type({{10,"TimeStamp"},{8,"Text"}}),textType=text.Type({{8,"Name"},{3,"Entries",entryType}});
        const auto textGroupType=text.Type({{8,"Name"},{3,"TransformTracks",TypeWith(text,"PositionCurve")},{9,"InitialPlacement"},{19,"AccumulationFlags"},{3,"TextTracks",textType}});
        const auto textGroup=text.Object(textGroupType),textTrack=text.Object(textType),entry=text.Object(entryType);
        Copy(text,Group(text),textGroup,84);text.Array(textGroup,84,1,textTrack);text.Array(textTrack,4,1,entry);
        text.Float(entry,4.6666665f);text.Pointer(entry+4,text.String("end"));
        text.Pointer(TypeWith(text,"TrackGroups")+3*32+8,textGroupType);
        const auto textAnimation=text.fixups.at(text.fixups.at(text.root+12));text.Pointer(text.fixups.at(textAnimation+16),textGroup);
        Check(Read(text).animations[0].textEvents[0].time==4.6666665f,"annotation beyond trimmed duration retained");
        text.Float(entry,-1);Rejected(text,"negative annotation rejected");
        text.Float(entry,std::numeric_limits<float>::infinity());Rejected(text,"nonfinite annotation rejected");

        Builder precision;const auto knots=precision.Allocate(16),controls=precision.Allocate(48);
        for(unsigned i=0;i<4;++i) {precision.Float(knots+i*4,std::array<float,4>{0,3,3.1f,4}[i]);precision.Float(controls+i*12,i<2?0.f:100.f);}
        precision.Value(precision.curve,1);precision.Array(precision.curve,4,4,knots);precision.Array(precision.curve,12,12,controls);
        precision.Float(precision.fixups.at(precision.fixups.at(precision.root+12))+4,4);
        auto precise=Read(precision);auto preciseClip=BindAnimation(precise.animations[0],precise.animationData[0],*precise.modelData[0].skeleton,error,0,"root");
        Check(bool(preciseClip),"float-clock refinement does not exhaust depth on a linear segment");
        Builder corrupt;ConstantPosition(corrupt,corrupt.track,std::numeric_limits<float>::quiet_NaN());Rejected(corrupt,"nonfinite curve remains rejected");
        std::cout<<"PASS indexed forest, duplicate binding, zero geometry, periodic metadata, trimmed annotations, float clocks and negative controls\n";
        return 0;
    } catch(const std::exception& error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
