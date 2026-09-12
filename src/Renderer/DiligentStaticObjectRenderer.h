#pragma once
#include "StaticObjectRenderData.h"
#include "DiligentD3D11Backend.h"

namespace Renderer
{
class DiligentStaticObjectRenderer final : public IStaticObjectRenderer
{
public:
    explicit DiligentStaticObjectRenderer(DiligentD3D11Backend&);
    ~DiligentStaticObjectRenderer() override;
    bool Initialize();
    void ResetFrame();
    bool Failed() const;
    uint32_t DrawCount() const;
    uint32_t LiveGeometryCount() const;
    uint32_t LiveTextureCount() const;
    StaticObjectGeometryPtr UploadGeometry(const StaticObjectSource&) override;
    TerrainTexturePtr UploadTexture(const TerrainTextureData&) override;
    void Draw(const StaticObjectGeometryPtr&, const TerrainTexturePtr&, const StaticObjectDraw&) override;
    void ReleaseBindings() override;
private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
