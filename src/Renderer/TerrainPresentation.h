#pragma once
#include <memory>
#include <cstdint>

namespace Renderer
{
class ITerrainPresentation
{
public:
    virtual ~ITerrainPresentation() = default;
    virtual bool BeginFrame() = 0;
    virtual bool Present() = 0;
    virtual bool Resize(uint32_t width, uint32_t height) = 0;
};
bool IsDiligentTerrainAvailable();
std::unique_ptr<ITerrainPresentation> CreateTerrainPresentation(void* parent, uint32_t width, uint32_t height);
}
