#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace Renderer
{
using ClearColor = std::array<float, 4>;

struct InitializeInfo
{
    void* window = nullptr; // HWND on Windows; no graphics API types in this interface.
    uint32_t width = 0;
    uint32_t height = 0;
    bool windowed = true;
    int bitsPerPixel = 32;
    int refreshRate = 0;
};

struct ClearInfo
{
    bool colorAndDepth = false; // false: depth only, matching the normal legacy frame.
    std::optional<ClearColor> color; // absent: preserve the legacy clear state.
};

// Render-thread only. Select an implementation once, before initialization.
// Draw calls and resource creation intentionally remain outside this milestone.
class IRenderBackend
{
public:
    virtual ~IRenderBackend() = default;
    virtual bool Initialize(const InitializeInfo& info) = 0;
    virtual bool BeginFrame() = 0;
    virtual void Clear(const ClearInfo& info) = 0;
    virtual void EndFrame() = 0;
    virtual void Present() = 0;
    // Zero extent suspends rendering until the next non-zero resize.
    virtual bool Resize(uint32_t width, uint32_t height) = 0;
    virtual void Shutdown() = 0;
};
}
