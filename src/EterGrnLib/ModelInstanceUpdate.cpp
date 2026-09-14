#include "StdAfx.h"
#include "Eterbase/Debug.h"
#include "ModelInstance.h"
#include "Model.h"
#include "Renderer/WorldRenderData.h"
#include "Renderer/SkinningBenchmark.h"


void CGrannyModelInstance::Update(DWORD dwAniFPS)
{		
	if (!dwAniFPS)
		return;

	const DWORD c_dwCurUpdateFrame = (DWORD) (GetLocalTime() * static_cast<float>(ANIFPS_MAX));
	const DWORD ANIFPS_STEP = ANIFPS_MAX/dwAniFPS;
	if (c_dwCurUpdateFrame>ANIFPS_STEP && c_dwCurUpdateFrame/ANIFPS_STEP==m_dwOldUpdateFrame/ANIFPS_STEP)
		return;

	m_dwOldUpdateFrame=c_dwCurUpdateFrame;

	GrannyFreeCompletedModelControls(m_pgrnModelInstance); //Black screen fix

	//DWORD t1=timeGetTime();
	GrannySetModelClock(m_pgrnModelInstance, GetLocalTime());	
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
	if (!m_pgrnModelInstance)
	{
		TraceError("CGrannyModelIstance::UpdateTransform - m_pgrnModelInstance = NULL");
		return;
	}
	GrannyUpdateModelMatrix(m_pgrnModelInstance, fSecondsElapsed, (const float *) pMatrix, (float *) pMatrix, false);
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
	
	UpdateWorldPose();
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
                m_pModel->GetGrannyModelPointer()->Name,static_cast<unsigned>(part),Renderer::SkinDataStatusName(m_skinningStatus),
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
			DeformPNTVertices(pntVertices);
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

//////////////////////////////////////////////////////
class CGrannyLocalPose
{
	public:
		CGrannyLocalPose()
		{
			m_pgrnLocalPose = NULL;
			m_boneCount = 0;
		}

		virtual ~CGrannyLocalPose()
		{
			if (m_pgrnLocalPose)
				GrannyFreeLocalPose(m_pgrnLocalPose);
		}

		granny_local_pose * Get(int boneCount)
		{
			if (m_pgrnLocalPose)
			{
				if (m_boneCount >= boneCount)
					return m_pgrnLocalPose;

				GrannyFreeLocalPose(m_pgrnLocalPose);
			}

			m_boneCount = boneCount;
			m_pgrnLocalPose = GrannyNewLocalPose(m_boneCount);
			return m_pgrnLocalPose;
		}

	private:
		granny_local_pose *	m_pgrnLocalPose;
		int					m_boneCount;
};
//////////////////////////////////////////////////////

void CGrannyModelInstance::UpdateSkeleton(const Math::Matrix * c_pWorldMatrix, float /*fLocalTime*/)
{	
	// DELETED
	//m_pgrnWorldPose = m_pgrnWorldPoseReal;
	///////////////////////////////////////////
	UpdateWorldPose();
	UpdateWorldMatrices(c_pWorldMatrix);
}

void CGrannyModelInstance::UpdateWorldPose()
{
	// WEP	= m_iParentBoneIndex != 0 -> UpdateWorldPose(O)
	// LOD	= UpdateWorldPose(O)
	// Hair	= UpdateWorldPose(X)

	if (m_ppkSkeletonInst)
		if (*m_ppkSkeletonInst!=this)
			return;
	
	static CGrannyLocalPose s_SharedLocalPose;

	granny_skeleton * pgrnSkeleton = GrannyGetSourceSkeleton(m_pgrnModelInstance);
	granny_local_pose * pgrnLocalPose = s_SharedLocalPose.Get(pgrnSkeleton->BoneCount);	

	const float * pAttachBoneMatrix = (mc_pParentInstance) ? mc_pParentInstance->GetBoneMatrixPointer(m_iParentBoneIndex) : NULL;

	GrannySampleModelAnimationsAccelerated(m_pgrnModelInstance, pgrnSkeleton->BoneCount, pAttachBoneMatrix, pgrnLocalPose, __GetWorldPosePtr());
    // ZiiNAN: GPU skinning static mesh data
    __CaptureSkinningPose();
	/*
	GrannySampleModelAnimations(m_pgrnModelInstance, 0, pgrnSkeleton->BoneCount, pgrnLocalPose);
	GrannyBuildWorldPose(pgrnSkeleton, 0, pgrnSkeleton->BoneCount, pgrnLocalPose, pAttachBoneMatrix, m_pgrnWorldPose);
	*/
	GrannyFreeCompletedModelControls(m_pgrnModelInstance);	

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
	
	granny_matrix_4x4 * pgrnMatCompositeBuffer = GrannyGetWorldPoseComposite4x4Array(__GetWorldPosePtr());
	Math::Matrix * boneMatrices = (Math::Matrix *) pgrnMatCompositeBuffer;

	for (int i = 0; i < meshCount; ++i)
	{
		Math::Matrix & rWorldMatrix = m_meshMatrices[i];

		const CGrannyMesh * pMesh = m_pModel->GetMeshPointer(i);

		// WORK
		int * boneIndices = __GetMeshBoneIndices(i);
		// END_OF_WORK

		if (pMesh->CanDeformPNTVertices())
		{
			rWorldMatrix = *c_pWorldMatrix;			
		}
		else
		{
			int iBone = *boneIndices;
			Math::MatrixMultiply(&rWorldMatrix, &boneMatrices[iBone], c_pWorldMatrix);
		}
	}

#ifdef _TEST
	TEST_matWorld = *c_pWorldMatrix;
#endif
    return true;
}

void CGrannyModelInstance::DeformPNTVertices(void * pvDest)
{
	assert(m_pModel != NULL);
	assert(m_pModel->CanDeformPNTVertices());

	// WORK
	m_pModel->DeformPNTVertices(pvDest, (Math::Matrix *) GrannyGetWorldPoseComposite4x4Array(__GetWorldPosePtr()), m_vct_pgrnMeshBinding);
	// END_OF_WORK
}
