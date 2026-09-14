#include "StdAfx.h"
#include "Mesh.h"
#include "AssetRuntime/Granny/LegacyVertexTypes.h"
#include "AssetRuntime/Granny/GrannyInterop.h"
#include "Model.h"
#include "Material.h"
#include "Deform.h"
#include <limits>

granny_data_type_definition GrannyPNT3322VertexType[5] =
{
	{GrannyReal32Member, GrannyVertexPositionName, 0, 3},
	{GrannyReal32Member, GrannyVertexNormalName, 0, 3},
	{GrannyReal32Member, GrannyVertexTextureCoordinatesName"0", 0, 2},
	{GrannyReal32Member, GrannyVertexTextureCoordinatesName"1", 0, 2},
	{GrannyEndMember}
};

bool CGrannyMesh::BindAsset(const AssetRuntime::ModelHandle& model, std::size_t mesh)
{
    m_runtimeBound = true;
    m_document = nullptr;
    m_asset = nullptr;
    const auto* asset = model.Get();
    if (!asset || mesh >= asset->meshes.size()) return false;
    m_document = model.GetDocument().get();
    m_asset = &asset->meshes[mesh];
    m_assetModel = model.Index();
    m_assetMesh = mesh;
    return true;
}

bool CGrannyMesh::CreateFromAsset(const AssetRuntime::ModelHandle& model, std::size_t mesh,
    int vertexBase, int indexBase, CGrannyMaterialPalette& palette)
{
    if (!IsEmpty() || vertexBase < 0 || indexBase < 0 || !BindAsset(model, mesh)) return false;
    if (m_asset->topology != AssetRuntime::PrimitiveTopology::TriangleList ||
        m_asset->deformation == AssetRuntime::Deformation::Mixed) return false;
    m_vtxBasePos = vertexBase;
    m_idxBasePos = indexBase;
    // Optional reference-only/native debug interop; metadata and upload require no native pointer.
    m_pgrnMesh = AssetRuntime::GrannyInterop::GetMesh(model, mesh);
    m_canDeformPNTVertex = m_asset->deformation == AssetRuntime::Deformation::Skinned;
    m_isTwoSide = m_asset->twoSided;
    const auto& materials = model.Get()->materials;
    for (const auto index : m_asset->materialBindings) {
        if (index >= materials.size()) return false;
        const auto slot = palette.RegisterMaterial(materials[index]);
        m_mtrlIndexVector.push_back(slot);
        m_bHaveBlendThing |= palette.GetMaterialRef(slot).GetType() == CGrannyMaterial::TYPE_BLEND_PNT;
    }
    if (m_mtrlIndexVector.empty() || m_asset->materialGroups.empty()) return true;
    if (m_asset->materialGroups.size() > std::size_t(std::numeric_limits<int>::max())) return false;
    m_triGroupNodes = new TTriGroupNode[m_asset->materialGroups.size()];
    for (std::size_t group = 0; group < m_asset->materialGroups.size(); ++group) {
        const auto& source = m_asset->materialGroups[group];
        if (source.firstIndex > m_asset->indexCount || source.indexCount > m_asset->indexCount - source.firstIndex ||
            source.indexCount % 3 != 0 || source.firstIndex > std::uint32_t(std::numeric_limits<int>::max() - indexBase)) return false;
        auto& node = m_triGroupNodes[group];
        node.idxPos = indexBase + static_cast<int>(source.firstIndex);
        node.triCount = static_cast<int>(source.indexCount / 3);
        node.mtrlIndex = source.materialIndex < m_mtrlIndexVector.size() ? m_mtrlIndexVector[source.materialIndex] : 0;
        if (node.mtrlIndex >= palette.GetMaterialCount()) return false;
        const auto type = palette.GetMaterialRef(node.mtrlIndex).GetType();
        node.pNextTriGroupNode = m_triGroupNodeLists[type];
        m_triGroupNodeLists[type] = &node;
    }
    return true;
}

bool CGrannyMesh::LoadIndices(void * dstBaseIndices)
{
    if (m_runtimeBound) {
        if (!m_document || !m_asset) return false;
        if (!m_asset->indexCount) return true;
        if (!dstBaseIndices) return false;
        static_assert(sizeof(TIndex) == 2 || sizeof(TIndex) == 4);
        const auto width = sizeof(TIndex) == 2 ? AssetRuntime::IndexWidth::UInt16 : AssetRuntime::IndexWidth::UInt32;
        TIndex* dstIndices = static_cast<TIndex*>(dstBaseIndices) + m_idxBasePos;
        const auto error = m_document->CopyIndices(m_assetModel, m_assetMesh, width,
            {reinterpret_cast<std::byte*>(dstIndices), std::size_t(m_asset->indexCount) * sizeof(TIndex)});
        if (error != AssetRuntime::AssetError::None)
            TraceError("Asset Runtime index upload: mesh=%zu error=%s", m_assetMesh, AssetRuntime::ErrorName(error));
        return error == AssetRuntime::AssetError::None;
    }

	const granny_mesh * pgrnMesh = GetGrannyMeshPointer();
	TIndex * dstIndices = ((TIndex *)dstBaseIndices) + m_idxBasePos;
	GrannyCopyMeshIndices(pgrnMesh, sizeof(TIndex), dstIndices);
	return true;
}

bool CGrannyMesh::LoadPNTVertices(void * dstBaseVertices)
{
    if (m_runtimeBound) {
        if (!m_document || !m_asset) return false;
        if (m_asset->deformation != AssetRuntime::Deformation::Rigid || !m_asset->vertexCount) return true;
        if (!dstBaseVertices) return false;
        // Preserve the existing base-vertex arithmetic, including the legacy UV2 path.
        TPNTVertex* dstVertices = static_cast<TPNTVertex*>(dstBaseVertices) + m_vtxBasePos;
        const auto error = m_document->CopyVertices(m_assetModel, m_assetMesh, m_uploadLayout,
            {reinterpret_cast<std::byte*>(dstVertices), std::size_t(m_asset->vertexCount) * AssetRuntime::VertexStride(m_uploadLayout)});
        if (error != AssetRuntime::AssetError::None)
            TraceError("Asset Runtime vertex upload: mesh=%zu error=%s", m_assetMesh, AssetRuntime::ErrorName(error));
        return error == AssetRuntime::AssetError::None;
    }

	const granny_mesh * pgrnMesh = GetGrannyMeshPointer();

	if (!GrannyMeshIsRigid(pgrnMesh))
		return true;

	TPNTVertex * dstVertices = ((TPNTVertex *)dstBaseVertices) + m_vtxBasePos;
	GrannyCopyMeshVertices(pgrnMesh, m_pgrnMeshType, dstVertices);
	return true;
}

bool CGrannyMesh::NEW_LoadVertices(void * dstBaseVertices)
{
	return LoadPNTVertices(dstBaseVertices);
}

bool CGrannyMesh::DeformPNTVertices(void* dstBaseVertices, AssetRuntime::PoseView pose, AssetRuntime::MeshBinding& binding) const
{
    const int count = GetVertexCount();
    if (!count) return true;
    if (!dstBaseVertices || !pose.Valid() || count < 0) return false;
    auto* destination = static_cast<TPNTVertex*>(dstBaseVertices) + m_vtxBasePos;
    extern bool CPU_HAS_SSE2;
    const auto error = binding.DeformVertices(
        {reinterpret_cast<std::byte*>(destination), std::size_t(count) * sizeof(TPNTVertex)}, pose.values, CPU_HAS_SSE2);
    if (error != AssetRuntime::AssetError::None)
        TraceError("Asset Runtime CPU deformation: mesh=%zu error=%s", m_assetMesh, AssetRuntime::ErrorName(error));
    return error == AssetRuntime::AssetError::None;
}

bool CGrannyMesh::CanDeformPNTVertices() const
{
	return m_canDeformPNTVertex;
}

const granny_mesh * CGrannyMesh::GetGrannyMeshPointer() const
{
	return m_pgrnMesh;
}

const CGrannyMesh::TTriGroupNode * CGrannyMesh::GetTriGroupNodeList(CGrannyMaterial::EType eMtrlType) const
{
	return m_triGroupNodeLists[eMtrlType];
}

int CGrannyMesh::GetVertexCount() const
{
	if (m_asset) return static_cast<int>(m_asset->vertexCount);
	assert(m_pgrnMesh!=NULL);
	return GrannyGetMeshVertexCount(m_pgrnMesh);
}

int CGrannyMesh::GetVertexBasePosition() const
{
	return m_vtxBasePos;
}

int CGrannyMesh::GetIndexBasePosition() const
{
	return m_idxBasePos;
}

// WORK
int * CGrannyMesh::GetDefaultBoneIndices() const
{
    return m_pgrnMeshBindingTemp ? (int*)GrannyGetMeshBindingToBoneIndices(m_pgrnMeshBindingTemp) : nullptr;
}
// END_OF_WORK

bool CGrannyMesh::IsEmpty() const
{
	if (m_asset || m_pgrnMesh)
		return false;

	return true;
}

bool CGrannyMesh::CreateFromGrannyMeshPointer(granny_skeleton * pgrnSkeleton, granny_mesh * pgrnMesh, int vtxBasePos, int idxBasePos, CGrannyMaterialPalette& rkMtrlPal)
{
	assert(IsEmpty());

	m_pgrnMesh = pgrnMesh;
	m_vtxBasePos = vtxBasePos;
	m_idxBasePos = idxBasePos;

	if (m_pgrnMesh->BoneBindingCount < 0)
		return true;

	// WORK
	m_pgrnMeshBindingTemp = GrannyNewMeshBinding(m_pgrnMesh, pgrnSkeleton, pgrnSkeleton);	
	// END_OF_WORK

	if (!GrannyMeshIsRigid(m_pgrnMesh))
	{
		m_canDeformPNTVertex = true;

		granny_data_type_definition * pgrnInputType = GrannyGetMeshVertexType(m_pgrnMesh);
		granny_data_type_definition * pgrnOutputType = m_pgrnMeshType;

		m_pgrnMeshDeformer = GrannyNewMeshDeformer(pgrnInputType, pgrnOutputType, GrannyDeformPositionNormal, GrannyAllowUncopiedTail);
		assert(m_pgrnMeshDeformer != NULL && "Cannot create mesh deformer");
	}

	// Two Side Mesh
	if (!strncmp(m_pgrnMesh->Name, "2x", 2))
		m_isTwoSide = true;

	if (!LoadMaterials(rkMtrlPal))
		return false;

	if (!LoadTriGroupNodeList(rkMtrlPal))
		return false;

	return true;
}

bool CGrannyMesh::LoadTriGroupNodeList(CGrannyMaterialPalette& rkMtrlPal)
{
	assert(m_pgrnMesh != NULL);
	assert(m_triGroupNodes == NULL);

	int mtrlCount		= m_pgrnMesh->MaterialBindingCount;
	if (mtrlCount <= 0) // 천의 동굴 2층 크래쉬 발생
		return true;

	int GroupNodeCount	= GrannyGetMeshTriangleGroupCount(m_pgrnMesh);
	if (GroupNodeCount <= 0)
		return true;

	m_triGroupNodes		= new TTriGroupNode[GroupNodeCount];

	const granny_tri_material_group * c_pgrnTriGroups = GrannyGetMeshTriangleGroups(m_pgrnMesh);

	for (int g = 0; g < GroupNodeCount; ++g)
	{
		const granny_tri_material_group & c_rgrnTriGroup = c_pgrnTriGroups[g];
		TTriGroupNode * pTriGroupNode = m_triGroupNodes + g;

		pTriGroupNode->idxPos = m_idxBasePos + c_rgrnTriGroup.TriFirst * 3;
		pTriGroupNode->triCount = c_rgrnTriGroup.TriCount;
		
		int iMtrl = c_rgrnTriGroup.MaterialIndex;		
		if (iMtrl < 0 || iMtrl >= mtrlCount)
		{
			pTriGroupNode->mtrlIndex=0;//m_mtrlIndexVector[iMtrl];			
		}
		else
		{	
			pTriGroupNode->mtrlIndex=m_mtrlIndexVector[iMtrl];
		}

		const CGrannyMaterial& rkMtrl=rkMtrlPal.GetMaterialRef(pTriGroupNode->mtrlIndex);
		pTriGroupNode->pNextTriGroupNode		= m_triGroupNodeLists[rkMtrl.GetType()];
		m_triGroupNodeLists[rkMtrl.GetType()]	= pTriGroupNode;

	}

	return true;
}

void CGrannyMesh::RebuildTriGroupNodeList()
{
	assert(!"CGrannyMesh::RebuildTriGroupNodeList() - should not be called");
}

bool CGrannyMesh::LoadMaterials(CGrannyMaterialPalette& rkMtrlPal)
{
	assert(m_pgrnMesh != NULL);
	
	if (m_pgrnMesh->MaterialBindingCount <= 0)
		return true;

	int mtrlCount = m_pgrnMesh->MaterialBindingCount;
	bool bHaveBlendThing = false;
	
	for (int m = 0; m < mtrlCount; ++m)
	{
		granny_material* pgrnMaterial = m_pgrnMesh->MaterialBindings[m].Material;
		DWORD mtrlIndex=rkMtrlPal.RegisterMaterial(pgrnMaterial);
		m_mtrlIndexVector.push_back(mtrlIndex);	
		bHaveBlendThing |= rkMtrlPal.GetMaterialRef(mtrlIndex).GetType() == CGrannyMaterial::TYPE_BLEND_PNT;
	}
	m_bHaveBlendThing = bHaveBlendThing;

	return true;
}

bool CGrannyMesh::IsTwoSide() const
{
	return m_isTwoSide;
}

void CGrannyMesh::SetPNT2Mesh()
{
	m_pgrnMeshType = GrannyPNT3322VertexType;
	m_uploadLayout = AssetRuntime::VertexLayout::PositionNormalUV2;
}

void CGrannyMesh::Destroy()
{
	if (m_triGroupNodes)
		delete [] m_triGroupNodes;

	m_mtrlIndexVector.clear();

	// WORK
	if (m_pgrnMeshBindingTemp) 
		GrannyFreeMeshBinding(m_pgrnMeshBindingTemp);
	// END_OF_WORK

    if (m_pgrnMeshDeformer)
		GrannyFreeMeshDeformer(m_pgrnMeshDeformer); 	
	
	Initialize();
}

void CGrannyMesh::Initialize()
{
	m_document = nullptr;
	m_asset = nullptr;
	m_assetModel = m_assetMesh = 0;
	m_runtimeBound = false;
	m_uploadLayout = AssetRuntime::VertexLayout::PositionNormalUV;
	for (int r = 0; r < CGrannyMaterial::TYPE_MAX_NUM; ++r)
		m_triGroupNodeLists[r] = NULL;

	m_pgrnMeshType = GrannyPNT332VertexType;
	m_pgrnMesh = NULL;
	// WORK
	m_pgrnMeshBindingTemp = NULL;
	// END_OF_WORK
	m_pgrnMeshDeformer = NULL;

	m_triGroupNodes = NULL;	
	
	m_vtxBasePos = 0;
	m_idxBasePos = 0;

	m_canDeformPNTVertex = false;
	m_isTwoSide = false;
	m_bHaveBlendThing = false;
}

CGrannyMesh::CGrannyMesh()
{
	Initialize();
}

CGrannyMesh::~CGrannyMesh()
{
	Destroy();
}
