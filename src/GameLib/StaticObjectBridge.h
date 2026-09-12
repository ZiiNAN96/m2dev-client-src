#pragma once
class CGraphicThingInstance;
namespace Renderer { struct StaticObjectDraw; }
// Read-only capture of the existing opaque draw contract; also used by GPU regression tests.
bool CaptureStaticMapObjectDraw(Renderer::StaticObjectDraw&);
// Only called for the existing opaque area-object list, never actor/shadow passes.
void SubmitStaticMapObject(CGraphicThingInstance&);
void ReleaseStaticMapObject(CGraphicThingInstance*);
