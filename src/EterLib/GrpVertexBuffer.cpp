#include "StdAfx.h"
#include "EterBase/Stl.h"
#include "GrpVertexBuffer.h"
#include "DrawState.h"

int	CGraphicVertexBuffer::GetVertexStride() const
{
	int retSize = Renderer::VertexStride(m_vertexLayout);
	return retSize;
}

DWORD CGraphicVertexBuffer::GetVertexLayout() const
{
	return m_vertexLayout;
}

int CGraphicVertexBuffer::GetVertexCount() const
{
	return m_vtxCount;
}



bool CGraphicVertexBuffer::LockRange(unsigned count, void** pretVertices) const
{
return count && m_cpuBuffer.Lock(0,size_t(GetVertexStride())*count,pretVertices);
}

bool CGraphicVertexBuffer::Lock(void ** pretVertices) const
{
return m_cpuBuffer.Lock(0,0,pretVertices);
}

bool CGraphicVertexBuffer::Unlock() const
{
return m_cpuBuffer.Unlock();
}

bool CGraphicVertexBuffer::IsEmpty() const
{
return m_cpuBuffer.Size()==0;
}

bool CGraphicVertexBuffer::LockDynamic(void** pretVertices)
{
return m_cpuBuffer.Lock(0,0,pretVertices);
}

bool CGraphicVertexBuffer::Lock(void ** pretVertices)
{
return m_cpuBuffer.Lock(0,0,pretVertices);
}

bool CGraphicVertexBuffer::Unlock()
{
return m_cpuBuffer.Unlock();
}

bool CGraphicVertexBuffer::Copy(int bufSize, const void* srcVertices)
{
    if (m_cpuBuffer.Size() && (!srcVertices || bufSize<0 || size_t(bufSize)>m_cpuBuffer.Size())) return false;
	void * dstVertices;

	if (!Lock(&dstVertices))
		return false;

	memcpy(dstVertices, srcVertices, bufSize);
	
	Unlock();
	return true;
}

bool CGraphicVertexBuffer::CreateDeviceObjects()
{
return m_cpuBuffer.Create(m_dwBufferSize);
}

void CGraphicVertexBuffer::DestroyDeviceObjects()
{
m_cpuBuffer.Clear();
}

bool CGraphicVertexBuffer::Create(int vtxCount, DWORD fvf)
{
	assert(vtxCount > 0);

	Destroy();

	m_vtxCount = vtxCount;
	m_dwBufferSize = Renderer::VertexStride(fvf) * m_vtxCount;
	m_vertexLayout = fvf;


	return CreateDeviceObjects();
}

void CGraphicVertexBuffer::Destroy()
{
	DestroyDeviceObjects();
}

void CGraphicVertexBuffer::Initialize()
{

	m_vtxCount = 0;
	m_dwBufferSize = 0;
}

CGraphicVertexBuffer::CGraphicVertexBuffer()
{
	Initialize();
}

CGraphicVertexBuffer::~CGraphicVertexBuffer()
{
	Destroy();
}
