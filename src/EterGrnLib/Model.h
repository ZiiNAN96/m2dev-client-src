#pragma once

#include "Eterlib/GrpVertexBuffer.h"
#include "Eterlib/GrpIndexBuffer.h"

#include "Mesh.h"
#include "Renderer/StaticObjectRenderData.h"
#include "Renderer/ActorRenderData.h" // ZiiNAN: Optional actor index snapshot.
#include "Renderer/SkinningData.h"
#include "AssetRuntime/AssetRuntime.h"

class CGrannyModel : public CReferenceObject
{
	public:
		typedef struct SMeshNode
		{
			int					iMesh;
			const CGrannyMesh * pMesh;
			SMeshNode *			pNextMeshNode;
		} TMeshNode;

	public:
		CGrannyModel();
		virtual ~CGrannyModel();

		bool IsEmpty() const;
        bool CreateFromAsset(AssetRuntime::ModelHandle asset);
        const AssetRuntime::ModelAsset* GetAsset() const { return m_asset.Get(); }
        const AssetRuntime::ModelHandle& GetAssetHandle() const { return m_asset; }
        AssetRuntime::SkinningStreamView GetSkinningView(size_t mesh) const;
		bool CreateDeviceObjects();
		void DestroyDeviceObjects();
		void Destroy();

		int GetRigidVertexCount() const;
        size_t GetRigidVertexBytes() const { return m_pntVtxBuf.GetBufferSize(); }
		int GetDeformVertexCount() const;
		int GetVertexCount() const;

		bool CanDeformPNTVertices() const;
		bool DeformPNTVertices(void* dstBaseVertices, AssetRuntime::PoseView pose,
			const std::vector<std::unique_ptr<AssetRuntime::MeshBinding>>& bindings) const;

		int GetIdxCount();
        AssetRuntime::IndexWidth GetIndexWidth() const { return m_indexWidth; }
		int GetMeshCount() const;
		CGrannyMesh * GetMeshPointer(int iMesh);
		const CGrannyMesh* GetMeshPointer(int iMesh) const;


		const CGrannyModel::TMeshNode*  GetMeshNodeList(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType) const;

		bool LockVertices(void** indicies, void** vertices) const;
		void UnlockVertices() const;

		const CGrannyMaterialPalette& GetMaterialPalette() const;
        bool CaptureStaticObjectSource();
        const std::shared_ptr<const Renderer::StaticObjectSource>& GetStaticObjectSource() const { return m_staticObjectSource; }
        // ZiiNAN: Capture immutable deformable indices before GPU upload.
        bool CaptureActorSource(bool attachment = false);
        const std::shared_ptr<const Renderer::ActorModelSource>& GetActorSource() const { return m_actorSource; }
        const std::shared_ptr<const Renderer::SkinningModelData>& GetSkinningData() const { return m_skinningData; }

	protected:
		bool LoadAssetMeshes();
		bool LoadPNTVertices();
		bool LoadIndices();
		void Initialize();

		BOOL CheckMeshIndex(int iIndex) const;
		void AppendMeshNode(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType, int iMesh);

	protected:
		// Animation Data

		// Static Data
		CGrannyMesh *			m_meshs;

		CGraphicVertexBuffer	m_pntVtxBuf;	// for rigid mesh
		CGraphicIndexBuffer		m_idxBuf;

		TMeshNode *				m_meshNodes;
		TMeshNode *				m_meshNodeLists[CGrannyMesh::TYPE_MAX_NUM][CGrannyMaterial::TYPE_MAX_NUM];

		int						m_deformVtxCount;
		int						m_rigidVtxCount;
		int						m_vtxCount;
		int						m_idxCount;

		int						m_meshNodeSize;
		int						m_meshNodeCapacity;

		bool					m_canDeformPNVertices;
		
		CGrannyMaterialPalette	m_kMtrlPal;
	private:
        AssetRuntime::ModelHandle m_asset;
        AssetRuntime::IndexWidth m_indexWidth{AssetRuntime::IndexWidth::UInt16};
		bool					m_bHaveBlendThing;
        std::shared_ptr<const Renderer::StaticObjectSource> m_staticObjectSource;
        std::shared_ptr<const Renderer::ActorModelSource> m_actorSource; // ZiiNAN: No bones or animation copies.
        std::shared_ptr<const Renderer::SkinningModelData> m_skinningData;
	public:
		bool					HaveBlendThing() { return m_bHaveBlendThing; }
	
	//////////////////////////////////////////////////////////////////////////
	// New members to support PNT2 type models
	protected:
		bool __LoadVertices();
	protected:
		DWORD m_vertexLayout;
	// New members to support PNT2 type models
	//////////////////////////////////////////////////////////////////////////

};
