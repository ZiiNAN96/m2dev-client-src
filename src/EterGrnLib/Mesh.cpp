#include "StdAfx.h"
#include "Mesh.h"
#include "Model.h"
#include "Material.h"
#include <limits>


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
    m_canDeformPNTVertex = m_asset->deformation == AssetRuntime::Deformation::Skinned;
    m_isTwoSide = m_asset->twoSided;
    const auto& materials = model.Get()->materials;
    for (const auto index : m_asset->materialBindings) {
        if (index >= materials.size()) return false;
        const auto slot = palette.RegisterMaterial(materials[index]);
        if (slot == CGrannyMaterialPalette::InvalidMaterial) return false;
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

bool CGrannyMesh::LoadIndices(void * dstBaseIndices, AssetRuntime::IndexWidth width)
{
    if (width != AssetRuntime::IndexWidth::UInt16 && width != AssetRuntime::IndexWidth::UInt32) return false;
    const std::size_t stride = width == AssetRuntime::IndexWidth::UInt32 ? 4 : 2;
    if (m_runtimeBound) {
        if (!m_document || !m_asset) return false;
        if (!m_asset->indexCount) return true;
        if (!dstBaseIndices) return false;
        if (m_idxBasePos < 0 || std::size_t(m_idxBasePos) > std::numeric_limits<std::size_t>::max() / stride ||
            std::size_t(m_asset->indexCount) > std::numeric_limits<std::size_t>::max() / stride) return false;
        auto* dstIndices = static_cast<std::byte*>(dstBaseIndices) + std::size_t(m_idxBasePos) * stride;
        const auto error = m_document->CopyIndices(m_assetModel, m_assetMesh, width,
            {dstIndices, std::size_t(m_asset->indexCount) * stride});
        if (error != AssetRuntime::AssetError::None)
            TraceError("Asset Runtime index upload: mesh=%zu error=%s", m_assetMesh, AssetRuntime::ErrorName(error));
        return error == AssetRuntime::AssetError::None;
    }

    return false;
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

    return false;
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


const CGrannyMesh::TTriGroupNode * CGrannyMesh::GetTriGroupNodeList(CGrannyMaterial::EType eMtrlType) const
{
	return m_triGroupNodeLists[eMtrlType];
}

int CGrannyMesh::GetVertexCount() const
{
    return m_asset ? static_cast<int>(m_asset->vertexCount) : 0;
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

// END_OF_WORK

bool CGrannyMesh::IsEmpty() const
{
	if (m_asset)
		return false;

	return true;
}


void CGrannyMesh::RebuildTriGroupNodeList()
{
	assert(!"CGrannyMesh::RebuildTriGroupNodeList() - should not be called");
}


bool CGrannyMesh::IsTwoSide() const
{
	return m_isTwoSide;
}

void CGrannyMesh::SetPNT2Mesh()
{
	m_uploadLayout = AssetRuntime::VertexLayout::PositionNormalUV2;
}

void CGrannyMesh::Destroy()
{
	if (m_triGroupNodes)
		delete [] m_triGroupNodes;

	m_mtrlIndexVector.clear();
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

	// WORK
	// END_OF_WORK

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
