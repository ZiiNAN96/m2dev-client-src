///////////////////////////////////////////////////////////////////////  
//	CSpeedTreeForestOpenGL Class
//
//	(c) 2003 IDV, Inc.
//
//	This class is provided to illustrate one way to incorporate
//	SpeedTreeRT into an OpenGL application.  All of the SpeedTreeRT
//	calls that must be made on a per tree basis are done by this class.
//	Calls that apply to all trees (i.e. static SpeedTreeRT functions)
//	are made in the functions in main.cpp.
//
//
//	*** INTERACTIVE DATA VISUALIZATION (IDV) PROPRIETARY INFORMATION ***
//
//	This software is supplied under the terms of a license agreement or
//	nondisclosure agreement with Interactive Data Visualization and may
//	not be copied or disclosed except in accordance with the terms of
//	that agreement.
//
//      Copyright (c) 2001-2003 IDV, Inc.
//      All Rights Reserved.
//
//		IDV, Inc.
//		1233 Washington St. Suite 610
//		Columbia, SC 29201
//		Voice: (803) 799-1699
//		Fax:   (803) 931-0320
//		Web:   http://www.idvinc.com

#pragma once


///////////////////////////////////////////////////////////////////////  
//	Include Files

//#include <map>
#define SPEEDTREE_DATA_FORMAT_DIRECTX

#include "SpeedTreeForest.h"
#include "SpeedTreeMaterial.h"

///////////////////////////////////////////////////////////////////////  
//	class CSpeedTreeForestRenderer declaration
class CSpeedTreeForestRenderer : public CSpeedTreeForest, public CGraphicBase, public CSingleton<CSpeedTreeForestRenderer>
{
	public:
		CSpeedTreeForestRenderer();
		virtual ~CSpeedTreeForestRenderer();

		void			UploadWindMatrix(unsigned int uiLocation, const float* pMatrix) const;
		void			UpdateCompundMatrix(const Math::Vector3 & c_rEyeVec, const Math::Matrix & c_rmatView, const Math::Matrix& c_rmatProj);

		void			Render(unsigned long ulRenderBitVector = Forest_RenderAll);
		bool			InitializeLighting();
		
	private:
		
	private:


};
