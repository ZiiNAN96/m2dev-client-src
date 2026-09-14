#include "StdAfx.h"
#include "Eterbase/Debug.h"
#include "ModelInstance.h"
#include "Model.h"
#include "Renderer/WorldRenderData.h"
#include "Renderer/SkinningBenchmark.h"


void CGrannyModelInstance::Update(DWORD dwAniFPS)
{		
	if (!dwAniFPS || !m_animationInstance)
		return;

	const DWORD c_dwCurUpdateFrame = (DWORD) (GetLocalTime() * static_cast<float>(ANIFPS_MAX));
	const DWORD ANIFPS_STEP = ANIFPS_MAX/dwAniFPS;
	if (c_dwCurUpdateFrame>ANIFPS_STEP && c_dwCurUpdateFrame/ANIFPS_STEP==m_dwOldUpdateFrame/ANIFPS_STEP)
		return;

	m_dwOldUpdateFrame=c_dwCurUpdateFrame;

    m_animationInstance->FreeCompletedControls();

	//DWORD t1=timeGetTime();
    m_animationInstance->SetClock(GetLocalTime());
	//DWORD t2=timeGetTime();

#ifdef __PERFORMANCE_CHECKER__
	{
		static FILE* fp=fopen("perf_grn_setmodelclock.txt", "w");

		if (t2-t1>3)
		{
			fprintf(fp, "%f:%p:- GrannySetModelClock(time=%f) = %dms\n", timeGetTime()/1000.0f, static_cast<void*>(this), GetLocalTime(), t2-t1);
			fflush(fp);
		}			
	}
#endif	

}

void CGrannyModelInstance::UpdateLocalTime(float fElapsedTime)
{
	m_fSecondsElapsed = fElapsedTime;
	m_fLocalTime += fElapsedTime;
}

void CGrannyModelInstance::UpdateTransform(Math::Matrix * pMatrix, float fSecondsElapsed)
{
	if (!m_animationInstance)
	{
		TraceError("CGrannyModelIstance::UpdateTransform - m_pgrnModelInstance = NULL");
		return;
	}
    m_animationInstance->UpdateTransform(fSecondsElapsed,std::span<float,16>(reinterpret_cast<float*>(pMatrix),16));
	//Tracef("%f %f %f",pMatrix->_41,pMatrix->_42,pMatrix->_43);
	
}

void CGrannyModelInstance::Deform(const Math::Matrix * c_pWorldMatrix)
{
	if (IsEmpty())
		return;

    // ZiiNAN: A failed native deformation must not submit a stale actor pose.
    const bool captureActor=m_pModel->GetActorSource()!=nullptr &&
        (Renderer::actorDeformTargets.Find(this)!=Renderer::ActorPart::Unsupported ||
         (Renderer::worldSurfaceFrame && !m_pModel->GetStaticObjectSource()));
    if(captureActor) m_actorRenderData.ready=false;

	// DELETED
	//m_pgrnWorldPose = m_pgrnWorldPoseReal;
	/////////////////////////////////////////////
	
	if (!UpdateWorldPose()) return;
	if (!UpdateWorldMatrices(c_pWorldMatrix)) return;

    // ZiiNAN: GPU skinning actor coverage
    const auto part=Renderer::actorDeformTargets.Find(this);
    const bool reference=captureActor && m_pModel->GetActorSource()->deformVertexCount &&
        (Renderer::actorDeformTargets.prototypeBody==this ||
         (Renderer::actorDeformTargets.gpuSkinning && part!=Renderer::ActorPart::Unsupported));
    if(reference && Renderer::startupSkinningMode==Renderer::PrototypeSkinningMode::GPUPrototype) {
        const auto start=Renderer::PrototypeClock::now();
        const auto palette=GetSkinningPalette();
        if(Renderer::actorRenderer && palette && m_pModel->GetSkinningData() &&
           Renderer::actorRenderer->PreparePrototype(m_actorRenderData.geometry,*m_pModel->GetSkinningData(),m_skinningRemaps,*palette,
               m_pModel->GetActorSource().get(),part,Renderer::actorDeformTargets.category)) {
            m_actorRenderData.gpuPrototype=true; m_actorRenderData.ready=true;
            m_actorRenderData.capturedFrame=Renderer::actorFrameSerial;
            ++Renderer::prototypeFrames;
            Renderer::prototypePrepareUs+=Renderer::PrototypeMicroseconds(start);
            return;
        }
        ++Renderer::skinningFallbacks;
        if(m_actorRenderData.reports.insert("GPU prototype CPU fallback").second)
            TraceError("GPU skinning CPU fallback: model=%s part=%u status=%s palette=%s remaps=%zu; original CPU deformation",
                m_pModel->GetAsset() ? m_pModel->GetAsset()->name.c_str() : "legacy-reference",static_cast<unsigned>(part),Renderer::SkinDataStatusName(m_skinningStatus),
                palette ? "present" : "unavailable",m_skinningRemaps.size());
    }
    if(m_actorRenderData.gpuPrototype) {
        m_actorRenderData.geometry.reset(); m_actorRenderData.uploadedRevision=0;
        m_actorRenderData.gpuPrototype=false;
    }

    // ZiiNAN: Diligent actor attachment rendering
    if(captureActor && m_pModel->GetActorSource()->IsRigid()) {
        m_actorRenderData.capturedFrame=Renderer::actorFrameSerial;
        m_actorRenderData.ready=true;
    }

	if (m_pModel->CanDeformPNTVertices())
	{
		// WORK
		CGraphicVertexBuffer& rkDeformableVertexBuffer = __GetDeformableVertexBufferRef();
		TPNTVertex* pntVertices;
		if (rkDeformableVertexBuffer.LockRange(m_pModel->GetDeformVertexCount(), (void **)&pntVertices))
		{
            const auto skinStart=reference ? Renderer::PrototypeClock::now() : Renderer::PrototypeClock::time_point{};
            if (!DeformPNTVertices(pntVertices)) { rkDeformableVertexBuffer.Unlock(); return; }
            if(reference) {
                Renderer::prototypeCpuSkinUs+=Renderer::PrototypeMicroseconds(skinStart);
                ++Renderer::prototypeCpuFrames;
                Renderer::prototypeCpuBytes+=uint64_t(m_pModel->GetDeformVertexCount())*sizeof(TPNTVertex);
            }
            // ZiiNAN: Copy finished CPU-skinned PNT before the existing Unlock; no second skinning pass.
            if(captureActor) {
                static_assert(sizeof(Renderer::StaticObjectVertex)==sizeof(TPNTVertex));
                const auto& source=*m_pModel->GetActorSource();
                m_actorRenderData.vertices.resize(source.vertexCount);
                memcpy(m_actorRenderData.vertices.data(),pntVertices,source.deformVertexCount*sizeof(TPNTVertex));
                // ZiiNAN: Rigid local vertices retain their separate native Bone*World draw matrix.
                if(!source.rigidVertices.empty())
                    memcpy(m_actorRenderData.vertices.data()+source.deformVertexCount,source.rigidVertices.data(),source.rigidVertices.size()*sizeof(TPNTVertex));
                ++m_actorRenderData.revision;
                m_actorRenderData.capturedFrame=Renderer::actorFrameSerial;
                m_actorRenderData.ready=true;
            }
			rkDeformableVertexBuffer.Unlock();
		}
		else
		{
			TraceError("GRANNY DEFORM DYNAMIC BUFFER LOCK ERROR");
		}
		// END_OF_WORK
	}	
}

void CGrannyModelInstance::UpdateSkeleton(const Math::Matrix * c_pWorldMatrix, float /*fLocalTime*/)
{	
	// DELETED
	//m_pgrnWorldPose = m_pgrnWorldPoseReal;
	///////////////////////////////////////////
	if (!UpdateWorldPose()) return;
	UpdateWorldMatrices(c_pWorldMatrix);
}

bool CGrannyModelInstance::UpdateWorldPose()
{
	// WEP	= m_iParentBoneIndex != 0 -> UpdateWorldPose(O)
	// LOD	= UpdateWorldPose(O)
	// Hair	= UpdateWorldPose(X)

	if (m_ppkSkeletonInst)
		if (*m_ppkSkeletonInst!=this)
			return *m_ppkSkeletonInst != nullptr;
	
    if (!m_animationInstance) return false;

	const float * pAttachBoneMatrix = (mc_pParentInstance) ? mc_pParentInstance->GetBoneMatrixPointer(m_iParentBoneIndex) : NULL;

    const auto evaluated = AssetRuntime::EvaluatePose(*m_animationInstance,
        {pAttachBoneMatrix ? std::span<const float>(pAttachBoneMatrix, 16) : std::span<const float>{}});
    if (evaluated.error != AssetRuntime::AssetError::None) {
        if (m_skinningPalette) m_skinningPalette->ready = false;
        m_actorRenderData.ready = false;
        return false;
    }
    // ZiiNAN: GPU skinning static mesh data
    __CaptureSkinningPose();
	/*
	GrannySampleModelAnimations(m_pgrnModelInstance, 0, pgrnSkeleton->BoneCount, pgrnLocalPose);
	GrannyBuildWorldPose(pgrnSkeleton, 0, pgrnSkeleton->BoneCount, pgrnLocalPose, pAttachBoneMatrix, m_pgrnWorldPose);
	*/
    m_animationInstance->FreeCompletedControls();
    return true;
}

bool CGrannyModelInstance::UpdateWorldMatrices(const Math::Matrix* c_pWorldMatrix)
{
    if(!__RefreshLinkedLodBinding()) { m_actorRenderData.ready=false; return false; }
    __PrepareSkinningBindings();
	// NO_MESH_BUG_FIX
	if (!m_meshMatrices)
		return false;
	// END_OF_NO_MESH_BUG_FIX
	
	assert(m_pModel != NULL);
	
	int meshCount = m_pModel->GetMeshCount();
	
    const auto pose=__GetCompositePose();
    if (!pose.Valid()) return false;

	for (int i = 0; i < meshCount; ++i)
	{
		Math::Matrix & rWorldMatrix = m_meshMatrices[i];

		const CGrannyMesh * pMesh = m_pModel->GetMeshPointer(i);

		// WORK
		const int * boneIndices = __GetMeshBoneIndices(i);
		// END_OF_WORK

		if (pMesh->CanDeformPNTVertices())
		{
			rWorldMatrix = *c_pWorldMatrix;			
		}
		else
		{
			if (!boneIndices || i>=m_meshBindings.size() || m_meshBindings[i]->BoneIndices().empty()) return false;
			int iBone = *boneIndices;
            if (iBone<0 || size_t(iBone)>=pose.BoneCount()) return false;
            Math::Matrix boneMatrix;
            std::memcpy(&boneMatrix,pose.values.data()+size_t(iBone)*16,sizeof(boneMatrix));
			Math::MatrixMultiply(&rWorldMatrix, &boneMatrix, c_pWorldMatrix);
		}
	}

#ifdef _TEST
	TEST_matWorld = *c_pWorldMatrix;
#endif
    return true;
}

bool CGrannyModelInstance::DeformPNTVertices(void * pvDest)
{
	assert(m_pModel != NULL);
	assert(m_pModel->CanDeformPNTVertices());

	// WORK
    return m_pModel->DeformPNTVertices(pvDest,__GetCompositePose(),m_meshBindings);
	// END_OF_WORK
}
