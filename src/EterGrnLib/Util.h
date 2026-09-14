#pragma once


class CGraphicImage;

struct SMaterialData
{
	CGraphicImage * pImage;
	float fSpecularPower;
	BOOL isSpecularEnable;	
	BYTE bSphereMapIndex;
};

