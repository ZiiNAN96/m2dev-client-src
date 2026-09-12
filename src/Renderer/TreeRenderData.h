#pragma once
#include "TerrainRenderData.h"
#include <string>
#include <unordered_map>

namespace Renderer
{
// ZiiNAN: Diligent SpeedTree rendering integration; no runtime or graphics API types.
enum class TreePart : uint32_t { Branch, Frond, Leaf, Billboard };
struct TreeVertex
{
    std::array<float,3> position{};
    uint32_t color=0xffffffff; // Native D3DCOLOR (BGRA bytes).
    std::array<float,2> uv{}, shadowUv{}, leaf{}; // Native constant-register index / LOD scalar.
};
static_assert(sizeof(TreeVertex)==40);
struct TreeSource { std::vector<TreeVertex> vertices; std::vector<uint16_t> indices; };
struct TreeGeometry { virtual ~TreeGeometry()=default; };
using TreeGeometryPtr=std::shared_ptr<TreeGeometry>;
struct TreeMesh { TreeSource source; TreeGeometryPtr geometry; };
struct TreeModelData
{
    TreeMesh branches,fronds;
    std::vector<TreeMesh> leaves;
    std::unordered_map<std::string,TerrainTexturePtr> textures;
    std::string asset;
};
struct TreeSampler
{
    TerrainSampling sampling{};
    bool anisotropic=false;
    uint32_t maxAnisotropy=1;
};
struct TreeDraw
{
    TerrainMatrices matrices{};
    std::array<std::array<float,4>,96> legacyConstants{};
    std::array<float,16> textureTransform{};
    std::array<float,4> fogColor{},fogParameters{};
    TreeSampler samplers[2];
    TreePart part=TreePart::Branch;
    uint32_t fog=0,stage1=0; // Fog: native 0..3, 4=legacy leaf shader. Stage1: 0=off, 1=RGB, 2=alpha modulation.
    bool rangeFog=false,cameraCoordinates=false,strip=true,blend=false,depthTest=true,depthWrite=true,alphaTest=true;
    uint32_t cull=1,depthFunction=4,alphaReference=0; // Cull: none/CW/CCW. Depth: native D3DCMP (1..8).
    uint32_t first=0,count=0;
};
class ITreeRenderer : public ITextureUploader
{
public:
    virtual TreeGeometryPtr UploadGeometry(const TreeSource&,bool dynamic=false)=0;
    virtual bool UpdateVertices(const TreeGeometryPtr&,const std::vector<TreeVertex>&)=0;
    virtual void Draw(const void* instance,const TreeGeometryPtr&,const TerrainTexturePtr&,const TerrainTexturePtr&,const TreeDraw&)=0;
    virtual void ReportFailure()=0;
};
inline ITreeRenderer* treeRenderer=nullptr;
inline bool treeWorldFrame=false;
inline bool treeDrawScope=false;
struct TreeDrawScope
{
    bool previous=treeDrawScope;
    explicit TreeDrawScope(bool active) { treeDrawScope=active && treeWorldFrame && treeRenderer; }
    ~TreeDrawScope() { treeDrawScope=previous; }
    TreeDrawScope(const TreeDrawScope&)=delete;
    TreeDrawScope& operator=(const TreeDrawScope&)=delete;
};
}
