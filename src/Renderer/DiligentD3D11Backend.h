#pragma once

#include "IRenderBackend.h"
#include <memory>

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

private:
    friend class BackendTestAccess;
    friend class DiligentTerrainRenderer;
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
}
