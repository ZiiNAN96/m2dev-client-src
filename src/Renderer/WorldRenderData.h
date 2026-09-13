#pragma once
#include "EffectRenderData.h"
#include <atomic>

namespace Renderer
{
// ZiiNAN: Original water/sky vertices share the tested fixed-function material contract.
enum class WorldPart : uint32_t { Water, Sky, Cloud, Guild, Dungeon, LensFlare };
using WorldResources=EffectResources;
inline std::atomic<uint32_t> waterGeometryCount{0};
struct WaterGeometry
{
    std::vector<EffectVertex> vertices;
    WaterGeometry() { ++waterGeometryCount; }
    ~WaterGeometry() { --waterGeometryCount; }
    WaterGeometry(const WaterGeometry&)=delete;
    WaterGeometry& operator=(const WaterGeometry&)=delete;
};
using WaterGeometryPtr=std::shared_ptr<WaterGeometry>;
class IWorldRenderer : public ITextureUploader
{
public:
    virtual void Draw(const EffectVertex*,uint32_t,const TerrainTexturePtr&,const EffectDraw&,WorldPart)=0;
    virtual void ReportFailure()=0;
    virtual void ReleaseBindings()=0;
};
inline IWorldRenderer* worldRenderer=nullptr;
inline bool worldSurfaceFrame=false;
inline uint64_t worldSurfaceSerial=0;
inline uint32_t waterTexturesResident=0;
struct SpecialMeshDraw { uint32_t mesh,material,firstIndex,indexCount,baseVertex,vertexCount; };
struct SpecialMeshTarget { const void* instance=nullptr; void (*submit)(const void*,const SpecialMeshDraw&)=nullptr; };
inline SpecialMeshTarget specialMeshTarget;
struct SpecialMeshScope
{
    SpecialMeshTarget previous=specialMeshTarget;
    explicit SpecialMeshScope(SpecialMeshTarget target) { specialMeshTarget=target; }
    ~SpecialMeshScope() { specialMeshTarget=previous; }
    SpecialMeshScope(const SpecialMeshScope&)=delete;
    SpecialMeshScope& operator=(const SpecialMeshScope&)=delete;
};
}
