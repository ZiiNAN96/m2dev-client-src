#pragma once

#include "GrpBase.h"
#include "DrawState.h"

#include <map>

class CGraphicDevice : public CGraphicBase
{
public:

	enum ECreateReturnValues { CREATE_OK=1, CREATE_DEVICE=16 };

	CGraphicDevice();
	virtual ~CGraphicDevice();


	void			Destroy();
	int				Create(HWND hWnd, int hres, int vres, bool Windowed = true, int bit = 32, int ReflashRate = 0);



	bool			ResizeBackBuffer(UINT uWidth, UINT uHeight);

protected:
	CDrawState*				m_drawState;
};
