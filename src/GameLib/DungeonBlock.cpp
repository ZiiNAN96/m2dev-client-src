#include "StdAfx.h"
#include "DungeonBlock.h"

#include "EterLib/DrawState.h"
#include "EterLib/MaterialStateSnapshot.h"
#include "EterLib/StaticObjectTextureLoader.h"
#include "Renderer/WorldRenderData.h"

class CDungeonModelInstance : public CGrannyModelInstance
{
		std::vector<std::array<float,10>> m_vertices;
		std::vector<uint16_t> m_indices;
        std::vector<uint32_t> m_indices32;
		Renderer::WorldResources m_resources;
		static void Submit(const void* native,const Renderer::SpecialMeshDraw& group)
		{
			auto& self=*const_cast<CDungeonModelInstance*>(static_cast<const CDungeonModelInstance*>(native));
			auto* renderer=Renderer::worldRenderer;
			if(!renderer || !Renderer::worldSurfaceFrame) return;
			Renderer::EffectDraw draw; draw.strip=false; std::string error;
            const auto indexCount = self.m_indices32.empty() ? self.m_indices.size() : self.m_indices32.size();
			if(!CaptureMaterialState(draw,error,true) || group.material>=self.m_kMtrlPal.GetMaterialCount() ||
				group.firstIndex>indexCount || group.indexCount>indexCount-group.firstIndex) { renderer->ReportFailure(); return; }
			auto& material=self.m_kMtrlPal.GetMaterialRef(group.material);
			auto load=[&](CGraphicImage* image) -> Renderer::TerrainTexturePtr {
				if(!image) return {}; auto& texture=self.m_resources.textures[image->GetFileName()];
				if(!texture) texture=LoadStaticObjectTextureFile(image->GetFileName(),*renderer); return texture;
			};
			auto texture=load(material.GetImagePointer(0)); draw.textured=material.GetImagePointer(0)!=nullptr;
			draw.secondaryTexture=load(material.GetImagePointer(1));
			if((draw.textured && !texture) || (material.GetImagePointer(1) && !draw.secondaryTexture)) { renderer->ReportFailure(); return; }
			std::vector<Renderer::EffectVertex> vertices; vertices.reserve(group.indexCount);
			for(uint32_t i=0;i<group.indexCount;++i) {
                const auto index=self.m_indices32.empty() ? self.m_indices[group.firstIndex+i] : self.m_indices32[group.firstIndex+i];
				if(index>=group.vertexCount || uint64_t(group.baseVertex)+index>=self.m_vertices.size()) { renderer->ReportFailure(); return; }
				const auto& v=self.m_vertices[group.baseVertex+index];
				vertices.push_back({{v[0],v[1],v[2]},0xffffffff,{v[6],v[7]}});
				if(draw.secondaryTexture && draw.secondaryCoordinates==1) draw.secondaryUV.push_back({v[8],v[9]});
			}
			renderer->Draw(vertices.data(),uint32_t(vertices.size()),texture,draw,Renderer::WorldPart::Dungeon);
		}
	public:
		CDungeonModelInstance() {}
		virtual ~CDungeonModelInstance() { if(Renderer::worldRenderer) Renderer::worldRenderer->ReleaseBindings(); }
		bool CaptureDiligentSource()
		{
			if(!Renderer::worldRenderer) return true;
			// ZiiNAN: Backend-neutral graphics resource ownership
			if(!m_pModel || m_pModel->GetDeformVertexCount()) return false;
			if(m_pModel->GetRigidVertexBytes()<size_t(m_pModel->GetRigidVertexCount())*40) return false;
			void* vertices=nullptr; void* indices=nullptr;
			if(!m_pModel->LockVertices(&indices,&vertices)) return false;
            m_vertices.resize(m_pModel->GetRigidVertexCount());
            m_indices.clear(); m_indices32.clear();
            if (m_pModel->GetIndexWidth() == AssetRuntime::IndexWidth::UInt32) {
                m_indices32.resize(m_pModel->GetIdxCount());
                memcpy(m_indices32.data(),indices,m_indices32.size()*sizeof(uint32_t));
            } else {
                m_indices.resize(m_pModel->GetIdxCount());
                memcpy(m_indices.data(),indices,m_indices.size()*sizeof(uint16_t));
            }
			memcpy(m_vertices.data(),vertices,m_vertices.size()*40);
			m_pModel->UnlockVertices(); return true;
		}

		void RenderDungeonBlock()
		{
			if (IsEmpty())
				return;

			if (m_pModel->GetRigidVertexCount()>0)
			{
				Renderer::SpecialMeshScope special({this,Submit});
				RenderMeshNodeListWithTwoTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
			}
		}

		void RenderDungeonBlockShadow()
		{
			if (IsEmpty())
				return;

			DRAWSTATE.SetRenderState(Renderer::StateTextureFactor, 0xffffffff);
			DRAWSTATE.SaveTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgTFactor);
			DRAWSTATE.SaveTextureStageState(0, Renderer::StageColorOp,   Renderer::TextureOpSelectArg1);
			DRAWSTATE.SaveTextureStageState(0, Renderer::StageAlphaOp,   Renderer::TextureOpDisable);
			DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
			DRAWSTATE.SaveRenderState(Renderer::StateSrcBlend, Renderer::BlendZero);
			DRAWSTATE.SaveRenderState(Renderer::StateDestBlend, Renderer::BlendSrcColor);

			if (m_pModel->GetRigidVertexCount()>0)
			{
				RenderMeshNodeListWithoutTexture(CGrannyMesh::TYPE_RIGID, CGrannyMaterial::TYPE_BLEND_PNT);
			}

			DRAWSTATE.RestoreTextureStageState(0, Renderer::StageColorArg1);
			DRAWSTATE.RestoreTextureStageState(0, Renderer::StageColorOp);
			DRAWSTATE.RestoreTextureStageState(0, Renderer::StageAlphaOp);
			DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
			DRAWSTATE.RestoreRenderState(Renderer::StateSrcBlend);
			DRAWSTATE.RestoreRenderState(Renderer::StateDestBlend);
		}
};


struct FUpdate
{
	float fElapsedTime;
	Math::Matrix * pmatWorld;
	void operator() (CGrannyModelInstance * pInstance)
	{
		pInstance->Update(CGrannyModelInstance::ANIFPS_MIN);
		pInstance->UpdateLocalTime(fElapsedTime);
		pInstance->Deform(pmatWorld);
	}
};

void CDungeonBlock::Update()
{
	Transform();

	FUpdate Update;
	Update.fElapsedTime = 0.0f;
	Update.pmatWorld = &m_worldMatrix;
	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), Update);
}

struct FRender
{
	void operator() (CDungeonModelInstance * pInstance)
	{
		pInstance->RenderDungeonBlock();
	}
};

void CDungeonBlock::Render()
{
//	if (!isShow())
//		return;

	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), FRender());
}

struct FRenderShadow
{
	void operator() (CDungeonModelInstance * pInstance)
	{
		pInstance->RenderDungeonBlockShadow();
	}
};

void CDungeonBlock::OnRenderShadow()
{
	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), FRenderShadow());
}

struct FBoundBox
{
	Math::Vector3 * m_pv3Min;
	Math::Vector3 * m_pv3Max;

	FBoundBox(Math::Vector3 * pv3Min, Math::Vector3 * pv3Max)
	{
		m_pv3Min = pv3Min;
		m_pv3Max = pv3Max;
	}
	void operator() (CGrannyModelInstance * pInstance)
	{
		pInstance->GetBoundBox(m_pv3Min, m_pv3Max);
	}
};

bool CDungeonBlock::GetBoundingSphere(Math::Vector3 & v3Center, float & fRadius)
{
	v3Center = m_v3Center;
	fRadius = m_fRadius;
	Math::Vec3TransformCoord(&v3Center, &v3Center, &GetTransform());
	return true;
}

void CDungeonBlock::OnUpdateCollisionData(const CStaticCollisionDataVector * pscdVector)
{
	assert(pscdVector);
	CStaticCollisionDataVector::const_iterator it;
	for(it = pscdVector->begin();it!=pscdVector->end();++it)
	{
		AddCollision(&(*it),&GetTransform());
	}
}

void CDungeonBlock::OnUpdateHeighInstance(CAttributeInstance * pAttributeInstance)
{
	assert(pAttributeInstance);
	SetHeightInstance(pAttributeInstance);	
}

bool CDungeonBlock::OnGetObjectHeight(float fX, float fY, float * pfHeight)
{
	if (m_pHeightAttributeInstance && m_pHeightAttributeInstance->GetHeight(fX, fY, pfHeight))
		return true;
	return false;
}

void CDungeonBlock::BuildBoundingSphere()
{
	Math::Vector3 v3Min, v3Max;
	for_each(m_ModelInstanceContainer.begin(), m_ModelInstanceContainer.end(), FBoundBox(&v3Min, &v3Max));

	m_v3Center = (v3Min+v3Max) * 0.5f;
	const auto vv = (v3Max - v3Min);
	m_fRadius = Math::Vec3Length(&vv)*0.5f + 150.0f; // extra length for attached objects
}

bool CDungeonBlock::Intersect(float * pfu, float * pfv, float * pft)
{
	TModelInstanceContainer::iterator itor = m_ModelInstanceContainer.begin();
	for (; itor != m_ModelInstanceContainer.end(); ++itor)
	{
		CDungeonModelInstance * pInstance = *itor;
		if (pInstance->Intersect(&CGraphicObjectInstance::GetTransform(), pfu, pfv, pft))
			return true;
	}

	return false;
}

void CDungeonBlock::GetBoundBox(Math::Vector3 * pv3Min, Math::Vector3 * pv3Max)
{
	pv3Min->x = +10000000.0f;
	pv3Min->y = +10000000.0f;
	pv3Min->z = +10000000.0f;
	pv3Max->x = -10000000.0f;
	pv3Max->y = -10000000.0f;
	pv3Max->z = -10000000.0f;

	TModelInstanceContainer::iterator itor = m_ModelInstanceContainer.begin();
	for (; itor != m_ModelInstanceContainer.end(); ++itor)
	{
		CDungeonModelInstance * pInstance = *itor;

		Math::Vector3 v3Min;
		Math::Vector3 v3Max;
		pInstance->GetBoundBox(&v3Min, &v3Max);

		pv3Min->x = std::min(v3Min.x, pv3Min->x);
		pv3Min->y = std::min(v3Min.x, pv3Min->y);
		pv3Min->z = std::min(v3Min.x, pv3Min->z);
		pv3Max->x = std::max(v3Max.x, pv3Max->x);
		pv3Max->y = std::max(v3Max.x, pv3Max->y);
		pv3Max->z = std::max(v3Max.x, pv3Max->z);
	}
}

bool CDungeonBlock::Load(const char * c_szFileName)
{
	Destroy();

	m_pThing = (CGraphicThing *)CResourceManager::Instance().GetResourcePointer(c_szFileName);

	m_pThing->AddReference();
	if (m_pThing->GetModelCount() <= 0)
	{
		TraceError("CDungeonBlock::Load(filename=%s) - model count is %d\n", c_szFileName, m_pThing->GetModelCount());
		return false;
	}

	m_ModelInstanceContainer.reserve(m_pThing->GetModelCount());

	for (int i = 0; i < m_pThing->GetModelCount(); ++i)
	{
		CDungeonModelInstance * pModelInstance = new CDungeonModelInstance;
		pModelInstance->SetMainModelPointer(m_pThing->GetModelPointer(i), &m_kDeformableVertexBuffer);
		if(!pModelInstance->CaptureDiligentSource()) {
			if(Renderer::worldRenderer) Renderer::worldRenderer->ReportFailure();
			delete pModelInstance; return false;
		}
		DWORD dwVertexCount = pModelInstance->GetVertexCount();
		m_kDeformableVertexBuffer.Destroy();
		m_kDeformableVertexBuffer.Create(
			dwVertexCount,
			Renderer::VertexPosition|Renderer::VertexNormal|Renderer::VertexTex1);
		m_ModelInstanceContainer.push_back(pModelInstance);
	}

	return true;
}

void CDungeonBlock::__Initialize()
{
	m_v3Center = Math::Vector3(0.0f, 0.0f, 0.0f);
	m_fRadius = 0.0f;

	m_pThing = NULL;
}

void CDungeonBlock::Destroy()
{
	if (m_pThing)
	{
		m_pThing->Release();
		m_pThing = NULL;
	}

	stl_wipe(m_ModelInstanceContainer);

	__Initialize();
}

CDungeonBlock::CDungeonBlock()
{
	__Initialize();
}
CDungeonBlock::~CDungeonBlock()
{
	Destroy();
}
