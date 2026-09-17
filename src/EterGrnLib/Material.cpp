#include "StdAfx.h"
#include "EterBase/MapLoadTrace.h"
#include "Material.h"
#include "Mesh.h"
#include "Eterbase/Filename.h"
#include "Eterlib/ResourceManager.h"
#include "Eterlib/DrawState.h"
#include "Eterlib/GrpScreen.h"
#include <cmath>

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
	m_sourceAsset = rkMtrl.m_sourceAsset;
	m_roImage[0] =  rkMtrl.m_roImage[0];
	m_roImage[1] =  rkMtrl.m_roImage[1];
    m_eType = rkMtrl.m_eType;
    m_asset = rkMtrl.m_asset;
    m_modernMaterial = rkMtrl.m_modernMaterial;
    m_modernUploader = rkMtrl.m_modernUploader;
    m_modernImages = rkMtrl.m_modernImages;
    if (m_asset.explicitRenderState) {
        m_bTwoSideRender = rkMtrl.m_bTwoSideRender;
        SetSpecularInfo(rkMtrl.m_bSpecularEnable, rkMtrl.m_fSpecularPower, rkMtrl.m_bSphereMapIndex);
        return;
    }
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
    m_asset.embeddedImages[iStage].reset();
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



bool CGrannyMaterial::CreateFromAsset(const AssetRuntime::MaterialAsset& material)
{
    MapLoadTrace::Scope p0lScope("Materials","material creation","cpu");

    m_modernMaterial.reset();m_modernUploader.reset();
    for(auto& image:m_modernImages) image=nullptr;
    if (material.explicitRenderState) {
        if (!std::isfinite(material.alphaCutoff) || material.alphaCutoff < 0.0f) return false;
        for (const auto factor : material.baseColorFactor)
            if (!std::isfinite(factor) || factor < 0.0f || factor > 1.0f) return false;
    }
    m_sourceAsset = &material;
    m_asset = material;
    m_bTwoSideRender = material.culling == AssetRuntime::Culling::None;
    for (int stage = 0; stage < 2; ++stage) {
        auto encoded = material.embeddedImages[stage];
        if (stage == 0 && material.explicitRenderState && !encoded && material.textures[stage].empty()) {
            // Untextured authored materials use the same cached diffuse-texture contract.
            static const auto white = [] {
                auto image = std::make_shared<AssetRuntime::EncodedImage>();
                image->id = "asset-runtime:white.png";
                image->mimeType = "image/png";
                const unsigned char png[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,8,6,0,0,0,31,21,196,137,0,0,0,13,73,68,65,84,120,156,99,248,255,255,255,127,0,9,251,3,253,42,134,227,138,0,0,0,0,73,69,78,68,174,66,96,130};
                image->bytes.resize(sizeof(png));
                memcpy(image->bytes.data(), png, sizeof(png));
                return image;
            }();
            encoded = white;
        }
        if (encoded) {
            auto* image = CResourceManager::Instance().GetEncodedImagePointer(encoded);
            if (!image) {
                TraceError("Asset material image decode failed: %s", encoded->id.c_str());
                return false;
            }
            m_roImage[stage] = image;
            m_asset.embeddedImages[stage] = std::move(encoded);
        } else {
            m_roImage[stage] = material.textures[stage].empty() ? nullptr : __GetImagePointer(material.textures[stage].c_str());
            if (material.explicitRenderState && !material.textures[stage].empty() &&
                (m_roImage[stage].IsNull() || m_roImage[stage]->IsEmpty())) return false;
        }
        const auto* image = GetImagePointer(stage);
        m_asset.textures[stage] = image ? image->GetFileName() : "";
    }
    // Existing palette classification follows successfully resolved opacity resources.
    m_eType = material.explicitRenderState ? (material.blending ? TYPE_BLEND_PNT : TYPE_DIFFUSE_PNT) :
        (m_roImage[1].IsNull() ? TYPE_DIFFUSE_PNT : TYPE_BLEND_PNT);
    if (!material.explicitRenderState) {
        m_asset.stage = m_eType == TYPE_BLEND_PNT ? AssetRuntime::MaterialStage::DiffuseOpacity : AssetRuntime::MaterialStage::Diffuse;
        m_asset.blending = m_eType == TYPE_BLEND_PNT;
    }
    SetSpecularInfo(material.specular ? TRUE : FALSE, material.specularPower, material.sphereMapIndex);
    return true;
}

void CGrannyMaterial::Initialize()
{
    m_modernMaterial.reset();m_modernUploader.reset();
    for(auto& image:m_modernImages) image=nullptr;
	m_sourceAsset = nullptr;
	m_eType = TYPE_DIFFUSE_PNT;
	m_roImage[0] = NULL;
	m_roImage[1] = NULL;

	SetSpecularInfo(FALSE, 0.0f, 0);
}

std::shared_ptr<const Renderer::MaterialRuntimeData> CGrannyMaterial::GetModernMaterial(Renderer::ITextureUploader& uploader) const
{
    MapLoadTrace::Scope p0lScope("Materials","modern material lookup","cpu");

    if(m_modernMaterial && m_modernUploader.lock()==uploader.TextureCacheLifetime().lock()) return m_modernMaterial;
    auto result=std::make_shared<Renderer::MaterialRuntimeData>();
    result->baseColor=m_asset.baseColorFactor;result->emissive=m_asset.emissiveColor;
    result->roughness=m_asset.roughness;result->metallic=m_asset.metallic;
    result->normalScale=m_asset.normalScale;result->occlusionStrength=m_asset.occlusionStrength;
    result->model=m_asset.model;
    result->roughnessChannel=m_asset.materialTextures[2].channel;
    result->metallicChannel=m_asset.materialTextures[3].channel;
    result->occlusionChannel=m_asset.materialTextures[4].channel;
    for(std::size_t i=1;i<AssetRuntime::MaterialTextureCount;++i) {
        const auto& source=m_asset.materialTextures[i];
        CGraphicImage* image=nullptr;
        if(source.image) image=CResourceManager::Instance().GetEncodedImagePointer(source.image);
        else if(!source.id.empty()) {
            auto* resource=CResourceManager::Instance().GetResourcePointer(source.id.c_str());
            if(resource&&resource->IsType(CGraphicImage::Type()))image=static_cast<CGraphicImage*>(resource);
        }
        if(image && !image->IsEmpty()) {
            m_modernImages[i]=image;result->textures[i]=image->GetAssetTexture(uploader);
        }
        if(!source.id.empty() && !result->textures[i])
            TraceError("G-DX: invalid optional material map uses neutral fallback: %s",source.id.c_str());
    }
    m_modernUploader=uploader.TextureCacheLifetime();m_modernMaterial=std::move(result);
    return m_modernMaterial;
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



DWORD CGrannyMaterialPalette::GetMaterialCount() const
{
	return m_mtrlVector.size();
}

DWORD CGrannyMaterialPalette::RegisterMaterial(const AssetRuntime::MaterialAsset& material)
{
    for (DWORD index = 0; index < m_mtrlVector.size(); ++index)
        if (m_mtrlVector[index]->IsEqual(&material)) return index;
    auto* translated = new CGrannyMaterial;
    if (!translated->CreateFromAsset(material)) { delete translated; return InvalidMaterial; }
    m_mtrlVector.push_back(translated);
    return static_cast<DWORD>(m_mtrlVector.size() - 1);
}

