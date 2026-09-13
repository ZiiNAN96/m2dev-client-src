#pragma once
class CGraphicThingInstance;
class CGraphicImage;
namespace Renderer { struct StaticObjectDraw; }
// Read-only legacy state capture, consumed by the Diligent material renderer.
// ZiiNAN: Actor lighting opts into native selection light 1 without shadow alpha semantics.
bool CaptureStaticMapObjectDraw(Renderer::StaticObjectDraw&, bool cameraMask = false, bool shadowBase = false, bool actorLighting = false, bool groundItem = false);
// Capture before the legacy dynamic shadow stage changes the base material state.
void BeginStaticMapObjects(bool legacyShadowActive);
enum class StaticMapObjectPass { Opaque, ShadowReceiver, CameraBlocker, GroundItem };
// Existing map lists only; never actor or shadow-map generation passes.
void SubmitStaticMapObject(CGraphicThingInstance&, StaticMapObjectPass = StaticMapObjectPass::Opaque, CGraphicImage* = nullptr);
void ReleaseStaticMapObject(CGraphicThingInstance*);
bool DrawSpecialMapObject(CGraphicThingInstance&,bool blend,CGraphicImage* cameraAlpha=nullptr);
