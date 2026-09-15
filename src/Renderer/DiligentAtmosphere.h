#pragma once
#include "Graphics/SceneLighting.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/TextureView.h"
#include <memory>
#include <cstdint>

namespace Diligent {struct IRenderDevice;struct IDeviceContext;}
namespace Renderer
{
// Small owner for the pinned FX atmosphere's optical-depth and single-scattering
// tables. World positions remain Z-up centimetres outside this adapter.
class DiligentAtmosphere final
{
public:
    DiligentAtmosphere(Diligent::IRenderDevice*,Diligent::IDeviceContext*);
    ~DiligentAtmosphere();
    void Prepare(const Graphics::SceneLighting&,bool highQuality);
    Diligent::ITextureView* Sky() const;
    Diligent::ITextureView* Irradiance() const;
    Diligent::ITextureView* PrefilteredEnvironment() const;
    Diligent::ISampler* Sampler() const;
    std::uint64_t TargetBytes() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
