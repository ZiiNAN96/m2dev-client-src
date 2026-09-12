#pragma once
class CGraphicThingInstance;
class CGraphicImage;
namespace Renderer { struct StaticObjectDraw; }
// Read-only legacy state capture, also used by native D3D9 parity tests.
bool CaptureStaticMapObjectDraw(Renderer::StaticObjectDraw&, bool cameraMask = false, bool shadowBase = false);
// Capture before the legacy dynamic shadow stage changes the base material state.
void BeginStaticMapObjects(bool legacyShadowActive);
enum class StaticMapObjectPass { Opaque, ShadowReceiver, CameraBlocker };
// Existing map lists only; never actor or shadow-map generation passes.
void SubmitStaticMapObject(CGraphicThingInstance&, StaticMapObjectPass = StaticMapObjectPass::Opaque, CGraphicImage* = nullptr);
void ReleaseStaticMapObject(CGraphicThingInstance*);
