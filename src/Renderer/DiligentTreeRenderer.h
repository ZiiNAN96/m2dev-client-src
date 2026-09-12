#pragma once
#include "TreeRenderData.h"
#include "DiligentD3D11Backend.h"

namespace Renderer
{
// ZiiNAN: Diligent SpeedTree rendering integration on the existing world/depth surface.
class DiligentTreeRenderer final : public ITreeRenderer
{
public:
    explicit DiligentTreeRenderer(DiligentD3D11Backend&);
    ~DiligentTreeRenderer() override;
    bool Initialize();
    void ResetFrame();
    bool Failed() const;
    void ReportFailure() override;
    TreeGeometryPtr UploadGeometry(const TreeSource&,bool dynamic=false) override;
    bool UpdateVertices(const TreeGeometryPtr&,const std::vector<TreeVertex>&) override;
    TerrainTexturePtr UploadTexture(const TerrainTextureData&) override;
    void Draw(const void*,const TreeGeometryPtr&,const TerrainTexturePtr&,const TerrainTexturePtr&,const TreeDraw&) override;
    uint32_t VisibleInstances() const;
    uint32_t DrawCount(TreePart) const;
    uint64_t Vertices() const;
    uint64_t Indices() const;
    uint32_t LiveGeometryCount() const;
    uint32_t LiveTextureCount() const;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
