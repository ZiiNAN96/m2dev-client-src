#include "StdAfx.h"
#include "EterLib/NativeResourceAudit.h"
#include "EterBase/Stl.h"
#include "GrpIndexBuffer.h"
#include "StateManager.h"

LPDIRECT3DINDEXBUFFER9 CGraphicIndexBuffer::GetD3DIndexBuffer() const
{
	assert(m_lpd3dIdxBuf != NULL || m_cpuBuffer.Size());
	return m_lpd3dIdxBuf;
}

void CGraphicIndexBuffer::SetIndices(int startIndex) const
{
	assert(Renderer::UseNeutralResources());
	STATEMANAGER.SetIndices(m_lpd3dIdxBuf, startIndex);
}


bool CGraphicIndexBuffer::Lock(void** pretIndices) const
{
    if (m_cpuBuffer.Size()) return m_cpuBuffer.Lock(0,0,pretIndices);
	assert(m_lpd3dIdxBuf != NULL || m_cpuBuffer.Size());

	if (!m_lpd3dIdxBuf)
		return false;

	if (FAILED(m_lpd3dIdxBuf->Lock(0, 0, pretIndices, 0)))
		return false;

	return true;
}

void CGraphicIndexBuffer::Unlock() const
{
    if (m_cpuBuffer.Size()) { m_cpuBuffer.Unlock(); return; }
	assert(m_lpd3dIdxBuf != NULL || m_cpuBuffer.Size());

	if (!m_lpd3dIdxBuf)
		return;

	m_lpd3dIdxBuf->Unlock();
}

bool CGraphicIndexBuffer::Lock(void** pretIndices)
{
    if (m_cpuBuffer.Size()) return m_cpuBuffer.Lock(0,0,pretIndices);
	assert(m_lpd3dIdxBuf != NULL || m_cpuBuffer.Size());

	if (!m_lpd3dIdxBuf)
		return false;

	if (FAILED(m_lpd3dIdxBuf->Lock(0, 0, pretIndices, 0)))
		return false;

	return true;
}

void CGraphicIndexBuffer::Unlock()
{
    if (m_cpuBuffer.Size()) { m_cpuBuffer.Unlock(); return; }
	assert(m_lpd3dIdxBuf != NULL || m_cpuBuffer.Size());

	if (!m_lpd3dIdxBuf)
		return;

	m_lpd3dIdxBuf->Unlock();
}

bool CGraphicIndexBuffer::Copy(int bufSize, const void* srcIndices)
{
    if (m_cpuBuffer.Size()) {
        if (!srcIndices || bufSize<0 || size_t(bufSize)>m_cpuBuffer.Size()) return false;
        void* destination=nullptr; if (!m_cpuBuffer.Lock(0,0,&destination)) return false;
        memcpy(destination,srcIndices,bufSize); return m_cpuBuffer.Unlock();
    }
	assert(m_lpd3dIdxBuf != NULL || m_cpuBuffer.Size());

	BYTE* dstIndices;
	if (!Lock((void**)&dstIndices))
		return false;

	memcpy(dstIndices, srcIndices, bufSize);

	m_lpd3dIdxBuf->Unlock();

	return true;
}

bool CGraphicIndexBuffer::Create(int faceCount, TFace* faces)
{
	int idxCount = faceCount * 3;
	m_iidxCount = idxCount;
	if (!Create(idxCount, D3DFMT_INDEX16))
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
    // ZiiNAN: Backend-neutral graphics resource ownership
    if (Renderer::UseNeutralResources()) return m_cpuBuffer.Create(m_dwBufferSize);
	if (FAILED(M2_NATIVE_RESOURCE(IndexBuffer, ms_lpd3dDevice->CreateIndexBuffer(
		m_dwBufferSize,
		D3DUSAGE_WRITEONLY,
		m_d3dFmt,
		D3DPOOL_DEFAULT,
		&m_lpd3dIdxBuf,
		NULL))
	))
		return false;

	return true;
}

void CGraphicIndexBuffer::DestroyDeviceObjects()
{
    m_cpuBuffer.Clear();
	safe_release(m_lpd3dIdxBuf);
}

bool CGraphicIndexBuffer::Create(int idxCount, D3DFORMAT d3dFmt)
{
	Destroy();

	m_iidxCount = idxCount;
	UINT bytesPerIndex = (d3dFmt == D3DFMT_INDEX32) ? 4u : 2u;
	m_dwBufferSize = bytesPerIndex * idxCount;
	m_d3dFmt = d3dFmt;

	return CreateDeviceObjects();
}

void CGraphicIndexBuffer::Destroy()
{
	DestroyDeviceObjects();
}

void CGraphicIndexBuffer::Initialize()
{
	m_lpd3dIdxBuf = NULL;
}

CGraphicIndexBuffer::CGraphicIndexBuffer()
{
	Initialize();
}

CGraphicIndexBuffer::~CGraphicIndexBuffer()
{
	Destroy();
}
