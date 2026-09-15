#pragma once

#include "IRenderBackend.h"
#include "GraphicsConfig.h"
#include <memory>
#include <vector>

namespace Renderer
{
class BackendTestAccess;

class DiligentD3D11Backend final : public IRenderBackend
{
public:
    DiligentD3D11Backend();
    ~DiligentD3D11Backend() override;
    bool Initialize(const InitializeInfo& info) override;
    bool BeginFrame() override;
    void Clear(const ClearInfo& info) override;
    void EndFrame() override;
    void Present() override;
    bool Resize(uint32_t width, uint32_t height) override;
    void Shutdown() override;
    bool CaptureRGB(std::vector<uint8_t>& pixels,uint32_t& width,uint32_t& height);
    const Graphics::GraphicsRuntimeConfig& GetGraphicsConfig() const { return m_graphicsConfig; }

private:
    friend class BackendTestAccess;
    friend class DiligentTerrainRenderer;
    friend class DiligentStaticObjectRenderer;
    friend class DiligentModernRenderer;
    friend class DiligentEffectRenderer; // ZiiNAN: Diligent effect rendering integration.
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    Graphics::GraphicsRuntimeConfig m_graphicsConfig;
};
}
