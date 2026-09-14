#pragma once
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_set>

namespace AssetRuntime::GlTFDetail
{
// cgltf intentionally accepts some malformed primitives as defaults. Validate JSON
// syntax and integral/enum fields before handing the document to its structure parser.
class JsonValidation
{
public:
    explicit JsonValidation(std::string_view input):input_(input){}
    bool Run()
    {
        const bool result=Value({},0,true);
        Space();return result && offset_==input_.size();
    }
private:
    void Space() { while(offset_<input_.size() && (input_[offset_]==' ' || input_[offset_]=='\t' || input_[offset_]=='\r' || input_[offset_]=='\n')) ++offset_; }
    bool Take(char c) { Space();if(offset_<input_.size() && input_[offset_]==c) { ++offset_;return true; }return false; }
    bool String(std::string& output)
    {
        if(!Take('"')) return false;
        while(offset_<input_.size()) {
            unsigned char c=static_cast<unsigned char>(input_[offset_++]);
            if(c=='"') return true;
            if(c<32) return false;
            if(c=='\\') {
                if(offset_==input_.size()) return false;
                c=static_cast<unsigned char>(input_[offset_++]);
                if(c=='u') {
                    unsigned value=0;
                    for(unsigned digit=0;digit<4;++digit) {
                        if(offset_==input_.size()) return false;
                        const auto hex=input_[offset_++];
                        const int number=hex>='0' && hex<='9' ? hex-'0' : hex>='A' && hex<='F' ? hex-'A'+10 : hex>='a' && hex<='f' ? hex-'a'+10 : -1;
                        if(number<0) return false;
                        value=value*16+unsigned(number);
                    }
                    if(value<128) c=static_cast<unsigned char>(value);
                    else { output+='\\';output+=std::to_string(value);continue; }
                } else if(c=='b') c='\b';else if(c=='f') c='\f';else if(c=='n') c='\n';else if(c=='r') c='\r';else if(c=='t') c='\t';
                else if(c!='"' && c!='\\' && c!='/') return false;
            }
            output+=static_cast<char>(c);
        }
        return false;
    }
    static bool IntegerKey(std::string_view key)
    {
        for(auto name:{"byteOffset","byteLength","byteStride","componentType","count","buffer","bufferView","mode","indices","material","mesh","skin","scene",
            "sampler","source","texCoord","index","skeleton","inverseBindMatrices","input","output","node","children","nodes","joints","camera","light","variants","target","magFilter","minFilter","wrapS","wrapT","POSITION","NORMAL","TANGENT","TEXCOORD_0","TEXCOORD_1","JOINTS_0","WEIGHTS_0","COLOR_0"})
            if(key==name) return true;
        return false;
    }
    static bool BooleanKey(std::string_view key) { return key=="normalized" || key=="doubleSided"; }
    static bool ScalarKey(std::string_view key)
    {
        return BooleanKey(key) || key=="alphaMode" || key=="interpolation" || key=="alphaCutoff" || key=="metallicFactor" || key=="roughnessFactor" ||
            (IntegerKey(key) && key!="nodes" && key!="children" && key!="joints" && key!="indices" && key!="variants" && key!="target");
    }
    static bool FloatKey(std::string_view key)
    {
        for(auto name:{"translation","rotation","scale","matrix","baseColorFactor","alphaCutoff","min","max","offset","weights","metallicFactor","roughnessFactor"})
            if(key==name) return true;
        return false;
    }
    bool Number(std::string_view key,bool semantic)
    {
        const auto start=offset_;
        if(input_[offset_]=='-') ++offset_;
        if(offset_==input_.size()) return false;
        if(input_[offset_]=='0') ++offset_;
        else {
            if(input_[offset_]<'1' || input_[offset_]>'9') return false;
            while(offset_<input_.size() && input_[offset_]>='0' && input_[offset_]<='9') ++offset_;
        }
        bool integral=true;
        if(offset_<input_.size() && input_[offset_]=='.') {
            integral=false;++offset_;
            const auto first=offset_;
            while(offset_<input_.size() && input_[offset_]>='0' && input_[offset_]<='9') ++offset_;
            if(first==offset_) return false;
        }
        if(offset_<input_.size() && (input_[offset_]=='e' || input_[offset_]=='E')) {
            integral=false;++offset_;
            if(offset_<input_.size() && (input_[offset_]=='-' || input_[offset_]=='+')) ++offset_;
            const auto first=offset_;
            while(offset_<input_.size() && input_[offset_]>='0' && input_[offset_]<='9') ++offset_;
            if(first==offset_) return false;
        }
        if(semantic && IntegerKey(key)) {
            if(!integral || input_[start]=='-') return false;
            std::uint32_t number{};
            const auto conversion=std::from_chars(input_.data()+start,input_.data()+offset_,number);
            if(conversion.ec!=std::errc{} || conversion.ptr!=input_.data()+offset_) return false;
            if(key=="mode" && number>6) return false;
        }
        return !semantic || (key!="alphaMode" && key!="interpolation" && !BooleanKey(key));
    }
    bool Value(std::string_view key,unsigned depth,bool semantic,bool integerValue=false)
    {
        Space();if(depth>64 || offset_==input_.size() || ++tokens_>1000000) return false;
        if(integerValue) {
            if(input_[offset_]<'0' || input_[offset_]>'9') return false;
            return Number("index",true);
        }
        if(semantic && ScalarKey(key) && (input_[offset_]=='{' || input_[offset_]=='[')) return false;
        if(input_[offset_]=='{') {
            ++offset_;
            if(Take('}')) return true;
            std::unordered_set<std::string> keys;
            do {
                std::string child;
                Space();const auto start=offset_;
                if(!String(child) || !keys.insert(child).second) return false;
                if(semantic && input_.substr(start,offset_-start).find('\\')!=std::string_view::npos) return false;
                const bool knownExtension=key!="extensions" || child=="KHR_texture_transform" || child=="KHR_mesh_quantization";
                const bool forceInteger=semantic && ((child=="indices" && key!="sparse") || (child=="target" && key=="bufferViews") || key=="attributes");
                if(!Take(':') || !Value(child,depth+1,semantic && child!="extras" && knownExtension,forceInteger)) return false;
            } while(Take(','));
            return Take('}');
        }
        if(input_[offset_]=='[') {
            ++offset_;if(Take(']')) return true;
            do { if(!Value(key,depth+1,semantic)) return false; } while(Take(','));
            return Take(']');
        }
        if(input_[offset_]=='"') {
            std::string value;
            const auto start=offset_;
            if(!String(value)) return false;
            if(semantic && (IntegerKey(key) || FloatKey(key) || BooleanKey(key))) return false;
            if(semantic && (key=="alphaMode" || key=="interpolation") && input_.substr(start,offset_-start).find('\\')!=std::string_view::npos) return false;
            if(semantic && key=="alphaMode") return value=="OPAQUE" || value=="MASK" || value=="BLEND";
            if(semantic && key=="interpolation") return value=="LINEAR" || value=="STEP" || value=="CUBICSPLINE";
            return true;
        }
        for(auto literal:{std::string_view("true"),std::string_view("false"),std::string_view("null")})
            if(input_.substr(offset_,literal.size())==literal) { offset_+=literal.size();return !semantic || (!IntegerKey(key) && !FloatKey(key) && key!="alphaMode" && key!="interpolation" && (!BooleanKey(key) || literal!="null")); }
        return Number(key,semantic);
    }
    std::string_view input_;
    std::size_t offset_{},tokens_{};
};
}
