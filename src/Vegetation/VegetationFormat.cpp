#include "VegetationRuntime.h"
#include "EterBase/MapLoadTrace.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Vegetation {
namespace {
using Value=rapidjson::Value;using Writer=rapidjson::Writer<rapidjson::StringBuffer>;
void Require(bool ok,const char* error){if(!ok)throw std::runtime_error(error);}
const Value& Field(const Value& v,const char* key){Require(v.IsObject()&&v.HasMember(key),"missing ZVEG field");return v[key];}
float Number(const Value& v){Require(v.IsNumber()&&std::isfinite(v.GetDouble())&&std::abs(v.GetDouble())<1e9,"invalid ZVEG number");return v.GetFloat();}
std::uint32_t Unsigned(const Value& v){Require(v.IsUint(),"invalid ZVEG integer");return v.GetUint();}
std::string String(const Value& v){Require(v.IsString()&&v.GetStringLength()<=1024,"invalid ZVEG string");std::string s(v.GetString(),v.GetStringLength());Require(s.find('\0')==s.npos,"embedded nul in path");return s;}
template<std::size_t N> std::array<float,N> Array(const Value& v){Require(v.IsArray()&&v.Size()==N,"invalid ZVEG array");std::array<float,N> out{};for(unsigned i=0;i<N;++i)out[i]=Number(v[i]);return out;}
Bounds ReadBounds(const Value& v){const auto values=Array<6>(v);Bounds b;std::copy_n(values.begin(),3,b.min.begin());std::copy_n(values.begin()+3,3,b.max.begin());b.valid=true;Require(ValidBounds(b),"invalid vegetation bounds");return b;}
void Unique(const Value& v,unsigned depth=0){
    Require(depth<=16,"ZVEG nesting limit");
    if(v.IsObject()){std::set<std::string> names;for(auto i=v.MemberBegin();i!=v.MemberEnd();++i){Require(names.emplace(i->name.GetString(),i->name.GetStringLength()).second,"duplicate JSON key");Unique(i->value,depth+1);}}
    else if(v.IsArray())for(const auto& x:v.GetArray())Unique(x,depth+1);
}
void Parse(std::string_view text,rapidjson::Document& d){
    Require(!text.empty()&&text.size()<=2*1024*1024,"vegetation JSON size limit");
    // Bound nesting before the DOM parser allocates or recurses.
    unsigned depth=0;bool quote=false,escape=false;
    for(char c:text){if(quote){if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quote=false;}else if(c=='"')quote=true;else if(c=='{'||c=='[')Require(++depth<=16,"vegetation JSON nesting limit");else if(c=='}'||c==']'){Require(depth>0,"invalid JSON nesting");--depth;}}
    d.Parse<rapidjson::kParseValidateEncodingFlag>(text.data(),text.size());Require(!d.HasParseError()&&d.IsObject(),"invalid vegetation JSON");Unique(d);
}
template<std::size_t N>void WriteArray(Writer&w,const std::array<float,N>&a){w.StartArray();for(float f:a)w.Double(f);w.EndArray();}
void WriteBounds(Writer&w,const Bounds& b){w.StartArray();for(float f:b.min)w.Double(f);for(float f:b.max)w.Double(f);w.EndArray();}
void WriteString(Writer&w,const char*k,const std::string&s){w.Key(k);w.String(s.data(),static_cast<unsigned>(s.size()));}
}
Result ParseMetadata(std::string_view text,Metadata& result){
    try{
        rapidjson::Document d;Parse(text,d);Metadata m;
        m.version=Unsigned(Field(d,"version"));Require(m.version==1||m.version==2,"unsupported ZVEG version");
        m.geometry=String(Field(d,"geometry"));Require(ValidCompiledPath(m.geometry,".glb"),"invalid compiled geometry path");
        m.shadowTexture=String(Field(d,"shadowTexture"));
        Require(m.shadowTexture.empty()||(m.shadowTexture.starts_with("d:/ymir work/")&&m.shadowTexture.ends_with(".dds")&&m.shadowTexture.find("..") == m.shadowTexture.npos&&m.shadowTexture.find('\\') == m.shadowTexture.npos&&std::none_of(m.shadowTexture.begin(),m.shadowTexture.end(),[](unsigned char c){return c<32||c=='%';})),"invalid shadow texture path");
        m.bounds=ReadBounds(Field(d,"bounds"));m.renderBounds=ReadBounds(Field(d,"renderBounds"));
        for(unsigned k=0;k<3;++k)Require(m.renderBounds.min[k]<=m.bounds.min[k]&&m.renderBounds.max[k]>=m.bounds.max[k],"render bounds do not enclose reference bounds");
        const auto limits=Array<3>(Field(d,"lodLimits"));m.nearDistance=limits[0];m.farDistance=limits[1];m.cullDistance=limits[2];
        Require(m.nearDistance>=0&&m.farDistance>m.nearDistance&&m.cullDistance>=m.farDistance&&m.cullDistance<=1e7f,"invalid LOD distances");
        const auto wind=Array<8>(Field(d,"wind"));m.wind.direction={wind[0],wind[1],wind[2]};m.wind.strength=wind[3];m.wind.branchAmplitude=wind[4];m.wind.frondAmplitude=wind[5];m.wind.leafAmplitude=wind[6];m.wind.frequency=wind[7];
        Require(m.wind.strength>=0&&m.wind.strength<=1&&m.wind.branchAmplitude>=0&&m.wind.branchAmplitude<=1&&m.wind.frondAmplitude>=0&&m.wind.frondAmplitude<=1&&m.wind.leafAmplitude>=0&&m.wind.leafAmplitude<=1&&m.wind.frequency>=0&&m.wind.frequency<=100,"invalid vegetation wind");
        const auto& parts=Field(d,"parts");Require(parts.IsArray()&&parts.Size()>0&&parts.Size()<=256,"invalid part count");
        std::set<std::pair<unsigned,unsigned>> partKeys;
        for(const auto& p:parts.GetArray()){Require(p.IsArray()&&p.Size()==3,"invalid part");Part part{static_cast<PartKind>(Unsigned(p[0])),Unsigned(p[1]),Unsigned(p[2])};Require(unsigned(part.kind)<=3&&part.lod<=64&&part.mesh==m.parts.size(),"invalid part metadata");Require(part.kind!=PartKind::Billboard||part.lod==0,"unsupported billboard layout");Require(partKeys.emplace(unsigned(part.kind),part.lod).second,"duplicate part LOD");m.parts.push_back(part);}
        const auto& lods=Field(d,"lods");Require(lods.IsArray()&&lods.Size()>=2&&lods.Size()<=2049,"invalid LOD table");
        for(const auto& row:lods.GetArray()){
            Require(row.IsArray()&&row.Size()==10,"invalid LOD state");LodState state;
            for(unsigned k=0;k<5;++k){Require(row[k].IsInt(),"invalid LOD mesh index");state.meshes[k]=row[k].GetInt();Require(state.meshes[k]>=-1&&state.meshes[k]<int(parts.Size()),"LOD mesh out of range");state.alpha[k]=Number(row[k+5]);Require(state.alpha[k]>=0&&state.alpha[k]<=255,"invalid LOD alpha");}
            for(unsigned k=0;k<5;++k)if(state.meshes[k]>=0){const auto expected=k<2?k:k<4?2:3;Require(unsigned(m.parts[state.meshes[k]].kind)==expected,"LOD slot uses wrong part kind");}
            m.lods.push_back(state);
        }
        const auto& collisions=Field(d,"collisions");Require(collisions.IsArray()&&collisions.Size()<=256,"invalid collision count");
        for(const auto& row:collisions.GetArray()){Require(row.IsArray()&&row.Size()==7,"invalid collision");Collision c;c.kind=Unsigned(row[0]);Require(c.kind<=2,"invalid collision type");for(unsigned k=0;k<3;++k){c.position[k]=Number(row[k+1]);c.dimensions[k]=Number(row[k+4]);Require(c.dimensions[k]>=0,"negative collision dimension");}m.collisions.push_back(c);}
        if(m.version==2) {
            const auto& modern=Field(d,"modern");
            m.plantKind=static_cast<PlantKind>(Unsigned(Field(modern,"plantKind")));
            Require(unsigned(m.plantKind)<=2,"invalid plant kind");
            m.lodDistances=Array<3>(Field(modern,"lodDistances"));
            m.transitionFraction=Number(Field(modern,"transitionFraction"));
            Require(m.lods.size()==4&&m.transitionFraction>=0&&m.transitionFraction<=.25f,"invalid modern LOD transitions");
            float previous=0;
            for(float distance:m.lodDistances) {
                Require(distance*(1-m.transitionFraction*.5f)>previous&&distance*(1+m.transitionFraction*.5f)<m.cullDistance,"overlapping or invalid modern LOD range");
                previous=distance*(1+m.transitionFraction*.5f);
            }
            m.foliage.transmissionColor=Array<3>(Field(modern,"transmissionColor"));
            m.foliage.transmissionStrength=Number(Field(modern,"transmissionStrength"));
            for(float color:m.foliage.transmissionColor)Require(color>=0&&color<=1,"invalid transmission color");
            Require(m.foliage.transmissionStrength>=0&&m.foliage.transmissionStrength<=.5f,"invalid transmission strength");
            const float windMargin=(m.bounds.max[2]-m.bounds.min[2])*(m.wind.branchAmplitude+m.wind.leafAmplitude)*m.wind.strength;
            for(unsigned k=0;k<2;++k)Require(m.renderBounds.min[k]<=m.bounds.min[k]-windMargin&&m.renderBounds.max[k]>=m.bounds.max[k]+windMargin,"modern bounds exclude wind displacement");
        }
        result=std::move(m);return {true,{}};
    }catch(const std::exception&e){return {false,e.what()};}
}
std::string SerializeMetadata(const Metadata&m){
    rapidjson::StringBuffer b;Writer w(b);w.StartObject();w.Key("version");w.Uint(m.version);
    WriteString(w,"geometry",m.geometry);WriteString(w,"shadowTexture",m.shadowTexture);
    w.Key("bounds");WriteBounds(w,m.bounds);w.Key("renderBounds");WriteBounds(w,m.renderBounds);
    w.Key("lodLimits");WriteArray(w,std::array<float,3>{m.nearDistance,m.farDistance,m.cullDistance});
    w.Key("wind");WriteArray(w,std::array<float,8>{m.wind.direction[0],m.wind.direction[1],m.wind.direction[2],m.wind.strength,m.wind.branchAmplitude,m.wind.frondAmplitude,m.wind.leafAmplitude,m.wind.frequency});
    w.Key("parts");w.StartArray();for(const auto&p:m.parts){w.StartArray();w.Uint(unsigned(p.kind));w.Uint(p.lod);w.Uint(p.mesh);w.EndArray();}w.EndArray();
    w.Key("lods");w.StartArray();for(const auto&s:m.lods){w.StartArray();for(auto i:s.meshes)w.Int(i);for(auto f:s.alpha)w.Double(f);w.EndArray();}w.EndArray();
    w.Key("collisions");w.StartArray();for(const auto&c:m.collisions){w.StartArray();w.Uint(c.kind);for(float f:c.position)w.Double(f);for(float f:c.dimensions)w.Double(f);w.EndArray();}w.EndArray();
    if(m.version==2){w.Key("modern");w.StartObject();w.Key("plantKind");w.Uint(unsigned(m.plantKind));w.Key("lodDistances");WriteArray(w,m.lodDistances);w.Key("transitionFraction");w.Double(m.transitionFraction);w.Key("transmissionColor");WriteArray(w,m.foliage.transmissionColor);w.Key("transmissionStrength");w.Double(m.foliage.transmissionStrength);w.EndObject();}
    w.EndObject();
    Metadata checked;std::string out=b.GetString();const auto result=ParseMetadata(out,checked);if(!result)throw std::invalid_argument(result.error);return out+"\n";
}
Result Registry::Add(std::string_view legacy,std::string_view compiled){
    const auto key=NormalizeKey(legacy),path=NormalizeKey(compiled);
    if(key.empty()||key.size()>1024||!key.ends_with(".spt")||!ValidCompiledPath(path,".zveg"))return {false,"invalid vegetation registry entry"};
    if(!entries_.emplace(key,path).second)return {false,"duplicate vegetation registry key"};return {true,{}};
}
Result Registry::Parse(std::string_view text){
    MapLoadTrace::Scope p0lScope("Vegetation","registry parse","cpu");
    MapLoadTrace::Count("registry-parse","vegetation");

    try{
        rapidjson::Document d;Vegetation::Parse(text,d);Require(Unsigned(Field(d,"version"))==1,"unsupported vegetation registry version");
        const auto& entries=Field(d,"entries");Require(entries.IsObject()&&entries.MemberCount()<=32768,"invalid registry entries");Registry candidate;
        for(auto i=entries.MemberBegin();i!=entries.MemberEnd();++i){const auto r=candidate.Add(String(i->name),String(i->value));Require(r.ok,r.error.c_str());}
        if(d.HasMember("modernOverrides")) {
            const auto& overrides=d["modernOverrides"];Require(overrides.IsObject()&&overrides.MemberCount()<=entries.MemberCount(),"invalid modern overrides");
            for(auto i=overrides.MemberBegin();i!=overrides.MemberEnd();++i){const auto r=candidate.AddOverride(String(i->name),String(i->value));Require(r.ok,r.error.c_str());}
        }
        *this=std::move(candidate);return {true,{}};
    }catch(const std::exception&e){return {false,e.what()};}
}
const std::string* Registry::Resolve(std::string_view key)const{const auto it=entries_.find(NormalizeKey(key));return it==entries_.end()?nullptr:&it->second;}
Result Registry::AddOverride(std::string_view legacy,std::string_view compiled){
    const auto key=NormalizeKey(legacy),path=NormalizeKey(compiled);
    if(!entries_.contains(key)||!ValidCompiledPath(path,".zveg"))return {false,"modern override requires legacy fallback and compiled path"};
    if(!overrides_.emplace(key,path).second)return {false,"duplicate modern override"};return {true,{}};
}
const std::string* Registry::ResolveOverride(std::string_view key)const{const auto it=overrides_.find(NormalizeKey(key));return it==overrides_.end()?nullptr:&it->second;}
std::string Registry::Serialize()const{
    rapidjson::StringBuffer b;Writer w(b);w.StartObject();w.Key("version");w.Uint(1);w.Key("entries");w.StartObject();for(const auto&[key,path]:entries_){w.Key(key.c_str());w.String(path.c_str());}w.EndObject();
    if(!overrides_.empty()){w.Key("modernOverrides");w.StartObject();for(const auto&[key,path]:overrides_){w.Key(key.c_str());w.String(path.c_str());}w.EndObject();}
    w.EndObject();return std::string(b.GetString())+"\n";
}
}
