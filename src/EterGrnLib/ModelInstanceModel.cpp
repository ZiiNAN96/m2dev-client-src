#include "StdAfx.h"
#include "AssetRuntime/Granny/GrannyInterop.h"
#include "ModelInstance.h"
#include "Model.h"

void CGrannyModelInstance::Clear()
{
    // ZiiNAN: GPU skinning static mesh data
    m_skinningRemaps.clear();
    m_skinningBindingDestination.reset();
    m_linkedLodBindingDestination.reset();
    m_skinningPalette.reset();
    m_skinningStatus=Renderer::SkinDataStatus::Empty;
    m_skinningIssueReported=false;
    // ZiiNAN: Release actor bindings before pooled instance/model reuse.
    if(Renderer::actorRenderer && m_actorRenderData.geometry) Renderer::actorRenderer->ReleaseBindings();
    m_actorRenderData={};
	m_kMtrlPal.Clear();
	
	DestroyDeviceObjects();
	// WORK
	__DestroyMeshBindingVector();
	// END_OF_WORK
	__DestroyMeshMatrices();
	__DestroyModelInstance();
	__DestroyWorldPose();

	__Initialize();
}

void CGrannyModelInstance::SetMainModelPointer(CGrannyModel* pModel, CGraphicVertexBuffer* pkSharedDeformableVertexBuffer)
{
	SetLinkedModelPointer(pModel, pkSharedDeformableVertexBuffer, NULL);
}

void CGrannyModelInstance::SetLinkedModelPointer(CGrannyModel* pkModel, CGraphicVertexBuffer* pkSharedDeformableVertexBuffer, CGrannyModelInstance** ppkSkeletonInst, bool refreshLinkedLodBinding)
{
	Clear();

	if (m_pModel)
		m_pModel->Release();

	m_pModel = pkModel;
	if (!m_pModel) return;

	m_pModel->AddReference();
	
	if (pkSharedDeformableVertexBuffer)
		__SetSharedDeformableVertexBuffer(pkSharedDeformableVertexBuffer);
	else
		__CreateDynamicVertexBuffer();

	__CreateModelInstance();
	if (!m_animationInstance) { Clear(); return; }
	
	// WORK
	if (ppkSkeletonInst && *ppkSkeletonInst)
	{
		m_ppkSkeletonInst = ppkSkeletonInst;
        if(refreshLinkedLodBinding && (*ppkSkeletonInst)->m_pModel->GetSkinningData())
            m_linkedLodBindingDestination=(*ppkSkeletonInst)->m_pModel->GetSkinningData()->skeleton;
		__CreateWorldPose(*ppkSkeletonInst);			
		if (!__CreateMeshBindingVector(*ppkSkeletonInst)) { Clear(); return; }
	}
	else
	{
		__CreateWorldPose(NULL);
        if (!m_ownsWorldPose) { Clear(); return; }
		if (!__CreateMeshBindingVector(NULL)) { Clear(); return; }
	}
	// END_OF_WORK	

	__CreateMeshMatrices();

	ResetLocalTime();
	
	m_kMtrlPal.Copy(pkModel->GetMaterialPalette());
}

// WORK
AssetRuntime::AnimationInstance* CGrannyModelInstance::__GetPoseOwner() const
{
    if (m_ownsWorldPose) return m_animationInstance.get();
	
	if (m_ppkSkeletonInst && *m_ppkSkeletonInst)
		return (*m_ppkSkeletonInst)->m_animationInstance.get();
    return nullptr;
}

AssetRuntime::PoseView CGrannyModelInstance::__GetCompositePose() const
{
    const auto* owner=__GetPoseOwner();
    return owner ? owner->CompositePose() : AssetRuntime::PoseView{};
}

const int* CGrannyModelInstance::__GetMeshBoneIndices(unsigned int index) const
{
    return index<m_meshBindings.size() && m_meshBindings[index] ? m_meshBindings[index]->BoneIndices().data() : nullptr;
}

bool CGrannyModelInstance::__CreateMeshBindingVector(CGrannyModelInstance* pkDstModelInst)
{
    assert(m_meshBindings.empty());
    if (!m_pModel) return false;
    auto* destination=pkDstModelInst ? pkDstModelInst->m_animationInstance.get() : m_animationInstance.get();
    if (!destination) return false;
    m_meshBindings.reserve(m_pModel->GetMeshCount());
    for (int mesh=0; mesh<m_pModel->GetMeshCount(); ++mesh) {
        auto binding=m_pModel->GetAssetHandle() ?
            destination->CreateMeshBinding(m_pModel->GetAssetHandle(),mesh) :
            AssetRuntime::GrannyInterop::CreateLegacyMeshBinding(m_pModel->GetGrannyModelPointer(),mesh,*destination);
        if (!binding) { m_meshBindings.clear(); return false; }
        m_meshBindings.push_back(std::move(binding));
    }

    __PrepareSkinningBindings();

	return true;
}

void CGrannyModelInstance::__DestroyMeshBindingVector()
{
    m_meshBindings.clear();
}

// END_OF_WORK


void CGrannyModelInstance::__CreateWorldPose(CGrannyModelInstance* pkSkeletonInst)
{
    assert(m_animationInstance);
    assert(!m_ownsWorldPose);

	// WORK
	if (pkSkeletonInst)
		return;	
	// END_OF_WORK

    m_ownsWorldPose=m_animationInstance->PreparePose();
}

void CGrannyModelInstance::__DestroyWorldPose()
{
    m_ownsWorldPose=false;
}

void CGrannyModelInstance::__CreateModelInstance()
{	
	assert(m_pModel != NULL);
    assert(!m_animationInstance);
    const auto& model=m_pModel->GetAssetHandle();
    m_animationInstance=model ? model.GetDocument()->CreateAnimationInstance(model) :
        AssetRuntime::GrannyInterop::CreateLegacyAnimationInstance(m_pModel->GetGrannyModelPointer());
}

void CGrannyModelInstance::__DestroyModelInstance()
{
    m_animationInstance.reset();
}

void CGrannyModelInstance::__CreateMeshMatrices()
{
	assert(m_pModel != NULL);
	
	if (m_pModel->GetMeshCount() <= 0) // 메쉬가 없는 (카메라 같은) 모델도 간혹 있다..
		return;
	
	int meshCount = m_pModel->GetMeshCount();	
	m_meshMatrices = new Math::Matrix[meshCount];
}

void CGrannyModelInstance::__DestroyMeshMatrices()
{
	if (!m_meshMatrices)
		return;

	delete [] m_meshMatrices;
	m_meshMatrices = NULL;
}

DWORD CGrannyModelInstance::GetDeformableVertexCount()
{
	if (!m_pModel)
		return 0;

	return m_pModel->GetDeformVertexCount();
}

DWORD CGrannyModelInstance::GetVertexCount()
{
	if (!m_pModel)
		return 0;

	return m_pModel->GetVertexCount();
}

// WORK

void CGrannyModelInstance::__SetSharedDeformableVertexBuffer(CGraphicVertexBuffer* pkSharedDeformableVertexBuffer)
{
	m_pkSharedDeformableVertexBuffer = pkSharedDeformableVertexBuffer;
}

bool CGrannyModelInstance::__IsDeformableVertexBuffer()
{
	if (m_pkSharedDeformableVertexBuffer)
		return true;

	return m_kLocalDeformableVertexBuffer.IsEmpty();
}



CGraphicVertexBuffer& CGrannyModelInstance::__GetDeformableVertexBufferRef()
{
	if (m_pkSharedDeformableVertexBuffer)
		return *m_pkSharedDeformableVertexBuffer;

	return m_kLocalDeformableVertexBuffer;
}

void CGrannyModelInstance::__CreateDynamicVertexBuffer()
{
	assert(m_pModel != NULL);
	assert(m_kLocalDeformableVertexBuffer.IsEmpty());

	int vtxCount = m_pModel->GetDeformVertexCount();

	if (0 != vtxCount)
	{
		if (!m_kLocalDeformableVertexBuffer.Create(vtxCount,
									   Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1
		))
			return;
	}	
}

void CGrannyModelInstance::__DestroyDynamicVertexBuffer()
{
	m_kLocalDeformableVertexBuffer.Destroy();
	m_pkSharedDeformableVertexBuffer = NULL;
}

// END_OF_WORK

bool CGrannyModelInstance::GetBoneIndexByName(const char * c_szBoneName, int * pBoneIndex) const
{
	if (!m_pModel || !c_szBoneName || !pBoneIndex) return false;
    if (const auto* asset = m_pModel->GetAsset()) {
        if (!asset->skeleton) return false;
        const auto binding = AssetRuntime::ResolveAttachment(*asset->skeleton, c_szBoneName);
        if (!binding) return false;
        *pBoneIndex = binding.bone;
        return true;
    }
    if (!m_animationInstance) return false;
    auto* native=AssetRuntime::GrannyInterop::GetAnimationInstance(*m_animationInstance);
    if (!native) return false;
	granny_skeleton * pgrnSkeleton = GrannyGetSourceSkeleton(native);

	if (!GrannyFindBoneByName(pgrnSkeleton, c_szBoneName, pBoneIndex))
		return false;

	return true;
}

const float * CGrannyModelInstance::GetBoneMatrixPointer(int iBone) const
{
    const auto* owner=__GetPoseOwner();
    const auto matrix=owner ? owner->BoneWorldMatrix(iBone) : std::span<const float>{};
    return matrix.size()==16 ? matrix.data() : nullptr;
}

const float * CGrannyModelInstance::GetCompositeBoneMatrixPointer(int iBone) const
{
	// NOTE : GrannyGetWorldPose4x4는 스케일 값등이 잘못나올 수 있음.. 그래니가 속도를 위해
	//        GrannyGetWorldPose4x4에 모든 matrix 원소를 제 값으로 넣지 않음
    const auto pose=__GetCompositePose();
    return iBone>=0 && size_t(iBone)<pose.BoneCount() ? pose.values.data()+size_t(iBone)*16 : nullptr;
}

void CGrannyModelInstance::ReloadTexture()
{
	assert("현재 사용하지 않음 - CGrannyModelInstance::ReloadTexture()");
/*
	assert(m_pModel != NULL);
	const CGrannyMaterialPalette & c_rGrannyMaterialPalette = m_pModel->GetMaterialPalette();
	DWORD dwMaterialCount = c_rGrannyMaterialPalette.GetMaterialCount();
	for (DWORD dwMtrIndex = 0; dwMtrIndex < dwMaterialCount; ++dwMtrIndex)
	{
		const CGrannyMaterial & c_rGrannyMaterial = c_rGrannyMaterialPalette.GetMaterialRef(dwMtrIndex);
		CGraphicImage * pImageStage0 = c_rGrannyMaterial.GetImagePointer(0);
		if (pImageStage0)
			pImageStage0->Reload();
		CGraphicImage * pImageStage1 = c_rGrannyMaterial.GetImagePointer(1);
		if (pImageStage1)
			pImageStage1->Reload();
	}
*/
}
