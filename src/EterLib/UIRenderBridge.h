#pragma once
#include "Renderer/UIRenderData.h"
#include "Renderer/DrawStateTypes.h"

class CGraphicImage;
// ZiiNAN: Only explicitly selected UI primitives enter this synchronous bridge.
namespace UIRenderBridge
{
enum class Primitive { Strip, List, Fan, Lines, IndexedQuad };
void Submit(const void* pdt,uint32_t count,Primitive,CGraphicImage*,
            Renderer::TerrainTexturePtr supplied={},Renderer::TerrainTexturePtr secondary={});
inline void Quad(const void* pdt,CGraphicImage* image)
{ Submit(pdt,4,Primitive::Strip,image); }
inline void IndexedQuad(const void* pdt,CGraphicImage* image)
{ Submit(pdt,4,Primitive::IndexedQuad,image); }
inline CGraphicImage* scopedImage=nullptr;
struct ImageScope
{
    CGraphicImage* previous;
    explicit ImageScope(CGraphicImage* image):previous(scopedImage) { scopedImage=image; }
    ~ImageScope() { scopedImage=previous; }
};
}
