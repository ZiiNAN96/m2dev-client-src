#pragma once
#include "AssetRuntime/AssetRuntime.h"
#include "StaticObjectRenderData.h"
#include <algorithm>
#include <cmath>

namespace Renderer
{
// ZiiNAN: Modern asset pipeline
inline void ApplyAssetMaterial(const AssetRuntime::MaterialAsset& material, StaticObjectDraw& draw)
{
    if (!material.explicitRenderState) return;
    draw.cull = material.culling == AssetRuntime::Culling::None ? StaticObjectCull::None : StaticObjectCull::Clockwise;
    draw.blend = material.blending;
    draw.depthWrite = material.depthWrite;
    draw.alphaTest = material.alphaTest ? StaticObjectAlphaTest::GreaterEqual : StaticObjectAlphaTest::Disabled;
    // The existing legacy alpha test compares an eight-bit stage result.
    draw.alphaReference = static_cast<uint32_t>(std::ceil(std::clamp(material.alphaCutoff, 0.0f, 1.0f) * 255.0f));
    if (material.alphaTest && material.alphaCutoff > 1.0f) draw.alphaTest = StaticObjectAlphaTest::Greater;
    draw.actorStage = ActorMaterialStage::Modulate;
    draw.textureFactor = material.baseColorFactor;
    draw.materialBaseColorInFactor = true;
    draw.factorAlpha = material.alphaTest || material.blending;
    draw.factorAlphaOnly = !draw.factorAlpha;
    if (draw.factorAlphaOnly) draw.textureFactor[3] = 1.0f;
    draw.textureAlpha = false;
    draw.diffuseAlphaOnly = false;
    // Camera blockers keep the existing camera-mask pass blend/depth semantics.
    if (draw.cameraAlpha) { draw.blend = true; draw.depthWrite = false; }
}
}
