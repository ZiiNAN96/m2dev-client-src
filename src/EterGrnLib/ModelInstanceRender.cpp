#include "StdAfx.h"
#include "Renderer/WorldRenderData.h"
#include "Eterlib/DrawState.h"
#include "ModelInstance.h"
#include "Model.h"

#ifdef _TEST

#include "Eterlib/GrpScreen.h"

void RenderAssetBones(CGrannyModelInstance& instance, const Math::Matrix& base)
{
    const auto* model=instance.GetModel();
    const auto& data=model->GetSkinningData();
    if (!data || !data->skeleton) return;
    CScreen screen;
    for (size_t bone=0;bone<data->skeleton->names.size();++bone) {
        const auto* values=instance.GetBoneMatrixPointer(static_cast<int>(bone));
        if (!values) continue;
        Math::Matrix local,world;
        std::memcpy(&local,values,sizeof(local));
        Math::MatrixMultiply(&world,&local,&base);
        DRAWSTATE.SetTransform(Renderer::MatrixWorld,&world);
        screen.RenderBox3d(-5.0f,-5.0f,-5.0f,5.0f,5.0f,5.0f);
    }
}
#endif


void CGrannyModelInstance::DeformNoSkin(const Math::Matrix * c_pWorldMatrix)
{
	if (IsEmpty())
		return;

	// DELETED
	//m_pgrnWorldPose = m_pgrnWorldPoseReal;
	///////////////////////////////
	
	if (!UpdateWorldPose()) return;
	UpdateWorldMatrices(c_pWorldMatrix);
}

///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//// Render
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
// With One Texture
void CGrannyModelInstance::RenderWithOneTexture()
{
	// FIXME : Deform, Render, BlendRender를 묶어 상위에서 걸러주는 것이 더 나을 듯 - [levites]
	if (IsEmpty())
		return;

#ifdef _TEST
	RenderAssetBones(*this, TEST_matWorld);
	if (GetAsyncKeyState('P'))
		Tracef("render %p", static_cast<void*>(m_animationInstance.get()));
	return;
#endif


	// WORK
	// END_OF_WORK


	if (m_pModel->GetDeformVertexCount()>0)
	{
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_DIFFUSE_PNT);
	}
	if (m_pModel->GetRigidVertexCount()>0)
	{
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);
	}
}

void CGrannyModelInstance::BlendRenderWithOneTexture()
{
	if (IsEmpty())
		return;

	// WORK
	// END_OF_WORK


	if (m_pModel->GetDeformVertexCount()>0)
	{
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_BLEND_PNT);
	}

	if (m_pModel->GetRigidVertexCount()>0)
	{
		RenderMeshNodeListWithOneTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
	}
}

// With Two Texture
void CGrannyModelInstance::RenderWithTwoTexture()
{
	// FIXME : Deform, Render, BlendRender를 묶어 상위에서 걸러주는 것이 더 나을 듯 - [levites]
	if (IsEmpty())
		return;


	// WORK
	// END_OF_WORK

	if (m_pModel->GetDeformVertexCount()>0)
	{
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_DIFFUSE_PNT);
	}
	if (m_pModel->GetRigidVertexCount()>0)
	{
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);
	}
}

void CGrannyModelInstance::BlendRenderWithTwoTexture()
{
	if (IsEmpty())
		return;

	// WORK
	// END_OF_WORK


	if (m_pModel->GetDeformVertexCount()>0)
	{
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_BLEND_PNT);
	}

	if (m_pModel->GetRigidVertexCount()>0)
	{
		RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
	}
}

void CGrannyModelInstance::RenderWithoutTexture()
{
	if (IsEmpty())
		return;

	DRAWSTATE.SetTexture(0, NULL);
	DRAWSTATE.SetTexture(1, NULL);

	// WORK
	// END_OF_WORK

	if (m_pModel->GetDeformVertexCount()>0)
	{
		RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_DIFFUSE_PNT);
		RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_DEFORM, CGrannyMaterial::TYPE_BLEND_PNT);
	}

	if (m_pModel->GetRigidVertexCount()>0)
	{
		RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_DIFFUSE_PNT);
		RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
//// Render Mesh List
///////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

// With One Texture
void CGrannyModelInstance::RenderMeshNodeListWithOneTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType)
{
	assert(m_pModel != NULL);

	assert(m_pModel->GetIdxCount()>0);

	const CGrannyModel::TMeshNode * pMeshNode = m_pModel->GetMeshNodeList(eMeshType, eMtrlType);

	while (pMeshNode)
	{
		const CGrannyMesh * pMesh = pMeshNode->pMesh;
		int vtxMeshBasePos = pMesh->GetVertexBasePosition();

		DRAWSTATE.SetTransform(Renderer::MatrixWorld, &m_meshMatrices[pMeshNode->iMesh]);

		/////
		const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(eMtrlType);
		int vtxCount = pMesh->GetVertexCount();

		while (pTriGroupNode)
		{
			ms_faceCount += pTriGroupNode->triCount;

			// MR-12: Fix specular isolation issue
			CGrannyMaterial& rkMtrl = m_kMtrlPal.GetMaterialRef(pTriGroupNode->mtrlIndex);

			if (!material_data_.pImage)
			{
				if (std::fabs(rkMtrl.GetSpecularPower() - material_data_.fSpecularPower) >= std::numeric_limits<float>::epsilon())
					rkMtrl.SetSpecularInfo(material_data_.isSpecularEnable, material_data_.fSpecularPower, material_data_.bSphereMapIndex);
			}
			// MR-12: -- END OF -- Fix specular isolation issue

			rkMtrl.ApplyRenderState();
			// ZiiNAN: Read the exact visible body material/pass; keep the native draw unchanged.
			Renderer::SubmitActorNativeDraw(this,
					{uint32_t(pMeshNode->iMesh),uint32_t(pTriGroupNode->mtrlIndex),uint32_t(pTriGroupNode->idxPos),
					 uint32_t(pTriGroupNode->triCount*3),uint32_t(vtxMeshBasePos),uint32_t(vtxCount),eMeshType==CGrannyMesh::TYPE_RIGID});
			rkMtrl.RestoreRenderState();
			
			pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
		}
		/////

		pMeshNode = pMeshNode->pNextMeshNode;
	}
}

// With Two Texture
void CGrannyModelInstance::RenderMeshNodeListWithTwoTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType)
{
	assert(m_pModel != NULL);

	assert(m_pModel->GetIdxCount()>0);

	const CGrannyModel::TMeshNode * pMeshNode = m_pModel->GetMeshNodeList(eMeshType, eMtrlType);

	while (pMeshNode)
	{
		const CGrannyMesh * pMesh = pMeshNode->pMesh;
		int vtxMeshBasePos = pMesh->GetVertexBasePosition();

		DRAWSTATE.SetTransform(Renderer::MatrixWorld, &m_meshMatrices[pMeshNode->iMesh]);

		/////
		const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(eMtrlType);
		int vtxCount = pMesh->GetVertexCount();
		while (pTriGroupNode)
		{
			ms_faceCount += pTriGroupNode->triCount;

			const CGrannyMaterial& rkMtrl=m_kMtrlPal.GetMaterialRef(pTriGroupNode->mtrlIndex);
			DRAWSTATE.SetTexture(0, rkMtrl.GetTextureBinding(0));
			DRAWSTATE.SetTexture(1, rkMtrl.GetTextureBinding(1));
			// ZiiNAN: Snapshot only the explicitly scoped DungeonBlock material draw.
			if(Renderer::specialMeshTarget.instance==this && Renderer::specialMeshTarget.submit)
				Renderer::specialMeshTarget.submit(this,{uint32_t(pMeshNode->iMesh),uint32_t(pTriGroupNode->mtrlIndex),
					uint32_t(pTriGroupNode->idxPos),uint32_t(pTriGroupNode->triCount*3),uint32_t(vtxMeshBasePos),uint32_t(vtxCount)});
			pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
		}
		/////

		pMeshNode = pMeshNode->pNextMeshNode;
	}
}

// Without Texture
void CGrannyModelInstance::RenderMeshNodeListWithoutTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType)
{
	assert(m_pModel != NULL);

	assert(m_pModel->GetIdxCount()>0);

	const CGrannyModel::TMeshNode * pMeshNode = m_pModel->GetMeshNodeList(eMeshType, eMtrlType);

	while (pMeshNode)
	{
		const CGrannyMesh * pMesh = pMeshNode->pMesh;
		int vtxMeshBasePos = pMesh->GetVertexBasePosition();

		DRAWSTATE.SetTransform(Renderer::MatrixWorld, &m_meshMatrices[pMeshNode->iMesh]);

		/////
		const CGrannyMesh::TTriGroupNode* pTriGroupNode = pMesh->GetTriGroupNodeList(eMtrlType);
		int vtxCount = pMesh->GetVertexCount();

		while (pTriGroupNode)
		{
			ms_faceCount += pTriGroupNode->triCount;
			pTriGroupNode = pTriGroupNode->pNextTriGroupNode;
		}
		/////

		pMeshNode = pMeshNode->pNextMeshNode;
	}
}

