#pragma once
#include "Renderer/UIRenderData.h"
#include <d3d9.h>

class CGraphicImage;
// ZiiNAN: Only explicitly selected UI primitives enter this synchronous bridge.
namespace UIRenderBridge
{
enum class Primitive { Strip, List, Fan, Lines, IndexedQuad };
void Submit(const void* pdt,uint32_t count,Primitive,CGraphicImage*,HRESULT);
inline void Quad(const void* pdt,CGraphicImage* image,HRESULT result)
{ Submit(pdt,4,Primitive::Strip,image,result); }
inline void IndexedQuad(const void* pdt,CGraphicImage* image,HRESULT result)
{ Submit(pdt,4,Primitive::IndexedQuad,image,result); }
inline CGraphicImage* scopedImage=nullptr;
struct ImageScope
{
    CGraphicImage* previous;
    explicit ImageScope(CGraphicImage* image):previous(scopedImage) { scopedImage=image; }
    ~ImageScope() { scopedImage=previous; }
};
}
