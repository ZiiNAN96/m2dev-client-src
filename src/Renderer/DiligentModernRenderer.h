#pragma once
// Implementation boundary: never included by game or asset public headers.
#include "ModernFrame.h"
#include "StaticObjectRenderData.h"
#include "TerrainRenderData.h"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Buffer.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"
#include <memory>

namespace Renderer
{
class DiligentD3D11Backend;
struct ModernTerrainSubmission
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertices,indices,attributes;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> color,alpha;
    Diligent::RefCntAutoPtr<Diligent::ISampler> colorSampler,alphaSampler;
    TerrainMatrices matrices;
    TerrainSplatParameters parameters;
    unsigned count{};
    bool strip{};
    bool solid{};
};
struct ModernMeshSubmission
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> vertices,indices,extras,palette,tangents;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> instances;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITextureView>,AssetRuntime::MaterialTextureCount> textures;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> cameraAlpha,sphereMap;
    StaticObjectDraw draw;
    Diligent::VALUE_TYPE indexType{Diligent::VT_UINT16};
    unsigned baseVertex{};
    unsigned tangentOffset{};
    unsigned instanceCount{1};
    bool skinned{},auxiliary{};
};
class DiligentModernRenderer final : public IModernFrame
{
public:
    explicit DiligentModernRenderer(DiligentD3D11Backend&);
    ~DiligentModernRenderer();
    void Begin(const Graphics::SceneLighting&,bool deferToneMapping=false) override;
    void SetCamera(const TerrainMatrices&) override;
    void End() override;
    void FinishWorld() override;
    void DrawWater(const EffectVertex*,unsigned,const EffectDraw&) override;
    void FinishWater() override;
    bool HDRWorldActive() const;
    void BindWorldTarget();
    bool BeginShadowCollection() override;
    void EndShadowCollection() override;
    bool ShadowCasterVisible(const std::array<float,3>&,float) const override;
    void BeginForwardWorld() override;
    void EndForwardWorld() override;
    ModernFrameStats Stats() const override;
    bool Active() const;
    void Draw(const ModernMeshSubmission&);
    void DrawTerrain(const ModernTerrainSubmission&);
    void ReleaseWindowResources();
    void ResetFrame();
    Diligent::ITextureView* DepthView() const;
    void BindTargets();
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
