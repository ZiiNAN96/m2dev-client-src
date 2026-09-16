#include "VegetationRuntime.h"
#include "AssetRuntime/GlTF/GlTFAssetProvider.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace Vegetation {
bool ValidBounds(const Bounds& b){
    if(!b.valid)return false;
    for(unsigned i=0;i<3;++i)if(!std::isfinite(b.min[i])||!std::isfinite(b.max[i])||b.min[i]>b.max[i]||std::abs(b.min[i])>1e8f||std::abs(b.max[i])>1e8f)return false;
    return true;
}
bool ValidTransform(const Matrix& m){
    for(float f:m)if(!std::isfinite(f)||std::abs(f)>1e9f)return false;
    const double det=double(m[0])*(double(m[5])*m[10]-double(m[6])*m[9])-double(m[1])*(double(m[4])*m[10]-double(m[6])*m[8])+double(m[2])*(double(m[4])*m[9]-double(m[5])*m[8]);
    return std::abs(det)>1e-12&&m[3]==0&&m[7]==0&&m[11]==0&&m[15]==1;
}
Bounds TransformBounds(const Bounds& b,const Matrix& m){
    Bounds out;if(!ValidBounds(b)||!ValidTransform(m))return out;
    for(unsigned corner=0;corner<8;++corner){
        Vec3 p{};for(unsigned k=0;k<3;++k)p[k]=(corner&(1<<k))?b.max[k]:b.min[k];
        for(unsigned k=0;k<3;++k){const float v=p[0]*m[k]+p[1]*m[4+k]+p[2]*m[8+k]+m[12+k];if(!corner)out.min[k]=out.max[k]=v;else{out.min[k]=std::min(out.min[k],v);out.max[k]=std::max(out.max[k],v);}}
    }out.valid=true;return ValidBounds(out)?out:Bounds{};
}
float WindPhase(const Matrix& m,std::uint64_t id){
    std::uint64_t hash=14695981039346656037ull;
    for(unsigned i=12;i<15;++i){const auto bits=std::bit_cast<std::uint32_t>(m[i]==0?0.f:m[i]);for(unsigned s=0;s<32;s+=8){hash^=(bits>>s)&255;hash*=1099511628211ull;}}
    for(unsigned s=0;s<64;s+=8){hash^=(id>>s)&255;hash*=1099511628211ull;}
    return float(hash&0xffffffu)*(6.28318530718f/16777216.f);
}
LodState SelectLOD(const Metadata& m,float distance){
    if(!std::isfinite(distance)||distance<0||distance>m.cullDistance||m.lods.empty()||m.farDistance<=m.nearDistance)return {};
    const float level=std::clamp((m.farDistance-distance)/(m.farDistance-m.nearDistance),0.f,1.f);
    const float f=level*float(m.lods.size()-1);const auto i=static_cast<std::size_t>(f);auto out=m.lods[i];
    if(i+1<m.lods.size()&&out.meshes==m.lods[i+1].meshes)for(unsigned k=0;k<5;++k)out.alpha[k]+=(m.lods[i+1].alpha[k]-out.alpha[k])*(f-float(i));
    return out;
}
bool Visible(const Bounds& b,std::span<const std::array<float,4>> planes){
    if(!ValidBounds(b))return false;
    for(const auto& p:planes){double d=p[3];for(unsigned k=0;k<3;++k)d+=p[k]*(p[k]>=0?b.max[k]:b.min[k]);if(!std::isfinite(d)||d<0)return false;}return true;
}
std::string NormalizeKey(std::string_view value){
    std::string out(value);for(auto& c:out){if(c=='\\')c='/';else if(c>='A'&&c<='Z')c+=32;}return out;
}
bool ValidCompiledPath(std::string_view path,std::string_view ext){
    if(path.size()>1024||!path.starts_with("vegetation/")||!path.ends_with(ext)||path.find(':')!=path.npos||path.find('\\')!=path.npos)return false;
    std::size_t begin=0;while(begin<path.size()){const auto end=path.find('/',begin);const auto part=path.substr(begin,end==path.npos?path.size()-begin:end-begin);if(part.empty()||part=="."||part=="..")return false;begin=end==path.npos?path.size():end+1;}
    return std::none_of(path.begin(),path.end(),[](unsigned char c){return c<32;});
}
Instance::Instance(AssetPtr a,const Matrix& m,std::uint64_t id):asset(std::move(a)),transform(m),phase(WindPhase(m,id)){
    if(!asset||!ValidTransform(m))throw std::invalid_argument("invalid vegetation instance");++liveInstances;
}
bool Instance::Update(const Vec3& camera,std::span<const std::array<float,4>> planes,float distanceScale){
    double squared=0;for(unsigned i=0;i<3;++i){const double d=double(camera[i])-transform[12+i];squared+=d*d;}
    if(!std::isfinite(distanceScale)||distanceScale<=0){lod={};return false;}
    const float distance=static_cast<float>(std::sqrt(squared))/distanceScale;
    if(!std::isfinite(distance)||distance>asset->metadata.cullDistance||!Visible(TransformBounds(asset->metadata.renderBounds,transform),planes)){lod={};return false;}
    lod=SelectLOD(asset->metadata,distance);return true;
}
LoadResult Runtime::Load(std::string_view legacy,const ReadFile& read,bool modern){
    const auto key=NormalizeKey(legacy);const auto* path=registry.Resolve(key);if(!path)return {{},"registry lookup missing: "+key};
    if(modern)if(const auto* overridePath=registry.ResolveOverride(key)) {
        auto result=LoadCompiled(*overridePath,read);if(result)return result;
        // Optional content must never hide a working legacy tree.
    }
    return LoadCompiled(*path,read);
}
LoadResult Runtime::LoadCompiled(std::string_view compiled,const ReadFile& read){
    const std::string normalized=NormalizeKey(compiled);const auto* path=&normalized;
    if(!ValidCompiledPath(*path,".zveg"))return {{},"invalid compiled vegetation path"};
    if(const auto found=assets_.find(*path);found!=assets_.end())return {found->second,{}};
    if(const auto found=failures_.find(*path);found!=failures_.end())return {{},found->second};
    auto fail=[&](std::string error)->LoadResult{failures_[*path]=error;return {{},std::move(error)};};
    try{
        std::vector<std::byte> bytes;if(!read(*path,bytes))return fail("compiled metadata missing: "+*path);
        Metadata metadata;auto parsed=ParseMetadata({reinterpret_cast<const char*>(bytes.data()),bytes.size()},metadata);if(!parsed)return fail(parsed.error);
        if(!read(metadata.geometry,bytes))return fail("compiled geometry missing: "+metadata.geometry);
        auto loaded=AssetRuntime::GetGlTFAssetProvider().Load(metadata.geometry,bytes);if(!loaded)return fail(loaded.diagnostic);
        if(loaded.asset.ModelCount()!=1)return fail("vegetation GLB must have one model");const auto& model=*loaded.asset.Model(0).Get();
        if(model.deformation!=AssetRuntime::Deformation::Rigid||!model.renderable||model.meshes.size()!=metadata.parts.size())return fail("vegetation mesh contract mismatch");
        std::set<std::uint32_t> used;
        for(const auto& p:metadata.parts){if(p.mesh>=model.meshes.size()||!used.insert(p.mesh).second)return fail("invalid part mesh");const auto& mesh=model.meshes[p.mesh];if(!ValidBounds(mesh.bounds)||!mesh.vertexCount||!mesh.indexCount)return fail("invalid part bounds or geometry");if(mesh.vertexExtrasChannels!=63||mesh.vertexExtras.size()!=mesh.vertexCount)return fail("missing compiled auxiliary channels");for(unsigned k=0;k<3;++k)if(mesh.bounds.min[k]<metadata.renderBounds.min[k]-.1f||mesh.bounds.max[k]>metadata.renderBounds.max[k]+.1f)return fail("render bounds do not enclose geometry");}
        for(const auto& lod:metadata.lods)for(auto index:lod.meshes)if(index>=0&&!used.contains(static_cast<std::uint32_t>(index)))return fail("LOD references nonexistent mesh");
        auto asset=std::make_shared<Asset>();asset->metadata=std::move(metadata);asset->geometry=std::move(loaded.asset);assets_[*path]=asset;return {asset,{}};
    }catch(const std::exception&e){return fail(e.what());}
}
}
