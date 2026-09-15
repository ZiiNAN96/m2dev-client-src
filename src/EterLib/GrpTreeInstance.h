#pragma once
#include "GrpObjectInstance.h"
#include <memory>
// ZiiNAN: Vegetation runtime boundary for existing world culling and collision consumers.
class CWorldTreeInstance : public CGraphicObjectInstance {
public:
    virtual void SetPosition(float x,float y,float z)=0;
    virtual const float* GetPosition()=0;
};
using WorldTreePtr=std::shared_ptr<CWorldTreeInstance>;
