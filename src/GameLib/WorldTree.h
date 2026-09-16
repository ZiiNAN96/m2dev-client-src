#pragma once
#include "EterLib/GrpTreeInstance.h"
#include "Vegetation/VegetationRenderer.h"
class CGraphicImage;
WorldTreePtr CreateWorldTree(float x,float y,float z,std::uint32_t crc,const char* legacyKey);
void DeleteWorldTree(WorldTreePtr&);
void RenderNativeVegetation();
class CMapOutdoor;
void PrepareNativeGrass(CMapOutdoor&);
void RenderNativeGrass(CMapOutdoor&);
void ClearNativeVegetation();
void SetNativeVegetationWind(float);
class VegetationCameraMaskScope {
public:
    explicit VegetationCameraMaskScope(CGraphicImage*);
    ~VegetationCameraMaskScope();
private:CGraphicImage* previous_{};
};
