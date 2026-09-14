#include "StdAfx.h"
#include "AssetRuntime/Granny/Native.h"
#include "Material.h"
#include "Mesh.h"
#include "Eterbase/Filename.h"
#include "Eterlib/ResourceManager.h"
#include "Eterlib/DrawState.h"
#include "Eterlib/GrpScreen.h"

CGraphicImageInstance CGrannyMaterial::ms_akSphereMapInstance[SPHEREMAP_NUM];

Math::Vector3	CGrannyMaterial::ms_v3SpecularTrans(0.0f, 0.0f, 0.0f);
Math::Matrix	CGrannyMaterial::ms_matSpecular;

Math::Color g_fSpecularColor = Math::Color(0.0f, 0.0f, 0.0f, 0.0f);

void CGrannyMaterial::TranslateSpecularMatrix(float fAddX, float fAddY, float fAddZ)
{
	static float SPECULAR_TRANSLATE_MAX = 1000000.0f;

	ms_v3SpecularTrans.x+=fAddX;
	ms_v3SpecularTrans.y+=fAddY;
	ms_v3SpecularTrans.z+=fAddZ;

	if (ms_v3SpecularTrans.x>=SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.x=0.0f;

	if (ms_v3SpecularTrans.y>=SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.y=0.0f;

	if (ms_v3SpecularTrans.z>=SPECULAR_TRANSLATE_MAX)
		ms_v3SpecularTrans.z=0.0f;

	Math::MatrixTranslation(&ms_matSpecular,
		ms_v3SpecularTrans.x, 
		ms_v3SpecularTrans.y, 
		ms_v3SpecularTrans.z
	);
}

void CGrannyMaterial::ApplyRenderState()
{
	assert(m_pfnApplyRenderState!=NULL && "CGrannyMaterial::SaveRenderState");
	(this->*m_pfnApplyRenderState)();
}

void CGrannyMaterial::RestoreRenderState()
{
	assert(m_pfnRestoreRenderState!=NULL && "CGrannyMaterial::RestoreRenderState");
	(this->*m_pfnRestoreRenderState)();
}

void CGrannyMaterial::Copy(CGrannyMaterial& rkMtrl)
{
	m_pgrnMaterial = rkMtrl.m_pgrnMaterial;
	m_sourceAsset = rkMtrl.m_sourceAsset;
	m_roImage[0] =  rkMtrl.m_roImage[0];
	m_roImage[1] =  rkMtrl.m_roImage[1];
    m_eType = rkMtrl.m_eType;
    m_asset = rkMtrl.m_asset;
    // Preserve the legacy copy semantics: culling/specular stay with this instance.
    m_asset.culling = m_bTwoSideRender ? AssetRuntime::Culling::None : AssetRuntime::Culling::Clockwise;
    m_asset.specular = m_bSpecularEnable != FALSE;
    m_asset.specularPower = m_fSpecularPower;
    m_asset.sphereMapIndex = m_bSphereMapIndex;
}

CGrannyMaterial::CGrannyMaterial()
{
	m_bTwoSideRender = false;
	m_dwLastCullRenderStateForTwoSideRendering = Renderer::CullCw;

	Initialize();
}

CGrannyMaterial::~CGrannyMaterial()
{
}

CGrannyMaterial::EType CGrannyMaterial::GetType() const
{
	return m_eType;
}

void CGrannyMaterial::SetImagePointer(int iStage, CGraphicImage* pImage)
{	
	assert(iStage<2 && "CGrannyMaterial::SetImagePointer");
	m_roImage[iStage]=pImage;
    m_asset.textures[iStage] = pImage ? pImage->GetFileName() : "";
}

bool CGrannyMaterial::IsIn(const char* c_szImageName, int* piStage)
{
	if (!c_szImageName || !piStage) return false;
	std::string strImageName = c_szImageName;
	CFileNameHelper::StringPath(strImageName);
	if (m_sourceAsset) {
        const auto& sourceTextures = m_sourceAsset->hasMatchingTextures ? m_sourceAsset->matchingTextures : m_sourceAsset->textures;
        for (int stage = 0; stage < 2; ++stage) {
            if (sourceTextures[stage].empty()) continue;
            std::string sourceName = sourceTextures[stage];
            CFileNameHelper::StringPath(sourceName);
            if (sourceName == strImageName) { *piStage = stage; return true; }
        }
        return false;
    }
    if (!m_pgrnMaterial) return false;

	granny_texture * pgrnDiffuseTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyDiffuseColorTexture);
	if (pgrnDiffuseTexture)
	{
		std::string strDiffuseFileName = pgrnDiffuseTexture->FromFileName;
		CFileNameHelper::StringPath(strDiffuseFileName);
		if (strDiffuseFileName == strImageName)
		{
			*piStage=0;
			return true;
		}
	}

    granny_texture * pgrnOpacityTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyOpacityTexture);
	if (pgrnOpacityTexture)
	{
		std::string strOpacityFileName = pgrnOpacityTexture->FromFileName;
		CFileNameHelper::StringPath(strOpacityFileName);
		if (strOpacityFileName == strImageName)
		{
			*piStage=1;
			return true;
		}
	}

	return false;
}

void CGrannyMaterial::SetSpecularInfo(BOOL bFlag, float fPower, BYTE uSphereMapIndex)
{
	m_fSpecularPower = fPower;
	m_bSphereMapIndex = uSphereMapIndex;
	m_bSpecularEnable = bFlag;
    m_asset.specular = bFlag != FALSE;
    m_asset.specularPower = fPower;
    m_asset.sphereMapIndex = uSphereMapIndex;

	if (bFlag)
	{
		m_pfnApplyRenderState = &CGrannyMaterial::__ApplySpecularRenderState;
		m_pfnRestoreRenderState = &CGrannyMaterial::__RestoreSpecularRenderState;
	}
	else
	{
		m_pfnApplyRenderState = &CGrannyMaterial::__ApplyDiffuseRenderState;
		m_pfnRestoreRenderState = &CGrannyMaterial::__RestoreDiffuseRenderState;
	}
}

bool CGrannyMaterial::IsEqual(granny_material* pgrnMaterial) const
{
	if (m_pgrnMaterial==pgrnMaterial)
		return true;

	return false;
}


TextureBinding CGrannyMaterial::GetTextureBinding(int stage) const
{
    auto* image=GetImagePointer(stage);
    return image ? image->GetTexturePointer()->GetTextureBinding() : TextureBinding{};
}



CGraphicImage * CGrannyMaterial::GetImagePointer(int iStage) const
{
	const CGraphicImage::TRef & ratImage = m_roImage[iStage];

	if (ratImage.IsNull())
		return NULL;

	CGraphicImage * pImage = ratImage.GetPointer();
	return pImage;
}

const CGraphicTexture* CGrannyMaterial::GetDiffuseTexture() const
{
	if (m_roImage[0].IsNull())
		return NULL;

	return m_roImage[0].GetPointer()->GetTexturePointer();
}

const CGraphicTexture* CGrannyMaterial::GetOpacityTexture() const
{
	if (m_roImage[1].IsNull())
		return NULL;

	return m_roImage[1].GetPointer()->GetTexturePointer();
}

BOOL CGrannyMaterial::__IsSpecularEnable() const
{
	return m_bSpecularEnable;
}

// MR-12: Fix specular isolation issue
float CGrannyMaterial::GetSpecularPower() const
{
	return m_fSpecularPower;
}
// MR-12: -- END OF -- Fix specular isolation issue

extern const std::string& GetModelLocalPath();

CGraphicImage* CGrannyMaterial::__GetImagePointer(const char* fileName)
{
	assert(*fileName != '\0');

	CResourceManager& rkResMgr = CResourceManager::Instance();

	// SUPPORT_LOCAL_TEXTURE
	int fileName_len = strlen(fileName);
	if (fileName_len > 2 && fileName[1] != ':')
	{
		char localFileName[256];		
		const std::string& modelLocalPath = GetModelLocalPath();

		int localFileName_len = modelLocalPath.length() + 1 + fileName_len;
		if (localFileName_len < sizeof(localFileName) - 1)
		{
			_snprintf(localFileName, sizeof(localFileName), "%s%s", GetModelLocalPath().c_str(), fileName);
			CResource* pResource = rkResMgr.GetResourcePointer(localFileName);
			return static_cast<CGraphicImage*>(pResource);
		}		
	}
	// END_OF_SUPPORT_LOCAL_TEXTURE
	

	CResource* pResource = rkResMgr.GetResourcePointer(fileName);
	return static_cast<CGraphicImage*>(pResource);
}

bool CGrannyMaterial::CreateFromGrannyMaterialPointer(granny_material * pgrnMaterial)
{
	m_sourceAsset = nullptr;
	m_pgrnMaterial = pgrnMaterial;

	granny_texture * pgrnDiffuseTexture = NULL;
	granny_texture * pgrnOpacityTexture = NULL;

	if (pgrnMaterial)
	{
		if (pgrnMaterial->MapCount > 1 && !_strnicmp(pgrnMaterial->Name, "Blend", 5))
		{
			pgrnDiffuseTexture = GrannyGetMaterialTextureByType(pgrnMaterial->Maps[0].Material, GrannyDiffuseColorTexture);
			pgrnOpacityTexture = GrannyGetMaterialTextureByType(pgrnMaterial->Maps[1].Material, GrannyDiffuseColorTexture);
		}
		else
		{
			pgrnDiffuseTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyDiffuseColorTexture);
			pgrnOpacityTexture = GrannyGetMaterialTextureByType(m_pgrnMaterial, GrannyOpacityTexture);
		}

		// Two-Side 렌더링이 필요한 지 검사
		{			
			granny_int32 twoSided = 0;
			granny_data_type_definition TwoSidedFieldType[] =
			{
				{GrannyInt32Member, "Two-sided"},
				{GrannyEndMember},
			};

			granny_variant twoSideResult;

			if (GrannyFindMatchingMember(pgrnMaterial->ExtendedData.Type, pgrnMaterial->ExtendedData.Object, "Two-sided", &twoSideResult)  && NULL != twoSideResult.Type)
				GrannyConvertSingleObject(twoSideResult.Type, twoSideResult.Object, TwoSidedFieldType, &twoSided, NULL);

			m_bTwoSideRender = 1 == twoSided;
		}
	}

	if (pgrnDiffuseTexture)
		m_roImage[0].SetPointer(__GetImagePointer(pgrnDiffuseTexture->FromFileName));

	if (pgrnOpacityTexture)
		m_roImage[1].SetPointer(__GetImagePointer(pgrnOpacityTexture->FromFileName));

	// 오퍼시티가 있으면 블렌딩 메쉬
	if (!m_roImage[1].IsNull())
		m_eType = TYPE_BLEND_PNT;
	else
		m_eType = TYPE_DIFFUSE_PNT;

    // ZiiNAN: Asset Runtime boundary
    // Resolve once with the existing resource cache; overrides update only their changed field.
    m_asset.name = pgrnMaterial && pgrnMaterial->Name ? pgrnMaterial->Name : "";
    for (int stage = 0; stage < 2; ++stage) {
        const auto* image = GetImagePointer(stage);
        m_asset.textures[stage] = image ? image->GetFileName() : "";
    }
    m_asset.stage = m_eType == TYPE_BLEND_PNT ? AssetRuntime::MaterialStage::DiffuseOpacity : AssetRuntime::MaterialStage::Diffuse;
    m_asset.blending = m_eType == TYPE_BLEND_PNT;
    m_asset.culling = m_bTwoSideRender ? AssetRuntime::Culling::None : AssetRuntime::Culling::Clockwise;

	return true;
}

bool CGrannyMaterial::CreateFromAsset(const AssetRuntime::MaterialAsset& material)
{
    m_pgrnMaterial = nullptr;
    m_sourceAsset = &material;
    m_asset = material;
    m_bTwoSideRender = material.culling == AssetRuntime::Culling::None;
    for (int stage = 0; stage < 2; ++stage) {
        m_roImage[stage] = material.textures[stage].empty() ? nullptr : __GetImagePointer(material.textures[stage].c_str());
        const auto* image = GetImagePointer(stage);
        m_asset.textures[stage] = image ? image->GetFileName() : "";
    }
    // Existing palette classification follows successfully resolved opacity resources.
    m_eType = m_roImage[1].IsNull() ? TYPE_DIFFUSE_PNT : TYPE_BLEND_PNT;
    m_asset.stage = m_eType == TYPE_BLEND_PNT ? AssetRuntime::MaterialStage::DiffuseOpacity : AssetRuntime::MaterialStage::Diffuse;
    m_asset.blending = m_eType == TYPE_BLEND_PNT;
    SetSpecularInfo(material.specular ? TRUE : FALSE, material.specularPower, material.sphereMapIndex);
    return true;
}

void CGrannyMaterial::Initialize()
{
	m_sourceAsset = nullptr;
	m_pgrnMaterial = nullptr;
	m_eType = TYPE_DIFFUSE_PNT;
	m_roImage[0] = NULL;
	m_roImage[1] = NULL;

	SetSpecularInfo(FALSE, 0.0f, 0);
}

void CGrannyMaterial::__ApplyDiffuseRenderState()
{
	DRAWSTATE.SetTexture(0, GetTextureBinding(0));

	if (m_bTwoSideRender)
	{
		// -_-렌더링 프로세스가 좀 구려서... Save & Restore 하면 순서때문에 좀 꼬인다. 귀찮으니 Save & Restore 대신 따로 저장해 둠.
		m_dwLastCullRenderStateForTwoSideRendering = DRAWSTATE.GetRenderState(Renderer::StateCullMode);
		DRAWSTATE.SetRenderState(Renderer::StateCullMode, Renderer::CullNone);
	}
}

void CGrannyMaterial::__RestoreDiffuseRenderState()
{
	if (m_bTwoSideRender)
	{
		DRAWSTATE.SetRenderState(Renderer::StateCullMode, m_dwLastCullRenderStateForTwoSideRendering);
	}
}

void CGrannyMaterial::__ApplySpecularRenderState()
{
	if (TRUE == DRAWSTATE.GetRenderState(Renderer::StateAlphaBlendEnable))
	{
		__ApplyDiffuseRenderState();
		return;
	}

	CGraphicTexture* pkTexture=ms_akSphereMapInstance[m_bSphereMapIndex].GetTexturePointer();

	DRAWSTATE.SetTexture(0, GetTextureBinding(0));

	if (pkTexture)
		DRAWSTATE.SetTexture(1, pkTexture->GetTextureBinding());
	else
		DRAWSTATE.SetTexture(1, NULL);

	// MR-12: Fix specular isolation issue
	DRAWSTATE.SetRenderState(Renderer::StateTextureFactor, Math::Color(g_fSpecularColor.r, g_fSpecularColor.g, g_fSpecularColor.b, GetSpecularPower()));
	// MR-12: -- END OF -- Fix specular isolation issue
	DRAWSTATE.SaveTextureStageState(1, Renderer::StageTexCoordIndex, Renderer::StageTciCameraSpaceReflectionVector);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageColorArg1,	Renderer::ArgTexture);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageColorArg2,	Renderer::ArgDiffuse);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageColorOp,	Renderer::TextureOpModulate);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageAlphaArg1,	Renderer::ArgTexture);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageAlphaArg2,	Renderer::ArgTFactor);
	DRAWSTATE.SaveTextureStageState(0, Renderer::StageAlphaOp,	Renderer::TextureOpModulate);

	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg1,	Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorArg2,	Renderer::ArgTexture);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp,	Renderer::TextureOpModulateAlphaAddColor);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaArg1,	Renderer::ArgCurrent);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp,	Renderer::TextureOpSelectArg1);

	DRAWSTATE.SetTransform(Renderer::MatrixTexture1, &ms_matSpecular);
	DRAWSTATE.SaveTextureStageState(1, Renderer::StageTextureTransformFlags, Renderer::TexTransformCount2);
	DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressU, Renderer::AddressWrap);
	DRAWSTATE.SaveSamplerState(1, Renderer::SamplerAddressV, Renderer::AddressWrap);
}

void CGrannyMaterial::__RestoreSpecularRenderState()
{
	if (TRUE == DRAWSTATE.GetRenderState(Renderer::StateAlphaBlendEnable))
	{
		__RestoreDiffuseRenderState();
		return;
	}

	DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTextureTransformFlags);
	DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressU);
	DRAWSTATE.RestoreSamplerState(1, Renderer::SamplerAddressV);

	DRAWSTATE.RestoreTextureStageState(1, Renderer::StageTexCoordIndex);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageColorOp, Renderer::TextureOpDisable);
	DRAWSTATE.SetTextureStageState(1, Renderer::StageAlphaOp, Renderer::TextureOpDisable);

	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageColorArg1);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageColorArg2);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageColorOp);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageAlphaArg1);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageAlphaArg2);
	DRAWSTATE.RestoreTextureStageState(0, Renderer::StageAlphaOp);
}

void CGrannyMaterial::CreateSphereMap(UINT uMapIndex, const char* c_szSphereMapImageFileName)
{
	CResourceManager& rkResMgr = CResourceManager::Instance();
	CGraphicImage * pImage = (CGraphicImage *)rkResMgr.GetResourcePointer(c_szSphereMapImageFileName);
	ms_akSphereMapInstance[uMapIndex].SetImagePointer(pImage);
}

void CGrannyMaterial::DestroySphereMap()
{
	for (UINT uMapIndex=0; uMapIndex<SPHEREMAP_NUM; ++uMapIndex)
		ms_akSphereMapInstance[uMapIndex].Destroy();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

CGrannyMaterialPalette::CGrannyMaterialPalette()
{
}

CGrannyMaterialPalette::~CGrannyMaterialPalette()
{
	Clear();
}

void CGrannyMaterialPalette::Copy(const CGrannyMaterialPalette& rkMtrlPalSrc)
{
	m_mtrlVector=rkMtrlPalSrc.m_mtrlVector;
}

void CGrannyMaterialPalette::Clear()
{
	m_mtrlVector.clear();
}

CGrannyMaterial& CGrannyMaterialPalette::GetMaterialRef(DWORD mtrlIndex)
{
	assert(mtrlIndex<m_mtrlVector.size());
	return *m_mtrlVector[mtrlIndex].GetPointer();
}

void CGrannyMaterialPalette::SetMaterialImagePointer(const char* c_szImageName, CGraphicImage* pImage)
{
	DWORD size=m_mtrlVector.size();
	DWORD i;
	for (i=0; i<size; ++i)
	{
		CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];

		int iStage;
		if (roMtrl->IsIn(c_szImageName, &iStage))
		{
			CGrannyMaterial* pkNewMtrl=new CGrannyMaterial;
			pkNewMtrl->Copy(*roMtrl.GetPointer());
			pkNewMtrl->SetImagePointer(iStage, pImage);
			roMtrl=pkNewMtrl;

			return;
		}
	}
}

void CGrannyMaterialPalette::SetMaterialData(const char* c_szMtrlName, const SMaterialData& c_rkMaterialData)
{
	if (c_szMtrlName)
	{
		std::vector<CGrannyMaterial::TRef>::iterator i;
		for (i=m_mtrlVector.begin(); i!=m_mtrlVector.end(); ++i)
		{
			CGrannyMaterial::TRef& roMtrl=*i;

			int iStage;
			if (roMtrl->IsIn(c_szMtrlName, &iStage))
			{
				CGrannyMaterial* pkNewMtrl=new CGrannyMaterial;
				pkNewMtrl->Copy(*roMtrl.GetPointer());
				pkNewMtrl->SetImagePointer(iStage, c_rkMaterialData.pImage);
				pkNewMtrl->SetSpecularInfo(c_rkMaterialData.isSpecularEnable, c_rkMaterialData.fSpecularPower, c_rkMaterialData.bSphereMapIndex);
				roMtrl=pkNewMtrl;

				return;
			}
		}
	}
	else
	{
		std::vector<CGrannyMaterial::TRef>::iterator i;
		for (i=m_mtrlVector.begin(); i!=m_mtrlVector.end(); ++i)
		{
			CGrannyMaterial::TRef& roMtrl=*i;
			roMtrl->SetSpecularInfo(c_rkMaterialData.isSpecularEnable, c_rkMaterialData.fSpecularPower, c_rkMaterialData.bSphereMapIndex);
		}
	}
}

void CGrannyMaterialPalette::SetSpecularInfo(const char* c_szMtrlName, BOOL bEnable, float fPower)
{
	DWORD size=m_mtrlVector.size();
	DWORD i;
	if (c_szMtrlName)
	{
		for (i=0; i<size; ++i)
		{
			CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];

			int iStage;
			if (roMtrl->IsIn(c_szMtrlName, &iStage))
			{
				roMtrl->SetSpecularInfo(bEnable, fPower, 0);
				return;
			}
		}
	}
	else
	{
		for (i=0; i<size; ++i)
		{
			CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];
			roMtrl->SetSpecularInfo(bEnable, fPower, 0);
		}
	}
}

DWORD CGrannyMaterialPalette::RegisterMaterial(granny_material* pgrnMaterial)
{
	DWORD size=m_mtrlVector.size();
	DWORD i;
	for (i=0; i<size; ++i)
	{
		CGrannyMaterial::TRef& roMtrl=m_mtrlVector[i];
		if (roMtrl->IsEqual(pgrnMaterial))
			return i;
	}

	CGrannyMaterial* pkNewMtrl=new CGrannyMaterial;
	pkNewMtrl->CreateFromGrannyMaterialPointer(pgrnMaterial);
	m_mtrlVector.push_back(pkNewMtrl);
	
	return size;
}

DWORD CGrannyMaterialPalette::GetMaterialCount() const
{
	return m_mtrlVector.size();
}

DWORD CGrannyMaterialPalette::RegisterMaterial(const AssetRuntime::MaterialAsset& material)
{
    for (DWORD index = 0; index < m_mtrlVector.size(); ++index)
        if (m_mtrlVector[index]->IsEqual(&material)) return index;
    auto* translated = new CGrannyMaterial;
    translated->CreateFromAsset(material);
    m_mtrlVector.push_back(translated);
    return static_cast<DWORD>(m_mtrlVector.size() - 1);
}

