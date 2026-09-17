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
enum class PlantKind : std::uint32_t { Tree, Grass, Bush };
struct FoliageProfile {
    Vec3 transmissionColor{1,1,1};
    float transmissionStrength{.16f};
};
struct Metadata {
    std::uint32_t version{1};std::string geometry,shadowTexture;
    Bounds bounds,renderBounds;
    float nearDistance{},farDistance{},cullDistance{};
    WindProfile wind;
    std::vector<Part> parts;
    std::vector<LodState> lods; // Samples from far (0) to near (1), alpha interpolation within a state.
    std::vector<Collision> collisions;
    // Version 2 uses four authored states, far to near, in the same GLB.
    PlantKind plantKind{PlantKind::Tree};
    std::array<float,3> lodDistances{2500,5500,9000};
    float transitionFraction{.12f};
    FoliageProfile foliage;
};
struct LodBlend { LodState first,second;float transition{}; };
// At most two representations; a third request waits for the current handover.
struct LodTransition {
    LodState from,to;
    float coverage{},lastTime{};
    bool initialized{},active{};
    LodBlend Update(const LodState& target,float seconds);
};
struct StableLod { LodState state; float distance{}; };
std::vector<StableLod> BuildStableLods(const Metadata&);
LodState SelectStableLOD(std::span<const StableLod>, float distance, float hysteresis, int& state);
struct Quality {float treeDistance{1},grassDistance{2400},grassDensity{.5f},windDetail{1},transmission{1};};
Quality ResolveQuality(unsigned level);
LodBlend SelectModernLOD(const Metadata&,float distance);
std::array<std::array<float,4>,6> FrustumPlanes(const Matrix& view,const Matrix& projection);
// A stable candidate lattice: density changes retain the same subset/positions.
struct GrassCandidate {float x{},y{},rotation{},scale{1},rank{};std::uint64_t id{};};
std::vector<GrassCandidate> GrassCandidates(std::uint64_t mapSeed,int cellX,int cellY,float cellSize,unsigned count=24);
// Compact map-lifetime placement, prepared before the first world frame. GPU
// batches expand these records; camera movement never samples terrain again.
struct GrassPlacement {
    float x{},y{},z{},cosScale{},sinScale{},scale{},phase{},rank{};
    Matrix Transform() const;
};
GrassPlacement PlaceGrass(const GrassCandidate&,float height,float density);
Vec3 BranchDisplacement(float heightFraction,float height,float time,float phase,const WindProfile&,const Vec3& direction);
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
    Result AddOverride(std::string_view legacy,std::string_view compiled);
    const std::string* Resolve(std::string_view legacy)const;
    const std::string* ResolveOverride(std::string_view legacy)const;
    std::string Serialize()const;
    std::size_t Size()const{return entries_.size();}
    const auto& Entries()const{return entries_;}
private:
    std::map<std::string,std::string> entries_;
    std::map<std::string,std::string> overrides_;
};
struct Asset {
    Metadata metadata;AssetRuntime::AssetHandle geometry;
    std::vector<StableLod> stableLods;
    Asset(){++liveAssets;}~Asset(){--liveAssets;}
    Asset(const Asset&)=delete;Asset& operator=(const Asset&)=delete;
};
using AssetPtr=std::shared_ptr<const Asset>;
struct Instance {
    AssetPtr asset;Matrix transform{Identity};LodState lod;float phase{};
    int stableLod{-1};bool distanceVisible{true};
    LodTransition transition;
    Instance(AssetPtr,const Matrix&,std::uint64_t stableId=0);
    ~Instance(){--liveInstances;}
    Instance(const Instance&)=delete;Instance& operator=(const Instance&)=delete;
    bool Update(const Vec3& camera,std::span<const std::array<float,4>> planes={},float distanceScale=1.f);
    bool UpdateStable(const Vec3& camera,std::span<const std::array<float,4>> planes={},float distanceScale=1.f,bool fixedDetail=false);
};
using ReadFile=std::function<bool(std::string_view,std::vector<std::byte>&)>;
struct LoadResult { AssetPtr asset;std::string error;explicit operator bool()const{return bool(asset);} };
class Runtime {
public:
    Registry registry;
    LoadResult Load(std::string_view legacy,const ReadFile&,bool modern=false);
    LoadResult LoadCompiled(std::string_view path,const ReadFile&);
    void Clear(){assets_.clear();failures_.clear();}
    std::size_t CachedAssets()const{return assets_.size();}
private:
    std::map<std::string,AssetPtr> assets_;
    std::map<std::string,std::string> failures_;
};
}
