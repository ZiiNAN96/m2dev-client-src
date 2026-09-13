#include "StdAfx.h"
#include "Model.h"
#include "Mesh.h"

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

void CGrannyModel::DeformPNTVertices(void * dstBaseVertices, Math::Matrix * boneMatrices, const std::vector<granny_mesh_binding*>& c_rvct_pgrnMeshBinding) const
{
	int meshCount = GetMeshCount();

	for (int iMesh = 0; iMesh < meshCount; ++iMesh)
	{
		assert(iMesh < c_rvct_pgrnMeshBinding.size());

		CGrannyMesh & rMesh = m_meshs[iMesh];
		if (rMesh.CanDeformPNTVertices())
			rMesh.DeformPNTVertices(dstBaseVertices, boneMatrices, c_rvct_pgrnMeshBinding[iMesh]);
	}
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
	return m_pgrnModel ? m_pgrnModel->MeshBindingCount : 0;
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

	if (!m_pntVtxBuf.Create(m_rigidVtxCount, m_vertexLayout))
		return false;

	void* vertices;
	if (!m_pntVtxBuf.Lock(&vertices))
		return false;

	for (int m = 0; m < m_pgrnModel->MeshBindingCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		rMesh.LoadPNTVertices(vertices);
	}

	m_pntVtxBuf.Unlock();
	return true;
}

bool CGrannyModel::LoadIndices()
{
	//assert(m_idxCount > 0);
	if (m_idxCount <= 0)
		return true;

	if (!m_idxBuf.Create(m_idxCount, Renderer::IndexFormat::UInt16))
		return false;

	void * indices;

	if (!m_idxBuf.Lock((void**)&indices))
		return false;

	for (int m = 0; m < m_pgrnModel->MeshBindingCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		rMesh.LoadIndices(indices);
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

		if (GrannyMeshIsRigid(pgrnMesh))
		{
			if (!rMesh.CreateFromGrannyMeshPointer(pgrnSkeleton, pgrnMesh, vtxRigidPos, idxPos, m_kMtrlPal))
				return false;

			vtxRigidPos += GrannyGetMeshVertexCount(pgrnMesh);	
		}
		else
		{
			if (!rMesh.CreateFromGrannyMeshPointer(pgrnSkeleton, pgrnMesh, vtxDeformPos, idxPos, m_kMtrlPal))
				return false;

			vtxDeformPos += GrannyGetMeshVertexCount(pgrnMesh);
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

		vtxPos += GrannyGetMeshVertexCount(pgrnMesh);
		idxPos += GrannyGetMeshIndexCount(pgrnMesh);		

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_DIFFUSE_PNT))
			++diffusePNTMeshNodeCount;

		if (rMesh.GetTriGroupNodeList(CGrannyMaterial::TYPE_BLEND_PNT))
			++blendPNTMeshNodeCount;
	}

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

bool CGrannyModel::CreateFromGrannyModelPointer(granny_model* pgrnModel)
{
	assert(IsEmpty());

	m_pgrnModel = pgrnModel;

	if (!LoadMeshs())
		return false;

	if (!__LoadVertices())
		return false;

	if (!LoadIndices())
		return false;

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
	if (m_pgrnModel)
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
	if (!m_pntVtxBuf.Create(m_rigidVtxCount, m_vertexLayout))
		return false;
	
	void* vertices;
	if (!m_pntVtxBuf.Lock(&vertices))
		return false;
	
	for (int m = 0; m < m_pgrnModel->MeshBindingCount; ++m)
	{
		CGrannyMesh& rMesh = m_meshs[m];
		rMesh.NEW_LoadVertices(vertices);
	}
	
	m_pntVtxBuf.Unlock();
	return true;
}

void CGrannyModel::Initialize()
{
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

void CGrannyModel::CaptureStaticObjectSource()
{
    if (!Renderer::staticObjectLoadDepth || m_deformVtxCount || m_bHaveBlendThing ||
        m_vertexLayout != (Renderer::VertexPosition | Renderer::VertexNormal | Renderer::VertexTex1) ||
        m_rigidVtxCount <= 0 || m_idxCount <= 0) return;
    static_assert(sizeof(Renderer::StaticObjectVertex) == sizeof(TPNTVertex));
    auto source = std::make_shared<Renderer::StaticObjectSource>();
    source->vertices.resize(m_rigidVtxCount);
    source->indices.resize(m_idxCount);
    for (int i=0; i<GetMeshCount(); ++i)
    {
        // Same original conversion and offsets, while Granny file sections are alive.
        m_meshs[i].NEW_LoadVertices(source->vertices.data());
        m_meshs[i].LoadIndices(source->indices.data());
    }
    m_staticObjectSource = std::move(source);
}

// ZiiNAN: Original deform indices plus local PNT for rigid pieces inside the same body.
void CGrannyModel::CaptureActorSource(bool attachment)
{
    // ZiiNAN: Diligent actor attachment rendering
    if(!Renderer::actorRenderer || m_vtxCount<=0 || m_idxCount<=0 ||
       m_vertexLayout!=(Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1)) return;
    auto source=std::make_shared<Renderer::ActorModelSource>();
    source->vertexCount=static_cast<uint32_t>(m_deformVtxCount+m_rigidVtxCount);
    source->deformVertexCount=static_cast<uint32_t>(m_deformVtxCount);
    source->rigidVertices.resize(m_rigidVtxCount);
    source->indices.resize(m_idxCount);
    for(int i=0;i<GetMeshCount();++i) {
        m_meshs[i].LoadIndices(source->indices.data());
        if(m_rigidVtxCount) m_meshs[i].NEW_LoadVertices(source->rigidVertices.data());
    }
    m_actorSource=std::move(source);
}

CGrannyModel::CGrannyModel()
{
	Initialize();
}

CGrannyModel::~CGrannyModel()
{
	Destroy();
}
