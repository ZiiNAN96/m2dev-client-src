#pragma once

#include "GrpBase.h"
#include "Renderer/ResourceData.h"

class CGraphicVertexBuffer : public CGraphicBase
{
	public:
		CGraphicVertexBuffer();
		virtual ~CGraphicVertexBuffer();

		void	Destroy();
		virtual bool	Create(int vtxCount, DWORD fvf);

		bool	CreateDeviceObjects();
		void	DestroyDeviceObjects();

		bool	Copy(int bufSize, const void* srcVertices);

		bool	LockRange(unsigned count, void** pretVertices) const;
		bool	Lock(void** pretVertices) const;
		bool	Unlock() const;

		bool	LockDynamic(void** pretVertices);
		virtual bool	Lock(void** pretVertices);
		bool	Unlock();


			
		int		GetVertexCount() const;
		int		GetVertexStride() const;
		DWORD	GetVertexLayout() const;


		inline	DWORD GetBufferSize() const	{ return m_dwBufferSize; }

		bool	IsEmpty() const;

	protected:
		void	Initialize();

	protected:
        mutable Renderer::CpuBuffer m_cpuBuffer;


		DWORD					m_dwBufferSize;
		DWORD					m_vertexLayout;
		int						m_vtxCount;
};
