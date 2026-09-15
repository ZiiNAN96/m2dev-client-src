#pragma once
#include "Graphics/ShadowAmbient.h"
#include <atomic>
#include <unordered_set>
#include <bit>

namespace Renderer {
inline int shadowPassIndex=-1;
inline Graphics::ShadowCascade activeShadowCascade;
inline Graphics::ShadowCascades shadowCullingCascades;
inline bool preparingShadowCasters{};
inline std::atomic_uint64_t liveShadowMaps{},liveShadowViews{},liveShadowPipelines{},liveShadowBuffers{};
inline std::atomic_uint64_t liveAOTargets{},liveAOViews{},liveAOPipelines{},liveAOBuffers{};
inline std::uint64_t shadowDraws{},shadowCasters{},shadowCulled{},shadowMemory{},aoMemory{};
inline double shadowCpuUs{},aoCpuUs{};
inline double lastAOGpuUs{-1};
struct ShadowCasterKey {const void* geometry{};Graphics::Matrix4 world{};bool operator==(const ShadowCasterKey&)const=default;};
struct ShadowCasterHash {std::size_t operator()(const ShadowCasterKey& k)const{
    auto hash=std::hash<const void*>{}(k.geometry);for(float v:k.world)hash^=std::size_t(v==0?0:std::bit_cast<std::uint32_t>(v))+0x9e3779b9+(hash<<6)+(hash>>2);return hash;
}};
inline std::unordered_set<ShadowCasterKey,ShadowCasterHash> frameShadowCasters;
inline void RecordShadowCaster(const void* geometry,const Graphics::Matrix4& world){
    ++shadowDraws;if(frameShadowCasters.insert({geometry,world}).second)++shadowCasters;
}
inline unsigned ambientDebugView{},cascadeDebugView{};
inline bool ShadowVisible(const Graphics::Vector3& center,float radius) {
    return shadowPassIndex<0||Graphics::IntersectsCascade(activeShadowCascade,center,radius);
}
}
