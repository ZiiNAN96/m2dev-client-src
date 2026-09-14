#pragma once

//#define CACHE_DEFORMED_VERTEX
#include "Eterlib/GrpImage.h"
#include "Eterlib/GrpCollisionObject.h"

#include "Model.h"
#include "Motion.h"

class CGrannyModelInstance : public CGraphicCollisionObject
{
	public:
		enum
		{
			ANIFPS_MIN = 30,
			ANIFPS_MAX = 120,
		};
	public:
		static void DestroySystem();

		static CGrannyModelInstance* New();
		static void Delete(CGrannyModelInstance* pkInst);

		static CDynamicPool<CGrannyModelInstance>		ms_kPool;

	public:
		struct FCreateDeviceObjects
		{
			void operator() (CGrannyModelInstance * pModelInstance)
			{pModelInstance->CreateDeviceObjects();}
		};

		struct FDestroyDeviceObjects
		{
			void operator() (CGrannyModelInstance * pModelInstance)
			{pModelInstance->DestroyDeviceObjects();}
		};

	public:
		CGrannyModelInstance();
		virtual ~CGrannyModelInstance();

		bool	IsEmpty();
		void	Clear();

		bool	CreateDeviceObjects();
		void	DestroyDeviceObjects();

		// Update & Render
		void	Update(DWORD dwAniFPS);
		void	UpdateLocalTime(float fElapsedTime);
		void	UpdateTransform(Math::Matrix * pMatrix, float fSecondsElapsed);

		void	UpdateSkeleton(const Math::Matrix * c_pWorldMatrix, float fLocalTime);
		void	DeformNoSkin(const Math::Matrix * c_pWorldMatrix);
		void	Deform(const Math::Matrix * c_pWorldMatrix);

		// FIXME : 현재는 하드웨어의 한계로 2장의 텍스춰로 제한이 되어있는 상태이기에 이런
		//         불안정한 아키텍춰가 가능하지만, 궁극적인 방향은 (모델 텍스춰 전부) + (효과용 텍스춰)
		//         이런식의 자동 셋팅이 이뤄져야 되지 않나 생각합니다. - [levites]
		// NOTE : 내부에 if문을 포함 시키기 보다는 조금은 번거롭지만 이렇게 함수 콜 자체를 분리
		//        시키는 것이 퍼포먼스 적인 측면에서는 더 나은 것 같습니다. - [levites]
		// NOTE : 건물은 무조건 OneTexture. 캐릭터는 경우에 따라 TwoTexture.
		void	RenderWithOneTexture();
		void	RenderWithTwoTexture();
		void	BlendRenderWithOneTexture();
		void	BlendRenderWithTwoTexture();
		void	RenderWithoutTexture();

		// Model
		CGrannyModel* GetModel();
        // ZiiNAN: Completed native deformation and renderer handles belong to this instance.
        Renderer::ActorInstanceData& GetActorRenderData() { return m_actorRenderData; }
        // ZiiNAN: GPU skinning static mesh data
        const std::vector<std::shared_ptr<const Renderer::BoneRemap>>& GetSkinningRemaps() const { return m_skinningRemaps; }
        std::shared_ptr<const Renderer::BonePalette> GetSkinningPalette() const;
        Renderer::SkinDataStatus GetSkinningStatus() const { return m_skinningStatus; }
        CGrannyMaterialPalette& GetStaticObjectMaterialPalette() { return m_kMtrlPal; }
        const Math::Matrix* GetStaticObjectWorldMatrix(int mesh) const
        {
            return m_pModel && m_meshMatrices && mesh>=0 && mesh<m_pModel->GetMeshCount() ? &m_meshMatrices[mesh] : nullptr;
        }
		void	SetMaterialImagePointer(const char* c_szImageName, CGraphicImage* pImage);
		void	SetMaterialData(const char* c_szImageName, const SMaterialData& c_rkMaterialData);
		void	SetSpecularInfo(const char* c_szMtrlName, BOOL bEnable, float fPower);
		
		void	SetMainModelPointer(CGrannyModel* pkModel, CGraphicVertexBuffer* pkSharedDefromableVertexBuffer);
		void	SetLinkedModelPointer(CGrannyModel* pkModel, CGraphicVertexBuffer* pkSharedDefromableVertexBuffer, CGrannyModelInstance** ppkSkeletonInst, bool refreshLinkedLodBinding = false);

		// Motion
		void	SetMotionPointer(const CGrannyMotion* pMotion, float blendTime=0.0f, int loopCount=0, float speedRatio=1.0f);
		void	ChangeMotionPointer(const CGrannyMotion* pMotion, int loopCount=0, float speedRatio=1.0f);
		void	SetMotionAtEnd();		
		bool	IsMotionPlaying();
		
		void	CopyMotion(CGrannyModelInstance * pModelInstance, bool bIsFreeSourceControl=false);

		// Time
		void	SetLocalTime(float fLocalTime);
		int		ResetLocalTime();
		float	GetLocalTime();
		float	GetNextTime();

		// WORK
		DWORD	GetDeformableVertexCount();
		DWORD	GetVertexCount();

		// END_OF_WORK

		// Bone & Attaching
		const float *	GetBoneMatrixPointer(int iBone) const;
		const float *	GetCompositeBoneMatrixPointer(int iBone) const;
		bool			GetMeshMatrixPointer(int iMesh, const Math::Matrix ** c_ppMatrix) const;
		bool			GetBoneIndexByName(const char * c_szBoneName, int * pBoneIndex) const;
		void			SetParentModelInstance(const CGrannyModelInstance* c_pParentModelInstance, const char * c_szBoneName);
		void			SetParentModelInstance(const CGrannyModelInstance* c_pParentModelInstance, int iBone);

		// Collision Detection
		bool	Intersect(const Math::Matrix * c_pMatrix, float * pu, float * pv, float * pt);
		void	MakeBoundBox(TBoundBox* pBoundBox, const float* mat, const float* OBBMin, const float* OBBMax, Math::Vector3* vtMin, Math::Vector3* vtMax);
		void	GetBoundBox(Math::Vector3 * vtMin, Math::Vector3* vtMax);

		// Reload Texture
		void	ReloadTexture();


	protected:
		void	__Initialize();
				
		void	__DestroyModelInstance();
		void	__DestroyMeshMatrices();
		void	__DestroyDynamicVertexBuffer();

		
		void	__CreateModelInstance();
		void	__CreateMeshMatrices();
		void	__CreateDynamicVertexBuffer();

		// WORK		
		void	__DestroyWorldPose();
		void	__CreateWorldPose(CGrannyModelInstance* pkSrcModelInst);

		bool	__CreateMeshBindingVector(CGrannyModelInstance* pkDstModelInst);
		void	__DestroyMeshBindingVector();
		
		const int* __GetMeshBoneIndices(unsigned int iMeshBinding) const;

		bool	__IsDeformableVertexBuffer();
		void	__SetSharedDeformableVertexBuffer(CGraphicVertexBuffer* pkSharedDeformableVertexBuffer);
		
		CGraphicVertexBuffer&	__GetDeformableVertexBufferRef();
		
        AssetRuntime::AnimationInstance* __GetPoseOwner() const;
        AssetRuntime::PoseView __GetCompositePose() const;
		// END_OF_WORK


		// Update & Render
		bool	UpdateWorldPose();
        void __PrepareSkinningBindings();
        bool __RefreshLinkedLodBinding();
        void __CaptureSkinningPose();
		bool	UpdateWorldMatrices(const Math::Matrix * c_pWorldMatrix);
		bool	DeformPNTVertices(void * pvDest);

		void	RenderMeshNodeListWithOneTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType);
		void	RenderMeshNodeListWithTwoTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType);
		void	RenderMeshNodeListWithoutTexture(CGrannyMesh::EType eMeshType, CGrannyMaterial::EType eMtrlType);

	protected:
		// Static Data
		CGrannyModel *					m_pModel;

		// Animation Data
        std::unique_ptr<AssetRuntime::AnimationInstance> m_animationInstance;

		// Meshes' Transform Data
		Math::Matrix *					m_meshMatrices;

		
		// Attaching Data
		const CGrannyModelInstance *	mc_pParentInstance;
		int								m_iParentBoneIndex;

		// Game Data
		float							m_fLocalTime;
		float							m_fSecondsElapsed;	

		DWORD							m_dwOldUpdateFrame;

		CGrannyMaterialPalette			m_kMtrlPal;
        Renderer::ActorInstanceData m_actorRenderData; // ZiiNAN: No animation ownership.
        std::vector<std::shared_ptr<const Renderer::BoneRemap>> m_skinningRemaps;
        std::shared_ptr<const Renderer::SkeletonLayout> m_skinningBindingDestination;
        // ZiiNAN: GPU skinning actor coverage - registered hair LOD links only.
        std::shared_ptr<const Renderer::SkeletonLayout> m_linkedLodBindingDestination;
        std::shared_ptr<Renderer::BonePalette> m_skinningPalette;
        Renderer::SkinDataStatus m_skinningStatus=Renderer::SkinDataStatus::Empty;
        bool m_skinningIssueReported=false;

		// WORK
        bool m_ownsWorldPose = false;
        std::vector<std::unique_ptr<AssetRuntime::MeshBinding>> m_meshBindings;

		// Dynamic Vertex Buffer
		CGraphicVertexBuffer*			m_pkSharedDeformableVertexBuffer;
		CGraphicVertexBuffer			m_kLocalDeformableVertexBuffer;
		bool							m_isDeformableVertexBuffer;		
		// END_OF_WORK

		// TEST
		CGrannyModelInstance** m_ppkSkeletonInst;
		// MR-12: Fix specular isolation issue
		SMaterialData material_data_;
		// MR-12: -- END OF -- Fix specular isolation issue
		// END_OF_TEST
#ifdef _TEST
		Math::Matrix TEST_matWorld;
#endif
	public:
		bool							HaveBlendThing() { return m_pModel->HaveBlendThing(); }
};
