#include "StdAfx.h"
#include "StateManager.h"
#include "GrpLightManager.h"

//#define StateManager_Assert(a) if (!(a)) puts("assert"#a)
#define StateManager_Assert(a) assert(a)

void CRenderState::InitializeDrawDefaults()
{
    // ZiiNAN: Legacy D3D9 renderer removed from production path.
    // Explicit CPU defaults. No device exists to seed or restore from.
    for (auto& value : m_CurrentState.m_RenderStates) value=0;
    m_CurrentState.m_RenderStates[D3DRS_TEXTUREFACTOR]=0xffffffff;
    m_CurrentState.m_RenderStates[D3DRS_BLENDOPALPHA]=D3DBLENDOP_ADD;
    m_CurrentState.m_RenderStates[D3DRS_SRCBLENDALPHA]=D3DBLEND_ONE;
    m_CurrentState.m_RenderStates[D3DRS_DESTBLENDALPHA]=D3DBLEND_ZERO;
    for (DWORD stage=0; stage<8; ++stage) {
        for (auto& value : m_CurrentState.m_TextureStates[stage]) value=0;
        for (auto& value : m_CurrentState.m_SamplerStates[stage]) value=0;
        m_CurrentState.m_TextureStates[stage][D3DTSS_COLORARG0]=D3DTA_CURRENT;
        m_CurrentState.m_TextureStates[stage][D3DTSS_ALPHAARG0]=D3DTA_CURRENT;
        m_CurrentState.m_TextureStates[stage][D3DTSS_RESULTARG]=D3DTA_CURRENT;
        m_CurrentState.m_SamplerStates[stage][D3DSAMP_ADDRESSW]=D3DTADDRESS_WRAP;
        m_CurrentState.m_SamplerStates[stage][D3DSAMP_MAXANISOTROPY]=1;
        m_lightEnabled[stage]=FALSE; m_lightValid[stage]=false; m_lights[stage]={};
    }
}

HRESULT CRenderState::SetViewport(const D3DVIEWPORT9* viewport)
{
    if(!viewport || !viewport->Width || !viewport->Height || viewport->MinZ>viewport->MaxZ) return D3DERR_INVALIDCALL;
    m_viewport=*viewport;
    return S_OK;
}
HRESULT CRenderState::LightEnable(DWORD index,BOOL enabled)
{
    if(index>=8) return D3DERR_INVALIDCALL;
    m_lightEnabled[index]=enabled;
    return S_OK;
}

HRESULT CRenderState::SetRenderTarget(DWORD index,IDirect3DSurface9* surface)
{
    return D3DERR_INVALIDCALL;
}
HRESULT CRenderState::SetDepthStencilSurface(IDirect3DSurface9* surface)
{
    return D3DERR_INVALIDCALL;
}





void CRenderState::SetLight(DWORD index, CONST D3DLIGHT9* pLight)
{
	assert(index < 8);

    m_lights[index]=*pLight; m_lightValid[index]=true;
}

void CRenderState::GetLight(DWORD index, D3DLIGHT9* pLight)
{
	assert(index < 8);
	*pLight = m_lights[index];
}

void CRenderState::SetScissorRect(const RECT& c_rRect)
{
    m_scissor=c_rRect;
}

void CRenderState::GetScissorRect(RECT* pRect)
{
    *pRect=m_scissor;
}

bool CRenderState::BeginScene()
{
    ResetNativeCounters(); m_bScene=true; return true;
}

void CRenderState::EndScene()
{
    m_bScene=false;
}

CRenderState::CRenderState()
{
    m_bScene=false; m_bForce=false;
    m_dwBestMinFilter=m_dwBestMagFilter=D3DTEXF_ANISOTROPIC;
    SetDefaultState();
#ifdef _DEBUG
    m_iDrawCallCount=m_iLastDrawCallCount=0;
#endif
}

CRenderState::~CRenderState()
{

}

void CRenderState::SetBestFiltering(DWORD dwStage)
{
	SetSamplerState(dwStage, D3DSAMP_MINFILTER, m_dwBestMinFilter);
	SetSamplerState(dwStage, D3DSAMP_MAGFILTER, m_dwBestMagFilter);
	SetSamplerState(dwStage, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);
}

void CRenderState::Restore()
{
    // Draw descriptions are CPU values, with nothing to resend to a device.
}

void CRenderState::SetDefaultState()
{
    for(auto& binding:m_textureBindings) binding={};
	m_CurrentState.ResetState();
    InitializeDrawDefaults();

	for (auto& stack : m_RenderStateStack)
		stack.clear();

	for (auto& stageStacks : m_SamplerStateStack)
		for (auto& stack : stageStacks)
			stack.clear();

	for (auto& stageStacks : m_TextureStageStateStack)
		for (auto& stack : stageStacks)
			stack.clear();

	for (auto& stack : m_TransformStack)
		stack.clear();

	for (auto& stack : m_TextureStack)
		stack.clear();

	for (auto& stack : m_StreamStack)
		stack.clear();

	m_MaterialStack.clear();
	m_FVFStack.clear();
	m_PixelShaderStack.clear();
	m_VertexShaderStack.clear();
	m_VertexDeclarationStack.clear();
	m_VertexProcessingStack.clear();
	m_IndexStack.clear();

	m_bScene = false;
	m_bForce = true;

	D3DXMATRIX matIdentity;
	D3DXMatrixIdentity(&matIdentity);

	SetTransform(Renderer::MatrixWorld, &matIdentity);
	SetTransform(Renderer::MatrixView, &matIdentity);
	SetTransform(Renderer::MatrixProjection, &matIdentity);

	D3DMATERIAL9 DefaultMat;
	ZeroMemory(&DefaultMat, sizeof(D3DMATERIAL9));

	DefaultMat.Diffuse.r = 1.0f;
	DefaultMat.Diffuse.g = 1.0f;
	DefaultMat.Diffuse.b = 1.0f;
	DefaultMat.Diffuse.a = 1.0f;
	DefaultMat.Ambient.r = 1.0f;
	DefaultMat.Ambient.g = 1.0f;
	DefaultMat.Ambient.b = 1.0f;
	DefaultMat.Ambient.a = 1.0f;
	DefaultMat.Emissive.r = 0.0f;
	DefaultMat.Emissive.g = 0.0f;
	DefaultMat.Emissive.b = 0.0f;
	DefaultMat.Emissive.a = 0.0f;
	DefaultMat.Specular.r = 0.0f;
	DefaultMat.Specular.g = 0.0f;
	DefaultMat.Specular.b = 0.0f;
	DefaultMat.Specular.a = 0.0f;
	DefaultMat.Power = 0.0f;

	SetMaterial(&DefaultMat);

	SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_SPECULARMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_AMBIENTMATERIALSOURCE, D3DMCS_MATERIAL);
	SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE, D3DMCS_MATERIAL);

	//SetRenderState(D3DRS_LINEPATTERN, 0xFFFFFFFF);
	SetRenderState(D3DRS_LASTPIXEL, TRUE);
	SetRenderState(D3DRS_ALPHAREF, 1);
	SetRenderState(D3DRS_ALPHAFUNC, D3DCMP_GREATEREQUAL);
	//SetRenderState(D3DRS_ZVISIBLE, FALSE);
	SetRenderState(D3DRS_FOGSTART, 0);
	SetRenderState(D3DRS_FOGEND, 0);
	SetRenderState(D3DRS_FOGDENSITY, 0);
	//SetRenderState(D3DRS_EDGEANTIALIAS, TRUE);
	//SetRenderState(D3DRS_ZBIAS, 0);
	SetRenderState(D3DRS_STENCILWRITEMASK, 0xFFFFFFFF);
	SetRenderState(D3DRS_AMBIENT, 0x00000000);
	SetRenderState(D3DRS_LOCALVIEWER, TRUE);
	SetRenderState(D3DRS_NORMALIZENORMALS, FALSE);
	SetRenderState(D3DRS_VERTEXBLEND, D3DVBF_DISABLE);
	SetRenderState(D3DRS_CLIPPLANEENABLE, 0);
	SaveVertexProcessing(FALSE);
	SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, TRUE);
	SetRenderState(D3DRS_MULTISAMPLEMASK, 0xFFFFFFFF);
	SetRenderState(D3DRS_PATCHEDGESTYLE, D3DPATCHEDGE_CONTINUOUS);
	SetRenderState(D3DRS_INDEXEDVERTEXBLENDENABLE, FALSE);
	SetRenderState(D3DRS_COLORWRITEENABLE, 0xFFFFFFFF);
	SetRenderState(D3DRS_FILLMODE, D3DFILL_SOLID);
	SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	SetRenderState(D3DRS_CULLMODE, D3DCULL_CW);
	SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	SetRenderState(D3DRS_FOGENABLE, FALSE);
	SetRenderState(D3DRS_FOGCOLOR, 0xFF000000);
	// MR-14: Fog update by Alaric
	SetRenderState(D3DRS_FOGTABLEMODE, D3DFOG_NONE);
	// MR-14: -- END OF -- Fog update by Alaric
	SetRenderState(D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR);
	SetRenderState(D3DRS_RANGEFOGENABLE, FALSE);
	SetRenderState(D3DRS_ZENABLE, TRUE);
	SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL);
	SetRenderState(D3DRS_ZWRITEENABLE, TRUE);
	SetRenderState(D3DRS_DITHERENABLE, TRUE);
	SetRenderState(D3DRS_STENCILENABLE, FALSE);
	SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	SetRenderState(D3DRS_CLIPPING, TRUE);
	SetRenderState(D3DRS_LIGHTING, FALSE);
	SetRenderState(D3DRS_SPECULARENABLE, FALSE);
	SetRenderState(D3DRS_COLORVERTEX, FALSE);
	SetRenderState(D3DRS_WRAP0, 0);
	SetRenderState(D3DRS_WRAP1, 0);
	SetRenderState(D3DRS_WRAP2, 0);
	SetRenderState(D3DRS_WRAP3, 0);
	SetRenderState(D3DRS_WRAP4, 0);
	SetRenderState(D3DRS_WRAP5, 0);
	SetRenderState(D3DRS_WRAP6, 0);
	SetRenderState(D3DRS_WRAP7, 0);

	SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_MODULATE);
	SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(0, D3DTSS_COLORARG2, D3DTA_CURRENT);
	SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(0, D3DTSS_ALPHAARG2, D3DTA_CURRENT);
	SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);

	SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(1, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(1, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(1, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(1, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(2, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(2, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(2, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(2, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(2, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(2, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(3, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(3, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(3, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(3, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(3, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(3, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(4, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(4, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(4, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(4, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(4, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(4, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(5, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(5, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(5, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(5, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(5, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(5, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(6, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(6, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(6, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(6, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(6, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(6, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(7, D3DTSS_COLOROP, D3DTOP_DISABLE);
	SetTextureStageState(7, D3DTSS_COLORARG1, D3DTA_TEXTURE);
	SetTextureStageState(7, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
	SetTextureStageState(7, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
	SetTextureStageState(7, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
	SetTextureStageState(7, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

	SetTextureStageState(0, D3DTSS_TEXCOORDINDEX, 0);
	SetTextureStageState(1, D3DTSS_TEXCOORDINDEX, 1);
	SetTextureStageState(2, D3DTSS_TEXCOORDINDEX, 2);
	SetTextureStageState(3, D3DTSS_TEXCOORDINDEX, 3);
	SetTextureStageState(4, D3DTSS_TEXCOORDINDEX, 4);
	SetTextureStageState(5, D3DTSS_TEXCOORDINDEX, 5);
	SetTextureStageState(6, D3DTSS_TEXCOORDINDEX, 6);
	SetTextureStageState(7, D3DTSS_TEXCOORDINDEX, 7);

	SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(1, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(2, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(3, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(3, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(3, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(4, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(4, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(4, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(5, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(5, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(5, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(6, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(6, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(6, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(7, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
	SetSamplerState(7, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	SetSamplerState(7, D3DSAMP_MIPFILTER, D3DTEXF_LINEAR);

	SetSamplerState(0, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(0, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(1, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(1, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(2, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(2, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(3, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(3, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(4, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(4, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(5, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(5, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(6, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(6, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);
	SetSamplerState(7, D3DSAMP_ADDRESSU, D3DTADDRESS_WRAP);
	SetSamplerState(7, D3DSAMP_ADDRESSV, D3DTADDRESS_WRAP);

	SetTextureStageState(0, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(1, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(2, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(3, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(4, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(5, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(6, D3DTSS_TEXTURETRANSFORMFLAGS, 0);
	SetTextureStageState(7, D3DTSS_TEXTURETRANSFORMFLAGS, 0);

	SetTexture(0, NULL);
	SetTexture(1, NULL);
	SetTexture(2, NULL);
	SetTexture(3, NULL);
	SetTexture(4, NULL);
	SetTexture(5, NULL);
	SetTexture(6, NULL);
	SetTexture(7, NULL);

	SetPixelShader(0);
	SetFVF(D3DFVF_XYZ);

	D3DXVECTOR4 av4Null[STATEMANAGER_MAX_VCONSTANTS];
	memset(av4Null, 0, sizeof(av4Null));
	SetVertexShaderConstant(0, av4Null, STATEMANAGER_MAX_VCONSTANTS);
	SetPixelShaderConstant(0, av4Null, STATEMANAGER_MAX_PCONSTANTS);

	m_bForce = false;
}

// Material
void CRenderState::SaveMaterial()
{
	m_MaterialStack.push_back(m_CurrentState.m_D3DMaterial);
}

void CRenderState::SaveMaterial(const D3DMATERIAL9* pMaterial)
{
	m_MaterialStack.push_back(m_CurrentState.m_D3DMaterial);
	SetMaterial(pMaterial);
}

void CRenderState::RestoreMaterial()
{
	SetMaterial(&m_MaterialStack.back());
	m_MaterialStack.pop_back();
}

void CRenderState::SetMaterial(const D3DMATERIAL9* pMaterial)
{
	m_CurrentState.m_D3DMaterial = *pMaterial;
}

void CRenderState::GetMaterial(D3DMATERIAL9* pMaterial)
{
	// Set the renderstate and remember it.
	*pMaterial = m_CurrentState.m_D3DMaterial;
}

// Renderstates
DWORD CRenderState::GetRenderState(D3DRENDERSTATETYPE Type)
{
	return m_CurrentState.m_RenderStates[Type];
}







void CRenderState::SaveRenderState(D3DRENDERSTATETYPE Type, DWORD dwValue)
{
	m_RenderStateStack[Type].push_back(m_CurrentState.m_RenderStates[Type]);
	SetRenderState(Type, dwValue);
}

void CRenderState::RestoreRenderState(D3DRENDERSTATETYPE Type)
{
#ifdef _DEBUG
	if (m_RenderStateStack[Type].empty())
	{
		Tracef(" CRenderState::SaveRenderState - This render state was not saved [%d, %d]\n", Type);
		StateManager_Assert(!" This render state was not saved!");
	}
#endif _DEBUG

	SetRenderState(Type, m_RenderStateStack[Type].back());
	m_RenderStateStack[Type].pop_back();
}

void CRenderState::SetRenderState(D3DRENDERSTATETYPE Type, DWORD Value)
{
	if (m_CurrentState.m_RenderStates[Type] == Value)
		return;

	m_CurrentState.m_RenderStates[Type] = Value;
}

void CRenderState::GetRenderState(D3DRENDERSTATETYPE Type, DWORD* pdwValue)
{
	*pdwValue = m_CurrentState.m_RenderStates[Type];
}

// Textures
void CRenderState::SaveTexture(DWORD dwStage, TextureBinding texture)
{
	m_TextureStack[dwStage].push_back(m_textureBindings[dwStage]);
	SetTexture(dwStage, std::move(texture));
}

void CRenderState::RestoreTexture(DWORD dwStage)
{
	SetTexture(dwStage, m_TextureStack[dwStage].back());
	m_TextureStack[dwStage].pop_back();
}

void CRenderState::SetTexture(DWORD dwStage, TextureBinding texture)
{
    assert(dwStage<8);
    assert(!texture.native && "Native texture passed to production draw state");
    m_textureBindings[dwStage]=TextureBinding(std::move(texture.source));
}

void CRenderState::GetTexture(DWORD dwStage, LPDIRECT3DBASETEXTURE9* ppTexture)
{
    *ppTexture=nullptr;
}

// Texture stage states
void CRenderState::SaveTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD dwValue)
{
	m_TextureStageStateStack[dwStage][Type].push_back(m_CurrentState.m_TextureStates[dwStage][Type]);
	SetTextureStageState(dwStage, Type, dwValue);
}

void CRenderState::RestoreTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type)
{
#ifdef _DEBUG
	if (m_TextureStageStateStack[dwStage][Type].empty())
	{
		Tracef(" CRenderState::RestoreTextureStageState - This texture stage state was not saved [%d, %d]\n", dwStage, Type);
		StateManager_Assert(!" This texture stage state was not saved!");
	}
#endif _DEBUG
	SetTextureStageState(dwStage, Type, m_TextureStageStateStack[dwStage][Type].back());
	m_TextureStageStateStack[dwStage][Type].pop_back();
}

void CRenderState::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD dwValue)
{
	if (m_CurrentState.m_TextureStates[dwStage][Type] == dwValue)
		return;

	m_CurrentState.m_TextureStates[dwStage][Type] = dwValue;
}

void CRenderState::GetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE Type, DWORD* pdwValue)
{
	*pdwValue = m_CurrentState.m_TextureStates[dwStage][Type];
}

// Sampler states
void CRenderState::SaveSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD dwValue)
{
	m_SamplerStateStack[dwStage][Type].push_back(m_CurrentState.m_SamplerStates[dwStage][Type]);
	SetSamplerState(dwStage, Type, dwValue);
}
void CRenderState::RestoreSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type)
{
#ifdef _DEBUG
	if (m_SamplerStateStack[dwStage][Type].empty())
	{
		Tracenf(" CRenderState::RestoreTextureStageState - This texture stage state was not saved [%d, %d]\n", dwStage, Type);
		StateManager_Assert(!" This texture stage state was not saved!");
	}
#endif _DEBUG
	SetSamplerState(dwStage, Type, m_SamplerStateStack[dwStage][Type].back());
	m_SamplerStateStack[dwStage][Type].pop_back();
}
void CRenderState::SetSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD dwValue)
{
	if (m_CurrentState.m_SamplerStates[dwStage][Type] == dwValue)
		return;
	m_CurrentState.m_SamplerStates[dwStage][Type] = dwValue;
}
void CRenderState::GetSamplerState(DWORD dwStage, D3DSAMPLERSTATETYPE Type, DWORD* pdwValue)
{
	*pdwValue = m_CurrentState.m_SamplerStates[dwStage][Type];
}

// Vertex Shader
void CRenderState::SaveVertexShader(LPDIRECT3DVERTEXSHADER9 dwShader)
{
	m_VertexShaderStack.push_back(m_CurrentState.m_dwVertexShader);
	SetVertexShader(dwShader);
}

void CRenderState::RestoreVertexShader()
{
	SetVertexShader(m_VertexShaderStack.back());
	m_VertexShaderStack.pop_back();
}

void CRenderState::SetVertexShader(LPDIRECT3DVERTEXSHADER9 dwShader)
{
    assert(!dwShader); m_CurrentState.m_dwVertexShader=nullptr;
}

void CRenderState::GetVertexShader(LPDIRECT3DVERTEXSHADER9* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwVertexShader;
}

// Vertex Processing
void CRenderState::SaveVertexProcessing(BOOL IsON)
{
	m_VertexProcessingStack.push_back(m_CurrentState.m_bVertexProcessing);
	m_CurrentState.m_bVertexProcessing = IsON;
}
void CRenderState::RestoreVertexProcessing()
{
	m_VertexProcessingStack.pop_back();
}
// Vertex Declaration
void CRenderState::SaveVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 dwShader)
{
	m_VertexDeclarationStack.push_back(m_CurrentState.m_dwVertexDeclaration);
	SetVertexDeclaration(dwShader);
}
void CRenderState::RestoreVertexDeclaration()
{
	SetVertexDeclaration(m_VertexDeclarationStack.back());
	m_VertexDeclarationStack.pop_back();
}
void CRenderState::SetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9 dwShader)
{
	m_CurrentState.m_dwVertexDeclaration = dwShader;
}
void CRenderState::GetVertexDeclaration(LPDIRECT3DVERTEXDECLARATION9* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwVertexDeclaration;
}
// FVF
void CRenderState::SaveFVF(DWORD dwShader)
{
	m_FVFStack.push_back(m_CurrentState.m_dwFVF);
	SetFVF(dwShader);
}
void CRenderState::RestoreFVF()
{
	SetFVF(m_FVFStack.back());
	m_FVFStack.pop_back();
}
void CRenderState::SetFVF(DWORD dwShader)
{
	//if (m_CurrentState.m_dwFVF == dwShader)
	//	return;
	m_CurrentState.m_dwFVF = dwShader;
}
void CRenderState::GetFVF(DWORD* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwFVF;
}

// Pixel Shader
void CRenderState::SavePixelShader(LPDIRECT3DPIXELSHADER9 dwShader)
{
	m_PixelShaderStack.push_back(m_CurrentState.m_dwPixelShader);
	SetPixelShader(dwShader);
}

void CRenderState::RestorePixelShader()
{
	SetPixelShader(m_PixelShaderStack.back());
	m_PixelShaderStack.pop_back();
}

void CRenderState::SetPixelShader(LPDIRECT3DPIXELSHADER9 dwShader)
{
    assert(!dwShader); m_CurrentState.m_dwPixelShader=nullptr;
}

void CRenderState::GetPixelShader(LPDIRECT3DPIXELSHADER9* pdwShader)
{
	*pdwShader = m_CurrentState.m_dwPixelShader;
}

// *** These states are cached, but not protected from multiple sends of the same value.
// Transform
void CRenderState::SaveTransform(Renderer::MatrixSlot Type, const D3DXMATRIX* pMatrix)
{
	m_TransformStack[Type].push_back(m_CurrentState.m_Matrices[Type]);
	SetTransform(Type, pMatrix);
}

void CRenderState::RestoreTransform(Renderer::MatrixSlot Type)
{
#ifdef _DEBUG
	if (m_TransformStack[Type].empty())
	{
		Tracef(" CRenderState::RestoreTransform - This transform was not saved [%d]\n", Type);
		StateManager_Assert(!" This render state was not saved!");
	}
#endif _DEBUG

	SetTransform(Type, &m_TransformStack[Type].back());
	m_TransformStack[Type].pop_back();
}

// Don't cache-check the transform.  To much to do
void CRenderState::SetTransform(Renderer::MatrixSlot Type, const D3DXMATRIX* pMatrix)
{
    m_CurrentState.m_Matrices[Type] = *pMatrix;
}

void CRenderState::GetTransform(Renderer::MatrixSlot Type, D3DXMATRIX* pMatrix)
{
	*pMatrix = m_CurrentState.m_Matrices[Type];
}

void CRenderState::SetVertexShaderConstant(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount)
{
    if(dwRegister<=96 && dwConstantCount<=96-dwRegister) memcpy(m_vertexConstants[dwRegister],pConstantData,dwConstantCount*16);
}

void CRenderState::SetPixelShaderConstant(DWORD dwRegister, CONST void* pConstantData, DWORD dwConstantCount)
{
    // No production pixel-constant consumer; Diligent materials own their constants.
}

void CRenderState::SaveStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER9 pStreamData, UINT Stride)
{
	m_StreamStack[StreamNumber].push_back(m_CurrentState.m_StreamData[StreamNumber]);
	SetStreamSource(StreamNumber, pStreamData, Stride);
}

void CRenderState::RestoreStreamSource(UINT StreamNumber)
{
	const auto& topStream = m_StreamStack[StreamNumber].back();
	SetStreamSource(StreamNumber,
		topStream.m_lpStreamData,
		topStream.m_Stride);
	m_StreamStack[StreamNumber].pop_back();
}

void CRenderState::SetStreamSource(UINT StreamNumber, LPDIRECT3DVERTEXBUFFER9 pStreamData, UINT Stride)
{
	CStreamData kStreamData(pStreamData, Stride);
	if (m_CurrentState.m_StreamData[StreamNumber] == kStreamData)
		return;

	m_CurrentState.m_StreamData[StreamNumber] = kStreamData;
}

void CRenderState::SaveIndices(LPDIRECT3DINDEXBUFFER9 pIndexData, UINT BaseVertexIndex)
{
	m_IndexStack.push_back(m_CurrentState.m_IndexData);
	SetIndices(pIndexData, BaseVertexIndex);
}

void CRenderState::RestoreIndices()
{
	const auto& topIndex = m_IndexStack.back();
	SetIndices(topIndex.m_lpIndexData, topIndex.m_BaseVertexIndex);
	m_IndexStack.pop_back();
}

void CRenderState::SetIndices(LPDIRECT3DINDEXBUFFER9 pIndexData, UINT BaseVertexIndex)
{
	CIndexData kIndexData(pIndexData, BaseVertexIndex);

	if (m_CurrentState.m_IndexData == kIndexData)
		return;

	m_CurrentState.m_IndexData = kIndexData;
}

HRESULT CRenderState::DrawPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
#ifdef _DEBUG
	++m_iDrawCallCount;
#endif

    ++m_nativeCounters.suppressedDraws; return S_OK;
}

HRESULT CRenderState::DrawPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, const void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
#ifdef _DEBUG
	++m_iDrawCallCount;
#endif

	m_CurrentState.m_StreamData[0] = NULL;
    ++m_nativeCounters.suppressedDraws; return S_OK;
}

HRESULT CRenderState::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
#ifdef _DEBUG
	++m_iDrawCallCount;
#endif

    ++m_nativeCounters.suppressedDraws; return S_OK;
}

HRESULT CRenderState::DrawIndexedPrimitive(D3DPRIMITIVETYPE PrimitiveType, INT baseVertexIndex, UINT minIndex, UINT NumVertices, UINT startIndex, UINT primCount)
{
#ifdef _DEBUG
	++m_iDrawCallCount;
#endif

    ++m_nativeCounters.suppressedDraws; return S_OK;
}

HRESULT CRenderState::DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertexIndices, UINT PrimitiveCount, CONST void* pIndexData, D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride)
{
#ifdef _DEBUG
	++m_iDrawCallCount;
#endif

	m_CurrentState.m_IndexData = NULL;
	m_CurrentState.m_StreamData[0] = NULL;
    ++m_nativeCounters.suppressedDraws; return S_OK;
}

#ifdef _DEBUG
void CRenderState::ResetDrawCallCounter()
{
	m_iLastDrawCallCount = m_iDrawCallCount;
	m_iDrawCallCount = 0;
}

int CRenderState::GetDrawCallCount() const
{
	return m_iLastDrawCallCount;
}
#endif
