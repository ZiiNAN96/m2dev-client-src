#pragma once
#include <memory>
#include <cstdint>
#include <vector>
#include <functional>

namespace Renderer
{
// ZiiNAN: Device-independent metadata supplied by the active Diligent device.
struct GraphicsCapabilities { uint32_t maxTextureDimension=0; uint64_t localMemory=0; };
inline GraphicsCapabilities graphicsCapabilities;
class ITerrainPresentation
{
public:
    virtual ~ITerrainPresentation() = default;
    virtual bool BeginFrame() = 0;
    virtual bool Present() = 0;
    virtual bool Resize(uint32_t width, uint32_t height) = 0;
    virtual void ClearDepth(float depth) = 0;
    using ScreenshotSink=std::function<bool(std::vector<uint8_t>&,uint32_t,uint32_t)>;
    virtual bool RequestScreenshot(ScreenshotSink sink) = 0;
};
inline ITerrainPresentation* activePresentation=nullptr;
std::unique_ptr<ITerrainPresentation> CreateTerrainPresentation(void* parent, uint32_t width, uint32_t height);
}
