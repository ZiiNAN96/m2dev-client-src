#pragma once
#include "AssetRuntime/AssetRuntime.h"
#include <functional>
#include <map>
#include <set>

namespace Vegetation {
// ZiiNAN: Vegetation runtime boundary
using Vec3=std::array<float,3>;
using Matrix=AssetRuntime::Matrix4;
using Bounds=AssetRuntime::Bounds;
inline constexpr Matrix Identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
inline std::atomic_size_t liveAssets{},liveInstances{};
enum class PartKind : std::uint32_t { Branch, Frond, Leaf, Billboard };
struct Part { PartKind kind{};std::uint32_t lod{},mesh{}; };
struct WindProfile {
    Vec3 direction{1,0,0};float strength{1},branchAmplitude{},frondAmplitude{},leafAmplitude{0.018f},frequency{1.3f};
};
struct LodState {
    std::array<std::int32_t,5> meshes{-1,-1,-1,-1,-1}; // branch, frond, leaf 0/1, billboard
    std::array<float,5> alpha{255,255,255,255,255};
};
struct Collision { std::uint32_t kind{};Vec3 position{},dimensions{}; };
struct Metadata {
    std::uint32_t version{1};std::string geometry,shadowTexture;
    Bounds bounds,renderBounds;
    float nearDistance{},farDistance{},cullDistance{};
    WindProfile wind;
    std::vector<Part> parts;
    std::vector<LodState> lods; // Samples from far (0) to near (1), alpha interpolation within a state.
    std::vector<Collision> collisions;
};
struct Result { bool ok{};std::string error;explicit operator bool()const{return ok;} };
bool ValidBounds(const Bounds&);
bool ValidTransform(const Matrix&);
Bounds TransformBounds(const Bounds&,const Matrix&);
float WindPhase(const Matrix&,std::uint64_t stableId=0);
LodState SelectLOD(const Metadata&,float distance);
bool Visible(const Bounds&,std::span<const std::array<float,4>> planes);
std::string NormalizeKey(std::string_view);
bool ValidCompiledPath(std::string_view,std::string_view extension);
Result ParseMetadata(std::string_view,Metadata&);
std::string SerializeMetadata(const Metadata&);
class Registry {
public:
    Result Parse(std::string_view);
    Result Add(std::string_view legacy,std::string_view compiled);
    const std::string* Resolve(std::string_view legacy)const;
    std::string Serialize()const;
    std::size_t Size()const{return entries_.size();}
private:
    std::map<std::string,std::string> entries_;
};
struct Asset {
    Metadata metadata;AssetRuntime::AssetHandle geometry;
    Asset(){++liveAssets;}~Asset(){--liveAssets;}
    Asset(const Asset&)=delete;Asset& operator=(const Asset&)=delete;
};
using AssetPtr=std::shared_ptr<const Asset>;
struct Instance {
    AssetPtr asset;Matrix transform{Identity};LodState lod;float phase{};
    Instance(AssetPtr,const Matrix&,std::uint64_t stableId=0);
    ~Instance(){--liveInstances;}
    Instance(const Instance&)=delete;Instance& operator=(const Instance&)=delete;
    bool Update(const Vec3& camera,std::span<const std::array<float,4>> planes={});
};
using ReadFile=std::function<bool(std::string_view,std::vector<std::byte>&)>;
struct LoadResult { AssetPtr asset;std::string error;explicit operator bool()const{return bool(asset);} };
class Runtime {
public:
    Registry registry;
    LoadResult Load(std::string_view legacy,const ReadFile&);
    void Clear(){assets_.clear();failures_.clear();}
    std::size_t CachedAssets()const{return assets_.size();}
private:
    std::map<std::string,AssetPtr> assets_;
    std::map<std::string,std::string> failures_;
};
}
