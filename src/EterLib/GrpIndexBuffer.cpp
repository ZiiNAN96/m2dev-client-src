#include "StdAfx.h"
#include "EterBase/Stl.h"
#include "GrpIndexBuffer.h"
#include "DrawState.h"






bool CGraphicIndexBuffer::Lock(void** pretIndices) const
{
return m_cpuBuffer.Lock(0,0,pretIndices);
}

void CGraphicIndexBuffer::Unlock() const
{
m_cpuBuffer.Unlock();
}

bool CGraphicIndexBuffer::Lock(void** pretIndices)
{
return m_cpuBuffer.Lock(0,0,pretIndices);
}

void CGraphicIndexBuffer::Unlock()
{
m_cpuBuffer.Unlock();
}

bool CGraphicIndexBuffer::Copy(int bufSize, const void* srcIndices)
{
    if (!srcIndices || bufSize<0 || size_t(bufSize)>m_cpuBuffer.Size()) return false;
    void* destination=nullptr; if (!m_cpuBuffer.Lock(0,0,&destination)) return false;
    memcpy(destination,srcIndices,bufSize); return m_cpuBuffer.Unlock();
}

bool CGraphicIndexBuffer::Create(int faceCount, TFace* faces)
{
	int idxCount = faceCount * 3;
	m_iidxCount = idxCount;
	if (!Create(idxCount, Renderer::IndexFormat::UInt16))
		return false;

	WORD* dstIndices;
	if (!Lock((void**)&dstIndices))
		return false;

	for (int i = 0; i < faceCount; ++i, dstIndices += 3)
	{
		TFace* curFace = faces + i;
		dstIndices[0] = curFace->indices[0];
		dstIndices[1] = curFace->indices[1];
		dstIndices[2] = curFace->indices[2];
	}

	Unlock();
	return true;
}

bool CGraphicIndexBuffer::CreateDeviceObjects()
{
return m_cpuBuffer.Create(m_dwBufferSize);
}

void CGraphicIndexBuffer::DestroyDeviceObjects()
{
m_cpuBuffer.Clear();
}

bool CGraphicIndexBuffer::Create(int idxCount, Renderer::IndexFormat indexFormat)
{
	Destroy();

	m_iidxCount = idxCount;
	UINT bytesPerIndex = (indexFormat == Renderer::IndexFormat::UInt32) ? 4u : 2u;
	m_dwBufferSize = bytesPerIndex * idxCount;

	return CreateDeviceObjects();
}

void CGraphicIndexBuffer::Destroy()
{
	DestroyDeviceObjects();
}

void CGraphicIndexBuffer::Initialize()
{
    m_iidxCount=0;
    m_dwBufferSize=0;
}

CGraphicIndexBuffer::CGraphicIndexBuffer()
{
	Initialize();
}

CGraphicIndexBuffer::~CGraphicIndexBuffer()
{
	Destroy();
}
