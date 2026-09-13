#include "stdafx.h"
#include "EterLib/DrawState.h"
#include "EterLib/Camera.h"

#include "FlyingData.h"
#include "FlyTrace.h"
#include "EffectLib/EffectRenderBridge.h" // ZiiNAN: Diligent effect rendering integration.

CDynamicPool<CFlyTrace>		CFlyTrace::ms_kPool;		

void CFlyTrace::DestroySystem()
{
	ms_kPool.Destroy();
}

CFlyTrace* CFlyTrace::New()
{
	return ms_kPool.Alloc();
}

void CFlyTrace::Delete(CFlyTrace* pkInst)
{
	pkInst->Destroy();
	ms_kPool.Free(pkInst);
}

CFlyTrace::CFlyTrace()
{
	__Initialize();


}

CFlyTrace::~CFlyTrace()
{
	Destroy();
}

				
void CFlyTrace::__Initialize()
{
	m_bRectShape=false;
	m_dwColor=0;
	m_fSize=0.0f;
	m_fTailLength=0.0f;	
}

void CFlyTrace::Destroy()
{
	m_TimePositionDeque.clear();

	__Initialize();
}

void CFlyTrace::UpdateNewPosition(const Math::Vector3 & v3Position)
{
	m_TimePositionDeque.push_front(TTimePosition(CTimer::Instance().GetCurrentSecond(),v3Position));
	//Tracenf("%f %f",m_TimePositionDeque.back().first, CTimer::Instance().GetCurrentSecond());
	while(!m_TimePositionDeque.empty() && m_TimePositionDeque.back().first+m_fTailLength<CTimer::Instance().GetCurrentSecond())
	{
		m_TimePositionDeque.pop_back();
	}
}

void CFlyTrace::Create(const CFlyingData::TFlyingAttachData & rFlyingAttachData)
{
	//assert(rFlyingAttachData.bHasTail);
	m_dwColor = rFlyingAttachData.dwTailColor;
	m_fTailLength = rFlyingAttachData.fTailLength;
	m_fSize = rFlyingAttachData.fTailSize;
	m_bRectShape = rFlyingAttachData.bRectShape;
}


void CFlyTrace::Update()
{ 
	
}

//1. 알파를 쓰려면 색깔만 줄수있다.
//2. 텍스쳐를 쓰려면 알파 없다-_-


struct TFlyVertex
{
	Math::Vector3 p;
	DWORD c;
	Math::Vector2 t;
	TFlyVertex(){};
	TFlyVertex(const Math::Vector3& p, DWORD c, const Math::Vector2 & t):p(p),c(c),t(t){}
};

struct TFlyVertexSet
{
	TFlyVertex v[6];
	TFlyVertexSet(TFlyVertex * pv)
	{
		memcpy(v,pv,sizeof(v));
	}
	bool operator < (const TFlyVertexSet& ) const
	{
		return false;
	}
	TFlyVertexSet & operator = ( const TFlyVertexSet& rhs )
	{
		memcpy(v,rhs.v,sizeof(v));
		return *this;
	}
};

typedef std::vector<std::pair<float, TFlyVertexSet> > TFlyVertexSetVector;

void CFlyTrace::Render()
{
    Renderer::EffectResources resources;
    EffectRenderScope effectScope(resources,"flying-trace",Renderer::EffectPart::FlyTrace);
	if (m_TimePositionDeque.size()<=1)
		return;
	TFlyVertexSetVector VSVector;

	//DRAWSTATE.SaveRenderState(Renderer::StateZFunc,Renderer::CompareLess);
	DRAWSTATE.SaveRenderState(Renderer::StateZFunc,Renderer::CompareLess);
	//DRAWSTATE.SaveRenderState(Renderer::StateZWriteEnable,FALSE);

	Math::Matrix matWorld;
	Math::MatrixIdentity(&matWorld);
	
	DRAWSTATE.SaveTransform(Renderer::MatrixWorld, &matWorld);
	DRAWSTATE.SaveRenderState(Renderer::StateCullMode, Renderer::CullNone);
	
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateSrcBlend, Renderer::BlendSrcAlpha);
	DRAWSTATE.SaveRenderState(Renderer::StateDestBlend, Renderer::BlendOne);

	DRAWSTATE.SaveRenderState(Renderer::StateAlphaTestEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaFunc, Renderer::CompareGreater);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaRef, 0x00000000);

	DRAWSTATE.SaveRenderState(Renderer::StateBlendOp, Renderer::BlendOpAdd );
	//DRAWSTATE.SaveRenderState(Renderer::StateBlendOp, Renderer::BlendOpAdd );
	
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg1, Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorArg2, Renderer::ArgTexture);
	//DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, /*(m_bUseTexture)?Renderer::TextureOpSelectArg2:*/Renderer::TextureOpSelectArg2);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageColorOp, Renderer::TextureOpSelectArg1);

	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg1, Renderer::ArgDiffuse);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaArg2, Renderer::ArgTexture);
	//DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, Renderer::TextureOpModulate);
	DRAWSTATE.SetTextureStageState(0, Renderer::StageAlphaOp, /*(m_bUseTexture)?Renderer::TextureOpSelectArg2:*/Renderer::TextureOpSelectArg1);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetRenderState(Renderer::StateLighting, FALSE);
	DRAWSTATE.SetTexture(0, NULL);
	DRAWSTATE.SetTexture(1, NULL);
	
	
	Math::Matrix m;
	CScreen s;s.UpdateViewMatrix();
	CCamera * pCurrentCamera = CCameraManager::Instance().GetCurrentCamera();
	if (!pCurrentCamera)
		return;

	const Math::Matrix & M = pCurrentCamera->GetViewMatrix();
	Math::MatrixIdentity(&m);
	Math::Vector3 F(pCurrentCamera->GetView());
	m._31 = F.x;
	m._32 = F.y;
	m._33 = F.z;

	Frustum & frustum = s.GetFrustum();
	//frustum.BuildViewFrustum(ms_matView * ms_matProj);

	TTimePositionDeque::iterator it1, it2;
	it2 = it1 = m_TimePositionDeque.begin();
	++it2;
	for(;it2!=m_TimePositionDeque.end();++it2,++it1)
	{
		const Math::Vector3& rkOld=it1->second;
		const Math::Vector3& rkNew=it2->second;
		Math::Vector3 B = rkNew - rkOld;
		
		float radius = std::max(fabs(B.x),std::max(fabs(B.y),fabs(B.z)))/2;
		Vector3d c(it1->second.x+B.x*0.5f,
			it1->second.y+B.y*0.5f,
			it1->second.z+B.z*0.5f
			);
		if (frustum.ViewVolumeTest(c, radius)==VS_OUTSIDE)
			continue;

		float rate1 = (1-(CTimer::Instance().GetCurrentSecond()-it1->first)/m_fTailLength);
		float rate2 = (1-(CTimer::Instance().GetCurrentSecond()-it2->first)/m_fTailLength);
		float size1 = m_fSize;
		float size2 = m_fSize;
		if (!m_bRectShape)
		{
			size1 *= rate1;
			size2 *= rate2;
		}
		TFlyVertex v[6] = 
		{
			TFlyVertex(Math::Vector3(0.0f,size1,0.0f), m_dwColor,Math::Vector2(0.0f,0.0f)),
			TFlyVertex(Math::Vector3(-size1,0.0f,0.0f),m_dwColor,Math::Vector2(0.0f,0.5f)),
			TFlyVertex(Math::Vector3(size1,0.0f,0.0f), m_dwColor,Math::Vector2(0.5f,0.0f)),
			TFlyVertex(Math::Vector3(-size2,0.0f,0.0f),m_dwColor,Math::Vector2(0.5f,1.0f)),
			TFlyVertex(Math::Vector3(size2,0.0f,0.0f), m_dwColor,Math::Vector2(1.0f,0.5f)),
			TFlyVertex(Math::Vector3(0.0f,-size2,0.0f),m_dwColor,Math::Vector2(1.0f,1.0f)),
	
			/*TVertex(Math::Vector3(0.0f,size1,0.0f), ((DWORD)(0x40*rate1)<<24) + 0x0000ff,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(-size1,0.0f,0.0f),((DWORD)(0x40*rate1)<<24) + 0x0000ff,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(size1,0.0f,0.0f), ((DWORD)(0x40*rate1)<<24) + 0x0000ff,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(-size2,0.0f,0.0f),((DWORD)(0x40*rate2)<<24) + 0x0000ff,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(size2,0.0f,0.0f), ((DWORD)(0x40*rate2)<<24) + 0x0000ff,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(0.0f,-size2,0.0f),((DWORD)(0x40*rate2)<<24) + 0x0000ff,Math::Vector2(0.0f,0.0f)),*/

			/*TVertex(Math::Vector3(0.0f,size1,0.0f),0x20ff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(-size1,0.0f,0.0f),0x20ff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(size1,0.0f,0.0f),0x20ff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(-size2,0.0f,0.0f),0x20ff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(size2,0.0f,0.0f),0x20ff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(0.0f,-size2,0.0f),0x20ff0000,Math::Vector2(0.0f,0.0f)),*/

			/*TVertex(Math::Vector3(0.0f,size1,0.0f),0xffff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(-size1,0.0f,0.0f),0xffff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(size1,0.0f,0.0f),0xffff0000,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(-size2,0.0f,0.0f),0xff0000ff,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(size2,0.0f,0.0f),0xff0000ff,Math::Vector2(0.0f,0.0f)),
			TVertex(Math::Vector3(0.0f,-size2,0.0f),0xff0000ff,Math::Vector2(0.0f,0.0f)),*/
		};


		Math::Vector3 E(M._41,M._42,M._43);
		E = pCurrentCamera->GetEye();
		E-=it1->second;

		Math::Vector3 P;
		Math::Vec3Cross(&P, &B,&E);

		Math::Vector3 U;
		Math::Vec3Cross(&U,&F,&P);
		Math::Vec3Normalize(&U,&U);
		Math::Vector3 R;
		Math::Vec3Cross(&R,&F,&U);
		//Math::MatrixIdentity(&m);
		m._21 = U.x;
		m._22 = U.y;
		m._23 = U.z;
		m._11 = R.x;
		m._12 = R.y;
		m._13 = R.z;
		int i;
		for(i=0;i<6;i++)
			Math::Vec3TransformNormal(&v[i].p,&v[i].p,&m);
		for(i=0;i<3;i++)
			v[i].p += it1->second;
		for(;i<6;i++)
			v[i].p += it2->second;
		//for(i=0;i<6;i++)
		//	Tracenf("#%d:%f %f %f", i, v[i].p.x,v[i].p.y,v[i].p.z);
		
		VSVector.push_back(std::make_pair(-Math::Vec3Dot(&E,&pCurrentCamera->GetView()),TFlyVertexSet(v)));
	}

	std::sort(VSVector.begin(),VSVector.end());

	for(TFlyVertexSetVector::iterator it = VSVector.begin();it!=VSVector.end();++it)
	{
		EffectRenderBridge::Submit(Renderer::TopologyTriangleStrip, 4, it->second.v, sizeof(TVertex));
	}
	DRAWSTATE.RestoreRenderState(Renderer::StateDestBlend);
	DRAWSTATE.RestoreRenderState(Renderer::StateSrcBlend);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateCullMode);
	DRAWSTATE.RestoreTransform(Renderer::MatrixWorld);
	//DRAWSTATE.RestoreRenderState(Renderer::StateZWriteEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateZFunc);
	DRAWSTATE.RestoreRenderState(Renderer::StateBlendOp);

	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaTestEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaFunc);
	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaRef);

}
