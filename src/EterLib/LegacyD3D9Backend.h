#pragma once

#include "Renderer/IRenderBackend.h"

class CGraphicDevice;
class CScreen;

namespace Renderer
{
class LegacyD3D9Backend final : public IRenderBackend
{
public:
    // The application's existing device and screen outlive this adapter.
    LegacyD3D9Backend(CGraphicDevice& device, CScreen& screen);
    ~LegacyD3D9Backend() override;
    bool Initialize(const InitializeInfo& info) override;
    bool BeginFrame() override;
    void Clear(const ClearInfo& info) override;
    void EndFrame() override;
    void Present() override;
    bool Resize(uint32_t width, uint32_t height) override;
    void Shutdown() override;

    int GetCreateResult() const { return m_createResult; }

private:
    CGraphicDevice& m_device;
    CScreen& m_screen;
    int m_createResult = 0;
    bool m_initialized = false;
    bool m_suspended = false;
    bool m_inFrame = false;
};
}
