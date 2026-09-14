#pragma once

#include "Material.h"

class CGrannyMesh
{
	public:
		enum EType
		{
			TYPE_RIGID,
			TYPE_DEFORM,
			TYPE_MAX_NUM
		};

		typedef struct STriGroupNode
		{
			STriGroupNode *		pNextTriGroupNode;
			int					idxPos;
			int					triCount;
			DWORD				mtrlIndex;						
		} TTriGroupNode;

	public:
		CGrannyMesh();
		virtual ~CGrannyMesh();

		bool					IsEmpty() const;
		bool BindAsset(const AssetRuntime::ModelHandle& model, std::size_t mesh);
		bool CreateFromAsset(const AssetRuntime::ModelHandle& model, std::size_t mesh,
			int vertexBase, int indexBase, CGrannyMaterialPalette& palette);
		const AssetRuntime::MeshAsset* GetAsset() const { return m_asset; }
		bool					CreateFromGrannyMeshPointer(granny_skeleton* pgrnSkeleton, granny_mesh* pgrnMesh, int vtxBasePos, int idxBasePos, CGrannyMaterialPalette& rkMtrlPal);			
		bool LoadIndices(void* dstBaseIndices, AssetRuntime::IndexWidth width = AssetRuntime::IndexWidth::UInt16);
		bool					LoadPNTVertices(void* dstBaseVertices);
		bool					NEW_LoadVertices(void* dstBaseVertices);
		void					Destroy();

		void					SetPNT2Mesh();

		bool DeformPNTVertices(void* dstBaseVertices, AssetRuntime::PoseView pose, AssetRuntime::MeshBinding& binding) const;
		bool					CanDeformPNTVertices() const;
		bool					IsTwoSide() const;

		int						GetVertexCount() const;
		
		// WORK
		int *					GetDefaultBoneIndices() const;
		// END_OF_WORK

		int						GetVertexBasePosition() const; 
		int						GetIndexBasePosition() const;

		const granny_mesh *					GetGrannyMeshPointer() const;
		const CGrannyMesh::TTriGroupNode *	GetTriGroupNodeList(CGrannyMaterial::EType eMtrlType) const;

		void					RebuildTriGroupNodeList();
		void					ReloadMaterials();

	protected:
		void					Initialize();

		bool					LoadMaterials(CGrannyMaterialPalette& rkMtrlPal);
		bool					LoadTriGroupNodeList(CGrannyMaterialPalette& rkMtrlPal);

	protected:
		// Granny Mesh Data
		granny_data_type_definition *	m_pgrnMeshType;
		granny_mesh *			m_pgrnMesh;
		
		// WORK
		granny_mesh_binding *	m_pgrnMeshBindingTemp;
		// END_OF_WORK

		granny_mesh_deformer *	m_pgrnMeshDeformer;

		// Granny Material Data
		std::vector<DWORD>		m_mtrlIndexVector;
		
		// TriGroups Data
		TTriGroupNode *			m_triGroupNodes;
		TTriGroupNode *			m_triGroupNodeLists[CGrannyMaterial::TYPE_MAX_NUM];

		int						m_vtxBasePos;
		int						m_idxBasePos;

		bool					m_canDeformPNTVertex;
		bool					m_isTwoSide;
	private:
		// Borrowed from the owning CGrannyModel handle, which is released after its meshes.
		const AssetRuntime::AssetDocument* m_document{};
		const AssetRuntime::MeshAsset* m_asset{};
		std::size_t m_assetModel{}, m_assetMesh{};
		bool m_runtimeBound{};
		AssetRuntime::VertexLayout m_uploadLayout{AssetRuntime::VertexLayout::PositionNormalUV};
		bool						m_bHaveBlendThing;
	public:
		bool						HaveBlendThing() { return m_bHaveBlendThing; }
};
