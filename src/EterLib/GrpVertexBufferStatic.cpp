#include "StdAfx.h"
#include "GrpVertexBufferStatic.h"

bool CStaticVertexBuffer::Create(int vtxCount, DWORD fvf, bool /*isManaged*/)
{
	return CGraphicVertexBuffer::Create(vtxCount, fvf);
}

CStaticVertexBuffer::CStaticVertexBuffer()
{
}

CStaticVertexBuffer::~CStaticVertexBuffer()
{
}
