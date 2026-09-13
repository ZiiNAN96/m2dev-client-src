#pragma once

#include "SkinningData.h"
#include <string_view>
#include <chrono>

namespace Renderer
{
// ZiiNAN: Diligent GPU skinning prototype
inline constexpr size_t gpuPrototypeBoneLimit = 163;
inline constexpr size_t gpuPrototypeBufferBones = 256;
static_assert(gpuPrototypeBoneLimit <= gpuPrototypeBufferBones);
static_assert(gpuPrototypeBufferBones * sizeof(SkinningMatrix) == 16384);

enum class PrototypeSkinningMode { CPU, GPUPrototype };
inline PrototypeSkinningMode startupSkinningMode = PrototypeSkinningMode::CPU;
inline bool IsReferenceSkinningAsset(std::string_view path)
{
    constexpr std::string_view expected="d:/ymir work/pc/warrior/warrior_novice.gr2";
    if(path.size()!=expected.size()) return false;
    for(size_t i=0;i<path.size();++i) {
        char c=path[i]; if(c=='\\') c='/'; if(c>='A' && c<='Z') c=char(c-'A'+'a');
        if(c!=expected[i]) return false;
    }
    return true;
}
inline std::atomic_size_t livePrototypeGeometry{}, livePrototypePalettes{};
inline uint64_t prototypeFrames{}, prototypeBoneBytes{}, prototypeCpuFrames{}, prototypeCpuBytes{};
inline double prototypePrepareUs{}, prototypeCpuSkinUs{};
using PrototypeClock = std::chrono::steady_clock;
inline double PrototypeMicroseconds(PrototypeClock::time_point start)
{ return std::chrono::duration<double,std::micro>(PrototypeClock::now()-start).count(); }

inline bool ValidPrototypePalette(const BonePalette& palette)
{
    if(!palette.ready || !palette.skeleton || palette.matrices.empty() ||
       palette.matrices.size()!=palette.skeleton->names.size() || palette.matrices.size()>gpuPrototypeBoneLimit) return false;
    for(const auto& matrix:palette.matrices) for(float value:matrix) if(!std::isfinite(value)) return false;
    return true;
}

inline bool IsReferenceSkinningModel(const SkinningModelData& data)
{
    if(!data.skeleton || data.skeleton->names.size()!=75 || data.meshes.size()!=3)
        return false;
    size_t vertices=0, indices=0;
    for(size_t mesh=0;mesh<data.meshes.size();++mesh) {
        if(mesh>=data.status.size() || data.status[mesh]!=SkinDataStatus::Ready || !data.meshes[mesh])
            return false;
        vertices+=data.meshes[mesh]->vertices.size();
        indices+=data.meshes[mesh]->indices.size();
    }
    return vertices==2207 && indices==6804;
}

inline bool BuildPrototypeVertices(const SkinningModelData& data,
    const std::vector<std::shared_ptr<const BoneRemap>>& remaps, const BonePalette& palette,
    std::vector<SkinningVertex>& vertices, std::vector<uint16_t>& indices)
{
    vertices.clear(); indices.clear();
    if(!IsReferenceSkinningModel(data) || !ValidPrototypePalette(palette) ||
       remaps.size()!=data.meshes.size() || palette.skeleton!=data.skeleton) return false;
    for(size_t m=0;m<data.meshes.size();++m) {
        const auto& mesh=*data.meshes[m]; const auto& remap=remaps[m];
        SkinDataDiagnostics diagnostics;
        if(!remap || remap->destination!=palette.skeleton || remap->meshToSkeleton.size()!=mesh.meshBoneCount ||
           mesh.deformVertexOffset!=vertices.size() ||
           ValidateSkinVertices(mesh.vertices,mesh.meshBoneCount,diagnostics)!=SkinDataStatus::Ready) return false;
        for(auto index:mesh.indices) if(index>=mesh.vertices.size()) return false;
        for(auto vertex:mesh.vertices) {
            for(size_t k=0;k<4;++k) {
                if(!vertex.weights[k]) { vertex.indices[k]=0; continue; }
                const auto bone=remap->meshToSkeleton[vertex.indices[k]];
                if(bone>=palette.matrices.size()) return false;
                vertex.indices[k]=static_cast<uint8_t>(bone);
            }
            vertices.push_back(vertex);
        }
        indices.insert(indices.end(),mesh.indices.begin(),mesh.indices.end());
    }
    return true;
}
}
