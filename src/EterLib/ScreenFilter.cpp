#include "StdAfx.h"
#include "ScreenFilter.h"
#include "DrawState.h"

void CScreenFilter::Render()
{
	if (!m_bEnable)
		return;

	DRAWSTATE.SaveTransform(Renderer::MatrixProjection, &ms_matIdentity);
	DRAWSTATE.SaveTransform(Renderer::MatrixView, &ms_matIdentity);
	DRAWSTATE.SetTransform(Renderer::MatrixWorld, &ms_matIdentity);
	DRAWSTATE.SaveRenderState(Renderer::StateAlphaBlendEnable, TRUE);
	DRAWSTATE.SaveRenderState(Renderer::StateSrcBlend, m_bySrcType);
	DRAWSTATE.SaveRenderState(Renderer::StateDestBlend, m_byDestType);

	SetOrtho2D(CScreen::ms_iWidth, CScreen::ms_iHeight, 400.0f);
	SetDiffuseColor(m_Color.r, m_Color.g, m_Color.b, m_Color.a);
	RenderBar2d(0, 0, CScreen::ms_iWidth, CScreen::ms_iHeight);

	DRAWSTATE.RestoreRenderState(Renderer::StateAlphaBlendEnable);
	DRAWSTATE.RestoreRenderState(Renderer::StateSrcBlend);
	DRAWSTATE.RestoreRenderState(Renderer::StateDestBlend);
	DRAWSTATE.RestoreTransform(Renderer::MatrixView);
	DRAWSTATE.RestoreTransform(Renderer::MatrixProjection);
}

void CScreenFilter::SetEnable(BOOL /*bFlag*/)
{
	m_bEnable = FALSE;
}

void CScreenFilter::SetBlendType(BYTE bySrcType, BYTE byDestType)
{
	m_bySrcType = bySrcType;
	m_byDestType = byDestType;
}
void CScreenFilter::SetColor(const Math::Color & c_rColor)
{
	m_Color = c_rColor;
}

CScreenFilter::CScreenFilter()
{
	m_bEnable = FALSE;
	m_bySrcType = Renderer::BlendSrcAlpha;
	m_byDestType = Renderer::BlendInvSrcAlpha;
	m_Color = Math::Color(0.0f, 0.0f, 0.0f, 0.0f);
}
CScreenFilter::~CScreenFilter()
{
}
