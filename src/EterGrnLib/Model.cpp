#include "StdAfx.h"
#include "AssetRuntime/Granny/GrannyInterop.h"
#include "Model.h"
#include "Mesh.h"
#include "SkinningDataAdapter.h"
#include "Renderer/SkinningBenchmark.h"

#include <limits>

const CGrannyMaterialPalette& CGrannyModel::GetMaterialPalette() const
{
	return m_kMtrlPal;
}

const CGrannyModel::TMeshNode* CGrannyModel::GetMeshNodeList(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType) const
{
	return m_meshNodeLists[eMeshType][eMtrlType];
}

CGrannyMesh * CGrannyModel::GetMeshPointer(int iMesh)
{
	assert(CheckMeshIndex(iMesh));
	assert(m_meshs != NULL);

	return m_meshs + iMesh;
}

const CGrannyMesh* CGrannyModel::GetMeshPointer(int iMesh) const
{
	assert(CheckMeshIndex(iMesh));
	assert(m_meshs != NULL);

	return m_meshs + iMesh;
}

bool CGrannyModel::CanDeformPNTVertices() const
{
	return m_canDeformPNVertices;
}

bool CGrannyModel::DeformPNTVertices(void* dstBaseVertices, AssetRuntime::PoseView pose,
    const std::vector<std::unique_ptr<AssetRuntime::MeshBinding>>& bindings) const
{
    // ZiiNAN: GPU skinning production path — counts every native CPU deformation, including non-actor callers.
    ++Renderer::skinningCpuCalls;Renderer::skinningCpuVertices+=GetDeformVertexCount();
	int meshCount = GetMeshCount();

	for (int iMesh = 0; iMesh < meshCount; ++iMesh)
	{
		CGrannyMesh & rMesh = m_meshs[iMesh];
		if (rMesh.CanDeformPNTVertices()) {
            if (std::size_t(iMesh) >= bindings.size() || !bindings[iMesh] ||
                !rMesh.DeformPNTVertices(dstBaseVertices, pose, *bindings[iMesh])) return false;
        }
	}
	return true;
}

int CGrannyModel::GetRigidVertexCount() const
{
	return m_rigidVtxCount;
}

int CGrannyModel::GetDeformVertexCount() const
{
	return m_deformVtxCount;
}

int CGrannyModel::GetVertexCount() const
{
	return m_vtxCount;
}

int CGrannyModel::GetMeshCount() const
{
	if (const auto* asset = GetAsset()) return static_cast<int>(asset->meshes.size());
	return m_pgrnModel ? m_pgrnModel->MeshBindingCount : 0;
}

AssetRuntime::SkinningStreamView CGrannyModel::GetSkinningView(size_t mesh) const
{
    if (!m_skinningData || mesh >= m_skinningData->meshes.size() || !m_skinningData->meshes[mesh]) return {};
    const auto& data = *m_skinningData->meshes[mesh];
    return {std::as_bytes(std::span(data.vertices)), sizeof(Renderer::SkinningVertex),
        offsetof(Renderer::SkinningVertex, weights), offsetof(Renderer::SkinningVertex, indices),
        data.meshToSourceSkeleton, data.indices};
}

granny_model* CGrannyModel::GetGrannyModelPointer()
{
	return m_pgrnModel;
}





bool CGrannyModel::LockVertices(void** indicies, void** vertices) const
{
	if (!m_idxBuf.Lock(indicies))
		return false;

	if (!m_pntVtxBuf.Lock(vertices))
	{
		m_idxBuf.Unlock();
		return false;
	}

	return true;
}

void CGrannyModel::UnlockVertices() const
{
	m_idxBuf.Unlock();
	m_pntVtxBuf.Unlock();
}

bool CGrannyModel::LoadPNTVertices()
{
	if (m_rigidVtxCount <= 0)
		return true;

	assert(m_meshs != NULL);

	const auto stride = Renderer::VertexStride(m_vertexLayout);
    if (!stride || std::uint32_t(m_rigidVtxCount) > std::uint32_t(std::numeric_limits<int>::max()) / stride) return false;
	if (!m_pntVtxBuf.Create(m_rigidVtxCount, m_vertexLayout))
		return false;

	void* vertices;
	if (!m_pntVtxBuf.Lock(&vertices))
		return false;

	for (int m = 0; m < GetMeshCount(); ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		if (!rMesh.LoadPNTVertices(vertices)) { m_pntVtxBuf.Unlock(); return false; }
	}

	m_pntVtxBuf.Unlock();
	return true;
}

bool CGrannyModel::LoadIndices()
{
	//assert(m_idxCount > 0);
	if (m_idxCount <= 0)
		return true;

    const bool wide = m_indexWidth == AssetRuntime::IndexWidth::UInt32;
    if (std::size_t(m_idxCount) > std::numeric_limits<std::uint32_t>::max() / (wide ? 4u : 2u)) return false;
	if (!m_idxBuf.Create(m_idxCount, wide ? Renderer::IndexFormat::UInt32 : Renderer::IndexFormat::UInt16))
		return false;

	void * indices;

	if (!m_idxBuf.Lock((void**)&indices))
		return false;

	for (int m = 0; m < GetMeshCount(); ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		if (!rMesh.LoadIndices(indices, m_indexWidth)) { m_idxBuf.Unlock(); return false; }
	}

	m_idxBuf.Unlock();	
	return true;
}

bool CGrannyModel::LoadMeshs()
{
	assert(m_meshs == NULL);
	assert(m_pgrnModel != NULL);

	if (m_pgrnModel->MeshBindingCount <= 0)	// 메쉬가 없는 모델
		return true;

	granny_skeleton * pgrnSkeleton = m_pgrnModel->Skeleton;

	int vtxRigidPos = 0;
	int vtxDeformPos = 0;
	int vtxPos = 0;
	int idxPos = 0;

	int diffusePNTMeshNodeCount = 0;
	int blendPNTMeshNodeCount = 0;
	int blendPNT2MeshNodeCount = 0;

	int meshCount = GetMeshCount();
	m_meshs = new CGrannyMesh[meshCount];

	m_vertexLayout = 0;

	for (int m = 0; m < meshCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		granny_mesh* pgrnMesh = m_pgrnModel->MeshBindings[m].Mesh;
        if (m_asset && !rMesh.BindAsset(m_asset, m)) return false;
        const auto* meshAsset = rMesh.GetAsset();
		const int vertexCount = meshAsset ? static_cast<int>(meshAsset->vertexCount) : GrannyGetMeshVertexCount(pgrnMesh);
		const int indexCount = meshAsset ? static_cast<int>(meshAsset->indexCount) : GrannyGetMeshIndexCount(pgrnMesh);
		// ZiiNAN: 64-bit safety cleanup
		if (vertexCount < 0 || indexCount < 0 ||
			vertexCount > std::numeric_limits<int>::max() - vtxPos ||
			indexCount > std::numeric_limits<int>::max() - idxPos)
			return false;

		if (GrannyMeshIsRigid(pgrnMesh))
		{
			if (vertexCount > std::numeric_limits<int>::max() - vtxRigidPos)
				return false;
			if (!rMesh.CreateFromGrannyMeshPointer(pgrnSkeleton, pgrnMesh, vtxRigidPos, idxPos, m_kMtrlPal))
				return false;

			vtxRigidPos += vertexCount;
		}
		else
		{
			if (vertexCount > std::numeric_limits<int>::max() - vtxDeformPos)
				return false;
			if (!rMesh.CreateFromGrannyMeshPointer(pgrnSkeleton, pgrnMesh, vtxDeformPos, idxPos, m_kMtrlPal))
				return false;

			vtxDeformPos += vertexCount;
			m_canDeformPNVertices |= rMesh.CanDeformPNTVertices();
		}
		m_bHaveBlendThing |= rMesh.HaveBlendThing();

		for (int i = 0; pgrnMesh->PrimaryVertexData->VertexType[i].Name != nullptr; ++i)
		{
			if ( 0 == strcmp(pgrnMesh->PrimaryVertexData->VertexType[i].Name, GrannyVertexPositionName) )
				m_vertexLayout |= Renderer::VertexPosition;
			else if ( 0 == strcmp(pgrnMesh->PrimaryVertexData->VertexType[i].Name, GrannyVertexNormalName) )
				m_vertexLayout |= Renderer::VertexNormal;
			else if ( 0 == strcmp(pgrnMesh->PrimaryVertexData->VertexType[i].Name, GrannyVertexTextureCoordinatesName"0") )
				m_vertexLayout |= Renderer::VertexTex1;
			else if ( 0 == strcmp(pgrnMesh->PrimaryVertexData->VertexType[i].Name, GrannyVertexTextureCoordinatesName"1") )
				m_vertexLayout |= Renderer::VertexTex2;
		}

		vtxPos += vertexCount;
		idxPos += indexCount;

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT))
			++diffusePNTMeshNodeCount;

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_BLEND_PNT))
			++blendPNTMeshNodeCount;
	}

	if (diffusePNTMeshNodeCount > std::numeric_limits<int>::max() - blendPNTMeshNodeCount ||
		diffusePNTMeshNodeCount + blendPNTMeshNodeCount > std::numeric_limits<int>::max() - blendPNT2MeshNodeCount)
		return false;
	m_meshNodeCapacity = diffusePNTMeshNodeCount + blendPNTMeshNodeCount + blendPNT2MeshNodeCount;
	m_meshNodes = new TMeshNode[m_meshNodeCapacity];

	for (int n = 0; n < meshCount; ++n)
	{
		CGrannyMesh& rMesh = m_meshs[n];
		granny_mesh* pgrnMesh = m_pgrnModel->MeshBindings[n].Mesh;

		CGrannyMesh::EType eMeshType = GrannyMeshIsRigid(pgrnMesh) ? CGrannyMesh::TYPE_RIGID : CGrannyMesh::TYPE_DEFORM;

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT))
			AppendMeshNode(eMeshType, CGrannyMaterial::TYPE_DIFFUSE_PNT, n);

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_BLEND_PNT))
			AppendMeshNode(eMeshType, CGrannyMaterial::TYPE_BLEND_PNT, n);
	}

	// For Dungeon Block
	if ((Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1|Renderer::VertexTex2) == m_vertexLayout)
	{
		for (int n = 0; n < meshCount; ++n)
		{
			CGrannyMesh& rMesh = m_meshs[n];
			rMesh.SetPNT2Mesh();
		}
	}

	m_rigidVtxCount = vtxRigidPos;
	m_deformVtxCount = vtxDeformPos;

	m_vtxCount = vtxPos;
	m_idxCount = idxPos;
	return true;
}

bool CGrannyModel::LoadAssetMeshes()
{
    const auto* asset = GetAsset();
    if (!asset || asset->meshes.size() > std::size_t(std::numeric_limits<int>::max()) ||
        asset->materials.size() > std::size_t(std::numeric_limits<DWORD>::max())) return false;
    const int meshCount = static_cast<int>(asset->meshes.size());
    if (!meshCount) return true;
    m_meshs = new CGrannyMesh[meshCount];
    int rigidVertices = 0, deformVertices = 0, vertices = 0, indices = 0;
    int diffuseNodes = 0, blendNodes = 0;
    for (int index = 0; index < meshCount; ++index) {
        const auto& source = asset->meshes[index];
        if (source.indexCount && source.indexWidth != AssetRuntime::IndexWidth::UInt16 &&
            source.indexWidth != AssetRuntime::IndexWidth::UInt32) return false;
        // Keep the productive Granny stream unchanged; neutral rigid assets retain their declared width.
        if (!m_pgrnModel && source.indexWidth == AssetRuntime::IndexWidth::UInt32)
            m_indexWidth = AssetRuntime::IndexWidth::UInt32;
        if (source.vertexCount > std::uint32_t(std::numeric_limits<int>::max() - vertices) ||
            source.indexCount > std::uint32_t(std::numeric_limits<int>::max() - indices) ||
            source.deformation == AssetRuntime::Deformation::Mixed ||
            source.topology != AssetRuntime::PrimitiveTopology::TriangleList) return false;
        constexpr std::uint32_t required = AssetRuntime::Position | AssetRuntime::Normal | AssetRuntime::UV0;
        auto attributes = source.vertexAttributes;
        if (!attributes) {
            if (source.vertexLayout == AssetRuntime::VertexLayout::PositionNormalUV ||
                source.vertexLayout == AssetRuntime::VertexLayout::WeightedPositionNormalUV) attributes = required;
            else if (source.vertexLayout == AssetRuntime::VertexLayout::PositionNormalUV2) attributes = required | AssetRuntime::UV1;
        }
        if ((attributes & required) != required || (attributes & ~(required | AssetRuntime::UV1)) != 0) {
            TraceError("Asset Runtime unsupported model vertex layout: model=%s mesh=%d", asset->name.c_str(), index);
            return false;
        }
        DWORD layout = Renderer::VertexPosition | Renderer::VertexNormal | Renderer::VertexTex1;
        if (attributes & AssetRuntime::UV1) layout |= Renderer::VertexTex2;
        m_vertexLayout |= layout;
        const bool rigid = source.deformation == AssetRuntime::Deformation::Rigid;
        auto& mesh = m_meshs[index];
        if (!mesh.CreateFromAsset(m_asset, index, rigid ? rigidVertices : deformVertices, indices, m_kMtrlPal)) return false;
        if (rigid) rigidVertices += static_cast<int>(source.vertexCount);
        else {
            deformVertices += static_cast<int>(source.vertexCount);
            m_canDeformPNVertices |= mesh.CanDeformPNTVertices();
        }
        vertices += static_cast<int>(source.vertexCount);
        indices += static_cast<int>(source.indexCount);
        m_bHaveBlendThing |= mesh.HaveBlendThing();
        if (mesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT)) ++diffuseNodes;
        if (mesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_BLEND_PNT)) ++blendNodes;
    }
    if (diffuseNodes > std::numeric_limits<int>::max() - blendNodes) return false;
    m_meshNodeCapacity = diffuseNodes + blendNodes;
    m_meshNodes = new TMeshNode[m_meshNodeCapacity];
    for (int index = 0; index < meshCount; ++index) {
        auto& mesh = m_meshs[index];
        const auto type = asset->meshes[index].deformation == AssetRuntime::Deformation::Rigid ?
            CGrannyMesh::TYPE_RIGID : CGrannyMesh::TYPE_DEFORM;
        if (mesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT))
            AppendMeshNode(type, CGrannyMaterial::TYPE_DIFFUSE_PNT, index);
        if (mesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_BLEND_PNT))
            AppendMeshNode(type, CGrannyMaterial::TYPE_BLEND_PNT, index);
        if (m_vertexLayout == (Renderer::VertexPosition | Renderer::VertexNormal | Renderer::VertexTex1 | Renderer::VertexTex2))
            mesh.SetPNT2Mesh();
    }
    m_rigidVtxCount = rigidVertices;
    m_deformVtxCount = deformVertices;
    m_vtxCount = vertices;
    m_idxCount = indices;
    return true;
}

BOOL CGrannyModel::CheckMeshIndex(int iIndex) const
{
	if (iIndex < 0)
		return FALSE;
	if (iIndex >= GetMeshCount())
		return FALSE;

	return TRUE;
}

void CGrannyModel::AppendMeshNode(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType, int iMesh)
{
	assert(m_meshNodeSize < m_meshNodeCapacity);

	TMeshNode& rMeshNode = m_meshNodes[m_meshNodeSize++];

	rMeshNode.iMesh = iMesh;
	rMeshNode.pMesh = m_meshs + iMesh;
	rMeshNode.pNextMeshNode = m_meshNodeLists[eMeshType][eMtrlType];
	m_meshNodeLists[eMeshType][eMtrlType] = &rMeshNode;
}

bool CGrannyModel::CreateFromAsset(AssetRuntime::ModelHandle asset)
{
    if (!IsEmpty() || !asset || !asset.Get()->renderable) return false;
    m_asset = std::move(asset);
    // Optional legacy/reference interop. All production geometry and material construction is neutral.
    m_pgrnModel = AssetRuntime::GrannyInterop::GetModel(m_asset);
    if (!LoadAssetMeshes() || !__LoadVertices() || !LoadIndices()) { Destroy(); return false; }
    m_skinningData = SkinningDataAdapter::Extract(m_asset);
    if (!m_skinningData) { Destroy(); return false; }
    for (std::size_t mesh = 0; mesh < m_skinningData->status.size(); ++mesh) {
        const auto status = m_skinningData->status[mesh];
        if (status != Renderer::SkinDataStatus::Ready && status != Renderer::SkinDataStatus::Rigid &&
            status != Renderer::SkinDataStatus::Empty && Renderer::skinSidecarFailures.fetch_add(1) < 16)
            TraceError("Skinning data preparation: model=%s mesh=%zu status=%s; CPU path unchanged",
                GetAsset()->name.c_str(), mesh, Renderer::SkinDataStatusName(status));
    }
    AddReference();
    return true;
}

bool CGrannyModel::CreateFromGrannyModelPointer(granny_model* pgrnModel)
{
	assert(IsEmpty());
	if (!pgrnModel) return false;

	m_pgrnModel = pgrnModel;

	if (!LoadMeshs())
		return false;

	if (!__LoadVertices())
		return false;

	if (!LoadIndices())
		return false;

    // ZiiNAN: GPU skinning static mesh data
    m_skinningData=SkinningDataAdapter::Extract(*pgrnModel, GetAsset());
    for(size_t mesh=0;mesh<m_skinningData->status.size();++mesh) {
        const auto status=m_skinningData->status[mesh];
        if(status!=Renderer::SkinDataStatus::Ready && status!=Renderer::SkinDataStatus::Rigid &&
           status!=Renderer::SkinDataStatus::Empty && Renderer::skinSidecarFailures.fetch_add(1)<16)
            TraceError("Skinning data preparation: model=%s mesh=%zu status=%s; CPU path unchanged",
                pgrnModel->Name?pgrnModel->Name:"",mesh,Renderer::SkinDataStatusName(status));
    }

	AddReference();

	return true;
}

int CGrannyModel::GetIdxCount()
{
	return m_idxCount;
}

bool CGrannyModel::CreateDeviceObjects()
{
	if (m_rigidVtxCount > 0)
		if (!m_pntVtxBuf.CreateDeviceObjects())
			return false;

	if (m_idxCount > 0)
		if (!m_idxBuf.CreateDeviceObjects())
			return false;

	int meshCount = GetMeshCount();

	for (int i = 0; i < meshCount; ++i)
	{
		CGrannyMesh& rMesh = m_meshs[i];
		rMesh.RebuildTriGroupNodeList();
	}
			
	return true;
}

void CGrannyModel::DestroyDeviceObjects()
{
	m_pntVtxBuf.DestroyDeviceObjects();
	m_idxBuf.DestroyDeviceObjects();
}

bool CGrannyModel::IsEmpty() const
{
	if (m_asset || m_pgrnModel)
		return false;

	return true;
}

void CGrannyModel::Destroy()
{	
	m_kMtrlPal.Clear();
	
	if (m_meshNodes)
		delete [] m_meshNodes;

	if (m_meshs)
		delete [] m_meshs;

	m_pntVtxBuf.Destroy();
	m_idxBuf.Destroy();

	Initialize();
}

bool CGrannyModel::__LoadVertices()
{
	if (m_rigidVtxCount <= 0)
		return true;
	
	assert(m_meshs != NULL);

//	assert((m_vertexLayout & (Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1)) == m_vertexLayout);

//	if (!m_pntVtxBuf.Create(m_rigidVtxCount, Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1))
	const auto stride = Renderer::VertexStride(m_vertexLayout);
    if (!stride || std::uint32_t(m_rigidVtxCount) > std::uint32_t(std::numeric_limits<int>::max()) / stride) return false;
	if (!m_pntVtxBuf.Create(m_rigidVtxCount, m_vertexLayout))
		return false;
	
	void* vertices;
	if (!m_pntVtxBuf.Lock(&vertices))
		return false;
	
	for (int m = 0; m < GetMeshCount(); ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		if (!rMesh.NEW_LoadVertices(vertices)) { m_pntVtxBuf.Unlock(); return false; }
	}
	
	m_pntVtxBuf.Unlock();
	return true;
}

void CGrannyModel::Initialize()
{
    m_asset = {};
    m_indexWidth = AssetRuntime::IndexWidth::UInt16;
    m_skinningData.reset();
    m_staticObjectSource.reset();
    m_actorSource.reset(); // ZiiNAN: Model-owned immutable actor indices.
	memset(m_meshNodeLists, 0, sizeof(m_meshNodeLists));
	
	m_pgrnModel = NULL;
	m_meshs = NULL;
	m_meshNodes = NULL;

	m_meshNodeSize = 0;
	m_meshNodeCapacity = 0;

	m_rigidVtxCount = 0;
	m_deformVtxCount = 0;
	m_vtxCount = 0;
	m_idxCount = 0;

	m_canDeformPNVertices = false;

	m_vertexLayout = 0;
	m_bHaveBlendThing = false;
}

bool CGrannyModel::CaptureStaticObjectSource()
{
    if (!Renderer::staticObjectLoadDepth || m_deformVtxCount || m_bHaveBlendThing ||
        m_vertexLayout != (Renderer::VertexPosition | Renderer::VertexNormal | Renderer::VertexTex1) ||
        m_rigidVtxCount <= 0 || m_idxCount <= 0) return true;
    static_assert(sizeof(Renderer::StaticObjectVertex) == sizeof(TPNTVertex));
    auto source = std::make_shared<Renderer::StaticObjectSource>();
    source->vertices.resize(m_rigidVtxCount);
    const bool wide = m_indexWidth == AssetRuntime::IndexWidth::UInt32;
    if (wide) source->indices32.resize(m_idxCount);
    else source->indices.resize(m_idxCount);
    void* indices = wide ? static_cast<void*>(source->indices32.data()) : static_cast<void*>(source->indices.data());
    for (int i=0; i<GetMeshCount(); ++i)
    {
        // Same original conversion and offsets, while Granny file sections are alive.
        if (!m_meshs[i].NEW_LoadVertices(source->vertices.data()) ||
            !m_meshs[i].LoadIndices(indices, m_indexWidth)) return false;
    }
    m_staticObjectSource = std::move(source);
    return true;
}

// ZiiNAN: Original deform indices plus local PNT for rigid pieces inside the same body.
bool CGrannyModel::CaptureActorSource(bool attachment)
{
    // ZiiNAN: Diligent actor attachment rendering
    if(!Renderer::actorRenderer || m_vtxCount<=0 || m_idxCount<=0 ||
       m_vertexLayout!=(Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1)) return true;
    const uint64_t vertexCount=static_cast<uint64_t>(m_deformVtxCount)+static_cast<uint64_t>(m_rigidVtxCount);
    if(vertexCount>std::numeric_limits<uint32_t>::max()) return false;
    auto source=std::make_shared<Renderer::ActorModelSource>();
    source->vertexCount=static_cast<uint32_t>(vertexCount);
    source->deformVertexCount=static_cast<uint32_t>(m_deformVtxCount);
    source->rigidVertices.resize(m_rigidVtxCount);
    const bool wide = m_indexWidth == AssetRuntime::IndexWidth::UInt32;
    if (wide) source->indices32.resize(m_idxCount);
    else source->indices.resize(m_idxCount);
    void* indices = wide ? static_cast<void*>(source->indices32.data()) : static_cast<void*>(source->indices.data());
    for(int i=0;i<GetMeshCount();++i) {
        if(!m_meshs[i].LoadIndices(indices, m_indexWidth)) return false;
        if(m_rigidVtxCount && !m_meshs[i].NEW_LoadVertices(source->rigidVertices.data())) return false;
    }
    m_actorSource=std::move(source);
    return true;
}

CGrannyModel::CGrannyModel()
{
	Initialize();
}

CGrannyModel::~CGrannyModel()
{
	Destroy();
}
