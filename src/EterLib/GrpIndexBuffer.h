#pragma once

#include "GrpBase.h"
#include "Renderer/ResourceData.h"

class CGraphicIndexBuffer : public CGraphicBase
{
public:
	CGraphicIndexBuffer();
	virtual ~CGraphicIndexBuffer();

	void Destroy();
	bool Create(int idxCount, Renderer::IndexFormat indexFormat);
	bool Create(int faceCount, TFace* faces);

	bool CreateDeviceObjects();
	void DestroyDeviceObjects();

	bool Copy(int bufSize, const void* srcIndices);

	bool Lock(void** pretIndices) const;
	void Unlock() const;

	bool Lock(void** pretIndices);
	void Unlock();





	int GetIndexCount() const { return m_iidxCount; }

protected:
	void Initialize();

protected:
        mutable Renderer::CpuBuffer m_cpuBuffer;

	DWORD					m_dwBufferSize;
	int						m_iidxCount;
};
