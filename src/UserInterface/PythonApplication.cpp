#include "StdAfx.h"
#include "Renderer/FirstUseAudit.h"
#include "Renderer/GraphicsConfig.h"
#include "Renderer/ModernFrame.h"
#include "Graphics/AtmosphereConfig.h"
#include "eterBase/Error.h"
#include "eterlib/Camera.h"
#include "eterlib/AttributeInstance.h"
#include "gamelib/AreaTerrain.h"
#include "EterGrnLib/Material.h"

#include "resource.h"
#include "PythonApplication.h"
#include "PythonCharacterManager.h"
#include "Renderer/ActorRenderData.h"
#include "Renderer/SkinningBenchmark.h"
#include "Renderer/AnimationStallFrame.h"
#include "Renderer/StaticObjectRenderData.h"
#include "Renderer/WorldRenderData.h"
#include "Renderer/UIRenderData.h"
#include "Platform/PlatformTime.h"

#include "ProcessScanner.h"

#include <utf8.h>
#include <fstream>




extern void CreateSharedDeformBuffer();
extern void DestroySharedDeformBuffer();

float MIN_FOG = 2400.0f;
double g_specularSpd=0.007f;

CPythonApplication * CPythonApplication::ms_pInstance;

float c_fDefaultCameraRotateSpeed = 1.5f;
float c_fDefaultCameraPitchSpeed = 1.5f;
float c_fDefaultCameraZoomSpeed = 0.05f;

CPythonApplication::CPythonApplication(Renderer::BackendKind backend) :
m_startupBackend(backend),
m_bCursorVisible(TRUE),
m_bLiarCursorOn(false),
m_iCursorMode(CURSOR_MODE_HARDWARE),
m_isWindowed(false),
m_isFrameSkipDisable(false),
m_poMouseHandler(NULL),
m_dwUpdateFPS(0),
m_dwRenderFPS(0),
m_fAveRenderTime(0.0f),
m_dwFaceCount(0),
m_fGlobalTime(0.0f),
m_fGlobalElapsedTime(0.0f),
m_dwLButtonDownTime(0),
m_dwLastIdleTime(0),
m_IsMovingMainWindow(false)
{
#ifndef _DEBUG
	SetEterExceptionHandler();
#endif

	CTimer::Instance().UseCustomTime();
	m_dwWidth = 800;
	m_dwHeight = 600;

	ms_pInstance = this;
	m_isWindowFullScreenEnable = FALSE;

	m_v3CenterPosition = Math::Vector3(0.0f, 0.0f, 0.0f);
	m_dwStartLocalTime = ELTimer_GetMSec();
	m_tServerTime = 0;
	m_tLocalStartTime = 0;

	m_iPort = 0;
	m_iFPS = 60;

	m_isActivateWnd = false;
	m_isMinimizedWnd = true;

	m_fRotationSpeed = 0.0f;
	m_fPitchSpeed = 0.0f;
	m_fZoomSpeed = 0.0f;

	m_fFaceSpd=0.0f;

	m_dwFaceAccCount=0;
	m_dwFaceAccTime=0;

	m_dwFaceSpdSum=0;
	m_dwFaceSpdCount=0;

	m_FlyingManager.SetMapManagerPtr(&m_pyBackground);

	m_iCursorNum = CURSOR_SHAPE_NORMAL;
	m_iContinuousCursorNum = CURSOR_SHAPE_NORMAL;

	m_isSpecialCameraMode = FALSE;
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed;

	m_iCameraMode = CAMERA_MODE_NORMAL;
	m_fBlendCameraStartTime = 0.0f;
	m_fBlendCameraBlendTime = 0.0f;

	m_iForceSightRange = -1;

	CCameraManager::Instance().AddCamera(EVENT_CAMERA_NUMBER);

	m_InitialMouseMovingPoint = {};
}

CPythonApplication::~CPythonApplication()
{
}

void CPythonApplication::GetMousePosition(POINT* ppt)
{
	const auto point = CMSApplication::GetMousePosition();
	ppt->x = point.x;
	ppt->y = point.y;
}

void CPythonApplication::SetMinFog(float fMinFog)
{
	MIN_FOG = fMinFog;
}

void CPythonApplication::SetFrameSkip(bool isEnable)
{
	if (isEnable)
		m_isFrameSkipDisable=false;
	else
		m_isFrameSkipDisable=true;
}

void CPythonApplication::NotifyHack(const char* c_szFormat, ...)
{
	char szBuf[1024];

	va_list args;
	va_start(args, c_szFormat);	
	_vsnprintf(szBuf, sizeof(szBuf), c_szFormat, args);
	va_end(args);
	m_pyNetworkStream.NotifyHack(szBuf);
}

void CPythonApplication::GetInfo(UINT eInfo, std::string* pstInfo)
{
	switch (eInfo)
	{
	case INFO_ACTOR:
		m_kChrMgr.GetInfo(pstInfo);
		break;
	case INFO_EFFECT:
		m_kEftMgr.GetInfo(pstInfo);			
		break;
	case INFO_ITEM:
		m_pyItem.GetInfo(pstInfo);
		break;
	case INFO_TEXTTAIL:
		m_pyTextTail.GetInfo(pstInfo);
		break;
	}
}

void CPythonApplication::Abort()
{
	TraceError("============================================================================================================");
	TraceError("Abort!!!!\n\n");

	GetPlatformWindow().RequestQuit(0);
}

void CPythonApplication::Exit()
{
	GetPlatformWindow().RequestQuit(0);
}

bool CPythonApplication::PrewarmModernWorld()
{
    if(!Renderer::modernFrame)return true;
    Renderer::LogClientLifecycle("LoadingPrewarmBegin");
    struct LoadingScope {
        LoadingScope(){Renderer::loadingPrewarm=true;}
        ~LoadingScope(){Renderer::loadingPrewarm=false;}
    } loading;
    Renderer::FirstUseAudit total("world-total","prewarm");
    // GameWindow::Open has installed the actual game camera. The previous
    // presented image is still LoadingWindow; no world frame is presented here.
    // Use the real scene once so its material, shadow, terrain and FX PSOs are ready.
    try {
        UpdateGame();
        __UpdateCamera();
        RenderGame();
        m_pyGraphic.PopState();
        m_pyGraphic.SetInterfaceRenderState();
        Renderer::LogClientLifecycle("LoadingPrewarmEnd");
        return true;
    } catch(const std::exception& error) {
        TraceError("Modern world prewarm failed: %s",error.what());
        Renderer::LogClientLifecycle("LoadingPrewarmFailed");
        return false;
    }
}

void CPythonApplication::RenderGame()
{
    if(Renderer::worldPrewarmPending&&!Renderer::loadingPrewarm) {
        Renderer::worldPrewarmPending=false;
        // SetGamePhase schedules a curtain transition. Only this callback runs
        // after GameWindow::Open installed the real camera and scene settings.
        if(!PrewarmModernWorld()) {
            m_rendererRuntimeFailed=true;GetPlatformWindow().RequestQuit(1);return;
        }
        Renderer::LogClientLifecycle("WorldReadyForPresent");
        Renderer::awaitingWorldPresent=true;
    }
    Renderer::FirstUseAudit firstVisible("world-total","first-visible",Renderer::awaitingWorldPresent);
    const auto benchmarkStart=Renderer::skinningBenchmarkEnabled ? Renderer::PrototypeClock::now() : Renderer::PrototypeClock::time_point{};
    // ZiiNAN: Indoor worlds do not require a terrain submission in the previous frame.
    if(Renderer::uiFrame && Renderer::worldRenderer) {
        Renderer::actorWorldFrame=Renderer::vegetationWorldFrame=true;
        Renderer::effectWorldFrame=Renderer::worldSurfaceFrame=true;
    }
	float fAspect = m_kWndMgr.GetAspect();
	float fFarClip = m_pyBackground.GetFarClip();

	m_pyGraphic.SetPerspective(30.0f, fAspect, 100.0, fFarClip);

	CCullingManager::Instance().Process();

    const auto deformStart=Renderer::skinningBenchmarkEnabled ? Renderer::PrototypeClock::now() : Renderer::PrototypeClock::time_point{};
	m_kChrMgr.Deform();
    if(Renderer::skinningBenchmarkEnabled) Renderer::skinningBenchmarkCurrent.deformUs+=Renderer::PrototypeMicroseconds(deformStart);

	m_pyBackground.RenderCharacterShadowToTexture();

	m_pyGraphic.SetGameRenderState();
	m_pyGraphic.PushState();

	{
		long lx, ly;
		m_kWndMgr.GetMousePosition(lx, ly);
		m_pyGraphic.SetCursorPosition(lx, ly);
	}

    if(!Renderer::modernFrame) {
        m_pyBackground.RenderSky();
        m_pyBackground.RenderBeforeLensFlare();
        m_pyBackground.RenderCloud();
    }

	m_pyBackground.BeginEnvironment();
    if(Renderer::modernFrame) {
        Graphics::SceneLighting light;
        const TEnvironmentData* environment=nullptr;
        m_pyBackground.GetCurrentEnvironmentData(&environment);
        if(environment) {
            const auto& sun=environment->DirLights[ENV_DIRLIGHT_BACKGROUND];
            const auto& config=Renderer::GetGraphicsRuntimeConfig();
            Graphics::LegacyEnvironmentLight source;
            source.direction={sun.Direction.x,sun.Direction.y,sun.Direction.z};
            source.diffuse={sun.Diffuse.r,sun.Diffuse.g,sun.Diffuse.b};
            source.ambient={sun.Ambient.r,sun.Ambient.g,sun.Ambient.b};
            const auto& material=environment->Material;
            source.materialDiffuse={material.Diffuse.r,material.Diffuse.g,material.Diffuse.b};
            source.materialAmbient={material.Ambient.r,material.Ambient.g,material.Ambient.b};
            source.environmentFill={material.Emissive.r,material.Emissive.g,material.Emissive.b};
            source.enabled=environment->bDirLightsEnable[ENV_DIRLIGHT_BACKGROUND]!=FALSE;
            light=Graphics::ResolveLegacyEnvironmentLight(source);
            light.fogColor={environment->FogColor.r,environment->FogColor.g,environment->FogColor.b};
            light.fogEnabled=environment->bFogEnable!=FALSE;
            light.densityFog=environment->bDensityFog&&environment->bFogLevel!=0;
            light.fogDensity=environment->bFogLevel*config.fogDensity;
            light.fogNear=environment->GetFogNearDistance()*config.fogDistanceScale;
            light.fogFar=environment->GetFogFarDistance()*config.fogDistanceScale;
        }
#ifdef M2_RENDERER_DIAGNOSTICS
        if(Graphics::developmentSunState>=0)light=Graphics::WithDevelopmentSun(light,unsigned(Graphics::developmentSunState));
#endif
        Renderer::modernFrame->Begin(light,true);
        Renderer::TerrainMatrices camera;
        Math::Matrix view,projection;
        DRAWSTATE.GetTransform(Renderer::MatrixView,&view);
        DRAWSTATE.GetTransform(Renderer::MatrixProjection,&projection);
        std::memcpy(camera.view.data(),&view,64);
        std::memcpy(camera.projection.data(),&projection,64);
        Renderer::modernFrame->SetCamera(camera);
    }
	m_pyBackground.Render();

	m_pyBackground.SetCharacterDirLight();
    const auto actorRenderStart=Renderer::skinningBenchmarkEnabled ? Renderer::PrototypeClock::now() : Renderer::PrototypeClock::time_point{};
	m_kChrMgr.Render();
    if(Renderer::skinningBenchmarkEnabled) Renderer::skinningBenchmarkCurrent.renderUs+=Renderer::PrototypeMicroseconds(actorRenderStart);

	m_pyBackground.SetBackgroundDirLight();
    if(Renderer::modernFrame) {
        if(Renderer::modernFrame->BeginShadowCollection()) {
            try {
                m_pyBackground.Render();
                m_pyBackground.SetCharacterDirLight();
                m_kChrMgr.Deform();
                m_kChrMgr.Render();
                m_pyBackground.SetBackgroundDirLight();
            } catch(...) {Renderer::modernFrame->EndShadowCollection();throw;}
            Renderer::modernFrame->EndShadowCollection();
        }
        Renderer::modernFrame->End();
    }
    if(Renderer::modernFrame)m_kChrMgr.RenderWorldTraces();
	m_pyBackground.RenderWater();
	m_pyBackground.RenderSnow();
	m_pyBackground.RenderEffect();

	m_pyBackground.EndEnvironment();

	m_kEftMgr.Render(Renderer::modernFrame?CEffectManager::RenderPass::World:CEffectManager::RenderPass::All);
    if(Renderer::modernFrame)Renderer::modernFrame->BeginForwardWorld();
	m_pyItem.Render();
    if(Renderer::modernFrame)Renderer::modernFrame->EndForwardWorld();
	m_FlyingManager.Render();

	m_pyBackground.BeginEnvironment();
    if(Renderer::modernFrame)Renderer::modernFrame->BeginForwardWorld();
	m_pyBackground.RenderPCBlocker();
    if(Renderer::modernFrame)Renderer::modernFrame->EndForwardWorld();
	m_pyBackground.EndEnvironment();

    if(Renderer::modernFrame) {
        Renderer::modernFrame->FinishWorld();
        m_kEftMgr.Render(CEffectManager::RenderPass::Screen);
    } else m_pyBackground.RenderAfterLensFlare();
    if(Renderer::skinningBenchmarkEnabled) Renderer::skinningBenchmarkCurrent.worldUs+=Renderer::PrototypeMicroseconds(benchmarkStart);
}

void CPythonApplication::UpdateGame()
{
	POINT ptMouse;
	GetMousePosition(&ptMouse);

	CGraphicTextInstance::Hyperlink_UpdateMousePos(ptMouse.x, ptMouse.y);


	//!@# Alt+Tab Áß SetTransfor ¿¡¼­ Æ¨±è Çö»ó ÇØ°áÀ» À§ÇØ - [levites]
	//if (m_isActivateWnd)
	{
		CScreen s;
		float fAspect = UI::CWindowManager::Instance().GetAspect();
		float fFarClip = CPythonBackground::Instance().GetFarClip();

		s.SetPerspective(30.0f,fAspect, 100.0f, fFarClip);
		s.BuildViewFrustum();
	}

	TPixelPosition kPPosMainActor;
	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);

	m_pyBackground.Update(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);

	m_GameEventManager.SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
	m_GameEventManager.Update();

	m_kChrMgr.Update();

	m_kEftMgr.Update();
	m_kEftMgr.UpdateSound();
	
	m_FlyingManager.Update();
	m_pyItem.Update(ptMouse);
	m_pyPlayer.Update();

	// NOTE : Update µ¿¾È À§Ä¡ °ªÀÌ ¹Ù²î¹Ç·Î ´Ù½Ã ¾ò¾î ¿É´Ï´Ù - [levites]
	//        ÀÌ ºÎºÐ ¶§¹®¿¡ ¸ÞÀÎ ÄÉ¸¯ÅÍÀÇ Sound°¡ ÀÌÀü À§Ä¡¿¡¼­ ÇÃ·¹ÀÌ µÇ´Â Çö»óÀÌ ÀÖ¾úÀ½.
	m_pyPlayer.NEW_GetMainActorPosition(&kPPosMainActor);
	SetCenterPosition(kPPosMainActor.x, kPPosMainActor.y, kPPosMainActor.z);
}

bool CPythonApplication::Process()
{
    m_pySystem.FlushGraphicsSettings();
    if(AssetRuntime::AnimationStallAudit::fullCapture && !AssetRuntime::AnimationStallAudit::explicitPhase) {
        auto* actor=m_kChrMgr.GetMainInstancePtr();
        AssetRuntime::AnimationStallAudit::capturePhase=!m_pyNetworkStream.IsGamePhaseForDiagnostics()?0:
            actor && actor->IsAttacking()?3:actor && actor->IsWalking()?2:1;
    }
    Renderer::AnimationStallFrame stallFrame(
        AssetRuntime::AnimationStallAudit::enabled && m_pyNetworkStream.IsGamePhaseForDiagnostics(),
        m_isMinimizedWnd != 0, m_isActivateWnd != 0);
    // ZiiNAN: GPU skinning production path — no timers or capture in ordinary sessions.
    Renderer::SkinningBenchmarkProcessScope benchmarkProcess;
	ELTimer_SetFrameMSec();

	// 	m_Profiler.Clear();
	DWORD dwStart = ELTimer_GetMSec();

	///////////////////////////////////////////////////////////////////////////////////////////////////
	static DWORD	s_dwUpdateFrameCount = 0;
	static DWORD	s_dwRenderFrameCount = 0;
	static DWORD	s_dwFaceCount = 0;
	static UINT		s_uiLoad = 0;
	static DWORD	s_dwCheckTime = ELTimer_GetMSec();

	if (ELTimer_GetMSec() - s_dwCheckTime > 1000) [[unlikely]] {
		m_dwUpdateFPS		= s_dwUpdateFrameCount;
		m_dwRenderFPS		= s_dwRenderFrameCount;
		m_dwLoad			= s_uiLoad;

		m_dwFaceCount		= s_dwFaceCount / std::max(1ul, s_dwRenderFrameCount);

		s_dwCheckTime		= ELTimer_GetMSec();

		s_uiLoad = s_dwFaceCount = s_dwUpdateFrameCount = s_dwRenderFrameCount = 0;
	}

	// Update Time
	static BOOL s_bFrameSkip = false;
	static UINT s_uiNextFrameTime = ELTimer_GetMSec();

#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime1=ELTimer_GetMSec();
#endif
	CTimer& rkTimer=CTimer::Instance();
	rkTimer.Advance();

	m_fGlobalTime = rkTimer.GetCurrentSecond();
	m_fGlobalElapsedTime = rkTimer.GetElapsedSecond();

	UINT uiFrameTime = rkTimer.GetElapsedMilliecond();
	s_uiNextFrameTime += uiFrameTime;	//17 - 1ÃÊ´ç 60fps±âÁØ.

	DWORD updatestart = ELTimer_GetMSec();
    AssetRuntime::AnimationStallAudit::WorkScope stallUpdate(AssetRuntime::AnimationStallAudit::Work::Update);
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime2=ELTimer_GetMSec();
#endif
	// Network I/O	
	m_pyNetworkStream.Process();	
	//m_pyNetworkDatagram.Process();

	m_kGuildMarkUploader.Process();

	m_kGuildMarkDownloader.Process();
	m_kAccountConnector.Process();

#ifdef __PERFORMANCE_CHECK__		
	DWORD dwUpdateTime3=ELTimer_GetMSec();
#endif
	//////////////////////
	// Input Process
	// Keyboard
	UpdateKeyboard();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime4=ELTimer_GetMSec();
#endif
	// Mouse
	Platform::Point point;
	if (GetPlatformWindow().GetCursorScreenPosition(point) && GetPlatformWindow().ScreenToClient(point)) [[likely]] {
		OnMouseMove(point.x, point.y);
	}
	//////////////////////
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime5=ELTimer_GetMSec();
#endif
	//!@# Alt+Tab Áß SetTransfor ¿¡¼­ Æ¨±è Çö»ó ÇØ°áÀ» À§ÇØ - [levites]
	//if (m_isActivateWnd)
	__UpdateCamera();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime6=ELTimer_GetMSec();
#endif
	// Update Game Playing
	CResourceManager::Instance().Update();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime7=ELTimer_GetMSec();
#endif
	OnCameraUpdate();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime8=ELTimer_GetMSec();
#endif
	OnMouseUpdate();
#ifdef __PERFORMANCE_CHECK__
	DWORD dwUpdateTime9=ELTimer_GetMSec();
#endif
	OnUIUpdate();
    stallUpdate.Stop();

#ifdef __PERFORMANCE_CHECK__		
	DWORD dwUpdateTime10=ELTimer_GetMSec();

	if (dwUpdateTime10-dwUpdateTime1>10)
	{			
		static FILE* fp=fopen("perf_app_update.txt", "w");

		fprintf(fp, "AU.Total %d (Time %d)\n", dwUpdateTime9-dwUpdateTime1, ELTimer_GetMSec());
		fprintf(fp, "AU.TU %d\n", dwUpdateTime2-dwUpdateTime1);
		fprintf(fp, "AU.NU %d\n", dwUpdateTime3-dwUpdateTime2);
		fprintf(fp, "AU.KU %d\n", dwUpdateTime4-dwUpdateTime3);
		fprintf(fp, "AU.MP %d\n", dwUpdateTime5-dwUpdateTime4);
		fprintf(fp, "AU.CP %d\n", dwUpdateTime6-dwUpdateTime5);
		fprintf(fp, "AU.RU %d\n", dwUpdateTime7-dwUpdateTime6);
		fprintf(fp, "AU.CU %d\n", dwUpdateTime8-dwUpdateTime7);
		fprintf(fp, "AU.MU %d\n", dwUpdateTime9-dwUpdateTime8);
		fprintf(fp, "AU.UU %d\n", dwUpdateTime10-dwUpdateTime9);			
		fprintf(fp, "----------------------------------\n");
		fflush(fp);
	}		
#endif

	//UpdateÇÏ´Âµ¥ °É¸°½Ã°£.delta°ª
	m_dwCurUpdateTime = ELTimer_GetMSec() - updatestart;

	DWORD dwCurrentTime = ELTimer_GetMSec();
	BOOL  bCurrentLateUpdate = FALSE;

	s_bFrameSkip = false;

	if (dwCurrentTime > s_uiNextFrameTime)
	{
		int dt = dwCurrentTime - s_uiNextFrameTime;
		int nAdjustTime = ((float)dt / (float)uiFrameTime) * uiFrameTime; 

		if ( dt >= 500 )
		{
			s_uiNextFrameTime += nAdjustTime; 
			printf("FrameSkip º¸Á¤ %d\n",nAdjustTime);
			CTimer::Instance().Adjust(nAdjustTime);
		}

		s_bFrameSkip = true;
		bCurrentLateUpdate = TRUE;
	}

	//s_bFrameSkip = false;

	//if (dwCurrentTime > s_uiNextFrameTime)
	//{
	//	int dt = dwCurrentTime - s_uiNextFrameTime;

	//	//³Ê¹« ´Ê¾úÀ» °æ¿ì µû¶óÀâ´Â´Ù.
	//	//±×¸®°í m_dwCurUpdateTime´Â deltaÀÎµ¥ delta¶û absolute timeÀÌ¶û ºñ±³ÇÏ¸é ¾îÂ¼ÀÚ´Â°Ü?
	//	//if (dt >= 500 || m_dwCurUpdateTime > s_uiNextFrameTime)

	//	//±âÁ¸ÄÚµå´ë·Î ÇÏ¸é 0.5ÃÊ ÀÌÇÏ Â÷ÀÌ³­ »óÅÂ·Î update°¡ Áö¼ÓµÇ¸é °è¼Ó rendering frame skip¹ß»ý
	//	if (dt >= 500 || m_dwCurUpdateTime > s_uiNextFrameTime)
	//	{
	//		s_uiNextFrameTime += dt / uiFrameTime * uiFrameTime; 
	//		printf("FrameSkip º¸Á¤ %d\n", dt / uiFrameTime * uiFrameTime);
	//		CTimer::Instance().Adjust((dt / uiFrameTime) * uiFrameTime);
	//		s_bFrameSkip = true;
	//	}
	//}

	if (m_isFrameSkipDisable)
		s_bFrameSkip = false;

#ifdef __VTUNE__
	s_bFrameSkip = false;
#endif
	if (!s_bFrameSkip)
	{
		//		static double pos=0.0f;
		//		CGrannyMaterial::TranslateSpecularMatrix(fabs(sin(pos)*0.005), fabs(cos(pos)*0.005), 0.0f);
		//		pos+=0.01f;

		CGrannyMaterial::TranslateSpecularMatrix(g_specularSpd, g_specularSpd, 0.0f);

		DWORD dwRenderStartTime = ELTimer_GetMSec();		

		bool canRender = true;

		if (m_isMinimizedWnd) [[unlikely]] {
			canRender = false;
		}

		if (canRender) [[likely]]
		{
            AssetRuntime::AnimationStallAudit::WorkScope stallSubmission(AssetRuntime::AnimationStallAudit::Work::Submission);
			// RestoreLostDevice
			CCullingManager::Instance().Update();
			if (m_terrainPresentation) [[likely]] {
                m_pyGraphic.Begin();

				/////////////////////
				// Interface
				if (m_terrainPresentation && !m_terrainPresentation->BeginFrame())
				{
					TraceError("Diligent terrain BeginFrame failed");
                    m_rendererRuntimeFailed = true;
					m_pyGraphic.End();
					GetPlatformWindow().RequestQuit(1);
					return false;
				}
				m_pyGraphic.SetInterfaceRenderState();

				m_pyNetworkStream.PrepareGamePhase();
				OnUIRender();
				OnMouseRender();
				/////////////////////

				m_pyGraphic.End();

				//DWORD t1 = ELTimer_GetMSec();
                stallSubmission.Stop();
                AssetRuntime::AnimationStallAudit::WorkScope stallPresentation(AssetRuntime::AnimationStallAudit::Work::Presentation);
				if (m_terrainPresentation && !m_terrainPresentation->Present())
				{
					TraceError("Diligent terrain rendering failed (resource, camera or legacy state mismatch)");
                    m_rendererRuntimeFailed = true;
					GetPlatformWindow().RequestQuit(1);
					return false;
				}
				//DWORD t2 = ELTimer_GetMSec();
                stallPresentation.Stop();
                stallFrame.Presented();


				DWORD dwRenderEndTime = ELTimer_GetMSec();

				static DWORD s_dwRenderCheckTime = dwRenderEndTime;
				static DWORD s_dwRenderRangeTime = 0;
				static DWORD s_dwRenderRangeFrame = 0;

				m_dwCurRenderTime = dwRenderEndTime - dwRenderStartTime;			
				s_dwRenderRangeTime += m_dwCurRenderTime;				
				++s_dwRenderRangeFrame;			

				if (dwRenderEndTime-s_dwRenderCheckTime>1000) [[unlikely]] {
					m_fAveRenderTime=float(double(s_dwRenderRangeTime)/double(s_dwRenderRangeFrame));

					s_dwRenderCheckTime=ELTimer_GetMSec();
					s_dwRenderRangeTime=0;
					s_dwRenderRangeFrame=0;
				}										

				DWORD dwCurFaceCount=m_pyGraphic.GetFaceCount();
				m_pyGraphic.ResetFaceCount();
				s_dwFaceCount += dwCurFaceCount;

				if (dwCurFaceCount > 5000)
				{
					m_dwFaceAccCount += dwCurFaceCount;
					m_dwFaceAccTime += m_dwCurRenderTime;

					m_fFaceSpd=(m_dwFaceAccCount/m_dwFaceAccTime);

					// °Å¸® ÀÚµ¿ Á¶Àý
					if (-1 == m_iForceSightRange)
					{
						static float s_fAveRenderTime = 16.0f;
						float fRatio=0.3f;
						s_fAveRenderTime=(s_fAveRenderTime*(100.0f-fRatio)+std::max(16.0f, (float)m_dwCurRenderTime)*fRatio)/100.0f;


						float fFar=Renderer::GetGraphicsRuntimeConfig().viewDistance;
						float fNear=MIN_FOG;
						double dbAvePow=double(1000.0f/s_fAveRenderTime);
						double dbMaxPow=60.0;
						float fDistance=std::max((float)(fNear+(fFar-fNear)*(dbAvePow)/dbMaxPow), fNear);
						m_pyBackground.SetViewDistanceSet(0, fDistance);
					}
					// °Å¸® °­Á¦ ¼³Á¤½Ã
					else
					{
						m_pyBackground.SetViewDistanceSet(0, float(m_iForceSightRange));
					}
				}
				else
				{
					// 10000 Æú¸®°ï º¸´Ù ÀûÀ»¶§´Â °¡Àå ¸Ö¸® º¸ÀÌ°Ô ÇÑ´Ù
					m_pyBackground.SetViewDistanceSet(0, Renderer::GetGraphicsRuntimeConfig().viewDistance);
				}

				++s_dwRenderFrameCount;
			}
		}
	}

    benchmarkProcess.BeforeFrameLimit();
	int rest = s_uiNextFrameTime - ELTimer_GetMSec();

	if (rest > 0 && !bCurrentLateUpdate )
	{
		s_uiLoad -= rest;	// ½® ½Ã°£Àº ·Îµå¿¡¼­ »«´Ù..
        AssetRuntime::AnimationStallAudit::WorkScope stallSleep(AssetRuntime::AnimationStallAudit::Work::Sleep);
		Platform::Time::SleepMilliseconds(static_cast<std::uint32_t>(rest));
	}	

	++s_dwUpdateFrameCount;

	s_uiLoad += ELTimer_GetMSec() - dwStart;
	//m_Profiler.ProfileByScreen();	
	return true;
}

void CPythonApplication::UpdateClientRect()
{
	const auto rcApp = GetClientRect();
	OnSizeChange(rcApp.right - rcApp.left, rcApp.bottom - rcApp.top);
}

void CPythonApplication::SetMouseHandler(PyObject* poMouseHandler)
{	
	m_poMouseHandler = poMouseHandler;
}

int CPythonApplication::CheckDeviceState()
{
    return m_terrainPresentation ? DEVICE_STATE_OK : DEVICE_STATE_FALSE;
}

bool CPythonApplication::CreateDevice(int width, int height, int Windowed, int bit, int frequency)
{
    // ZiiNAN: Legacy D3D9 renderer removed from production path.
    if (m_terrainPresentation) return false;
    m_terrainPresentation=Renderer::CreateTerrainPresentation(GetPlatformWindow(),width,height);
    if(!m_terrainPresentation) return FailRendererStartup("Diligent D3D11 initialization failed. No fallback.");
    if(m_grpDevice.Create(GetNativeHandle(),width,height,Windowed!=0,bit,frequency)!=CGraphicDevice::CREATE_OK) {
        m_terrainPresentation.reset();
        return FailRendererStartup("CPU graphics context initialization failed. No fallback.");
    }
    return true;
}

void CPythonApplication::SetUserMovingMainWindow(bool flag)
{
	if (flag && !GetPlatformWindow().GetCursorScreenPosition(m_InitialMouseMovingPoint))
		return;

	m_IsMovingMainWindow = flag;
}

bool CPythonApplication::IsUserMovingMainWindow() const
{
	return m_IsMovingMainWindow;
}

void CPythonApplication::UpdateMainWindowPosition()
{
	Platform::Point finalPoint{};
	if (GetPlatformWindow().GetCursorScreenPosition(finalPoint))
	{
		LONG xDiff = finalPoint.x - m_InitialMouseMovingPoint.x;
		LONG yDiff = finalPoint.y - m_InitialMouseMovingPoint.y;

		const auto r = GetWindowRect();

		SetPosition(r.left + xDiff, r.top + yDiff);
		m_InitialMouseMovingPoint = finalPoint;
	}
}

void CPythonApplication::Loop()
{	
	while (1)
	{	
		if (IsUserMovingMainWindow())
			UpdateMainWindowPosition();
		
		const auto eventResult = PollEvents();
		if (eventResult == Platform::PollResult::Quit)
			break;
		if (eventResult == Platform::PollResult::Idle)
		{
			if (!Process())
				break;

			m_dwLastIdleTime=ELTimer_GetMSec();
		}
	}
}

// ZiiNAN: Preserve renderer startup failure even if the original Python script catches it.
bool CPythonApplication::FailRendererStartup(const char* message)
{
    m_rendererStartupFailed = true;
    TraceError("Renderer startup: %s", message);
    PyErr_SetString(PyExc_RuntimeError, message);
    return false;
}

bool LoadLocaleData(const char* localePath)
{
	CPythonNonPlayer&	rkNPCMgr	= CPythonNonPlayer::Instance();
	CItemManager&		rkItemMgr	= CItemManager::Instance();	
	CPythonSkill&		rkSkillMgr	= CPythonSkill::Instance();
	CPythonNetworkStream& rkNetStream = CPythonNetworkStream::Instance();

	char szItemList[256];
	char szItemProto[256];
	char szItemDesc[256];
	char szMobProto[256];
	char szSkillDescFileName[256];
	char szSkillTableFileName[256];
	char szInsultList[256];

	snprintf (szItemList,	sizeof (szItemList),	"%s/item_list.txt", GetLocalePathCommon());
	snprintf (szItemProto,	sizeof (szItemProto),	"%s/item_proto",	localePath);
	snprintf (szItemDesc,	sizeof (szItemDesc),	"%s/itemdesc.txt",	localePath);
	snprintf (szMobProto,	sizeof (szMobProto),	"%s/mob_proto",		localePath);
	snprintf (szSkillDescFileName, sizeof (szSkillDescFileName),	"%s/SkillDesc.txt", localePath);
	snprintf (szSkillTableFileName, sizeof (szSkillTableFileName),	"%s/SkillTable.txt", GetLocalePathCommon());
	snprintf (szInsultList,	sizeof (szInsultList),	"%s/insult.txt", localePath);

	rkNPCMgr.Destroy();
	rkItemMgr.Destroy();
	rkSkillMgr.Destroy();

	if (!rkItemMgr.LoadItemList(szItemList))
	{
		TraceError("LoadLocaleData - LoadItemList(%s) Error", szItemList);
	}

	if (!rkItemMgr.LoadItemTable(szItemProto))
	{
		TraceError("LoadLocaleData - LoadItemProto(%s) Error", szItemProto);
		return false;
	}

	if (!rkItemMgr.LoadItemDesc(szItemDesc))
	{
		Tracenf("LoadLocaleData - LoadItemDesc(%s) Error", szItemDesc);
	}

	if (!rkNPCMgr.LoadNonPlayerData(szMobProto))
	{
		TraceError("LoadLocaleData - LoadMobProto(%s) Error", szMobProto);
		return false;
	}

	if (!rkSkillMgr.RegisterSkillDesc(szSkillDescFileName))
	{
		TraceError("LoadLocaleData - RegisterSkillDesc(%s) Error", szMobProto);
		return false;
	}

	if (!rkSkillMgr.RegisterSkillTable(szSkillTableFileName))
	{
		TraceError("LoadLocaleData - RegisterSkillTable(%s) Error", szMobProto);
		return false;
	}

	if (!rkNetStream.LoadInsultList(szInsultList))
	{
		Tracenf("CPythonApplication - CPythonNetworkStream::LoadInsultList(%s)", szInsultList);
	}

	return true;
}

Platform::WindowStyle __GetWindowMode(bool windowed)
{
	if (windowed)
		return Platform::WindowStyle::Windowed;

	return Platform::WindowStyle::Popup;
}

bool CPythonApplication::Create(PyObject * poSelf, const char * c_szName, int width, int height, int Windowed)
{
	// Initialize Game Thread Pool first - required by other systems
	CGameThreadPool* pThreadPool = CGameThreadPool::InstancePtr();
	if (pThreadPool)
	{
		pThreadPool->Initialize();
	}

	NANOBEGIN
		Windowed = CPythonSystem::Instance().IsWindowed() ? 1 : 0;
        if(m_startupBackend == Renderer::BackendKind::DiligentD3D11 && !Windowed)
            return FailRendererStartup("Diligent D3D11 requires windowed mode (WINDOWED 1). Legacy fullscreen is no longer supported. No fallback.");

	bool bAnotherWindow = false;

	if (GetPlatformWindow().HasWindowWithTitle(c_szName))
		bAnotherWindow = true;

	m_dwWidth = width;
	m_dwHeight = height;

	// Window
	const auto windowMode = __GetWindowMode(Windowed != 0);

	Platform::WindowCreateInfo windowInfo;
	windowInfo.title = c_szName ? c_szName : "";
	windowInfo.style = windowMode;
	windowInfo.classBrush = 4;
	windowInfo.iconResource = IDI_METIN2;
	windowInfo.cursorResource = IDC_CURSOR_NORMAL;
	if (!CMSWindow::Create(windowInfo))
	{
		TraceError("CMSWindow::Create failed");
		SET_EXCEPTION(CREATE_WINDOW);
		return false;
	}

	if (m_pySystem.IsUseDefaultIME())
	{
		CPythonIME::Instance().UseDefaultIME();
	}

#if defined(ENABLE_DISCORD_RPC)
	m_pyNetworkStream.Discord_Start();
#endif

	if (!m_pySystem.IsWindowed())
	{
		m_isWindowed = false;
		m_isWindowFullScreenEnable = TRUE;
		__SetFullScreenWindow(width, height, m_pySystem.GetBPP());

		Windowed = true;
	}
	else
	{
		AdjustSize(m_pySystem.GetWidth(), m_pySystem.GetHeight());

		if (Windowed)
		{
			m_isWindowed = true;

			if (bAnotherWindow)
			{
				const auto rc = GetClientRect();

				int windowWidth = rc.right - rc.left;
				int windowHeight = (rc.bottom - rc.top);

				CMSApplication::SetPosition(GetScreenWidth() - windowWidth, GetScreenHeight() - 60 - windowHeight);
			}
			SetPosition(-8, 0); //Fix
		}
		else
		{
			m_isWindowed = false;
			SetPosition(0, 0);
		}
	}

	NANOEND
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
		///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Cursor
		if (!CreateCursors())
		{
			TraceError("CMSWindow::Cursors Create Error");
			SET_EXCEPTION("CREATE_CURSOR");
			return false;
		}

		if (!m_pySystem.IsNoSoundCard())
		{
			// Sound
			if (!m_SoundEngine.Initialize())
			{
				TraceError("Failed to initialize sound manager!");
				return false; // Is this important enough to stop the client?
			}
		}

		extern bool GRAPHICS_CAPS_SOFTWARE_TILING;

		if (!m_pySystem.IsAutoTiling())
			GRAPHICS_CAPS_SOFTWARE_TILING = m_pySystem.IsSoftwareTiling();

		// Device
		if (!CreateDevice(m_pySystem.GetWidth(), m_pySystem.GetHeight(), Windowed, m_pySystem.GetBPP(), m_pySystem.GetFrequency()))
        {
            m_rendererStartupFailed = true;
            return false;
        }

		CreateSharedDeformBuffer();

		if (m_pySystem.IsAutoTiling())
		{
			if (m_grpDevice.IsFastTNL())
			{
				m_pyBackground.ReserveSoftwareTilingEnable(false);
			}
			else
			{
				m_pyBackground.ReserveSoftwareTilingEnable(true);
			}
		}
		else
		{
			m_pyBackground.ReserveSoftwareTilingEnable(m_pySystem.IsSoftwareTiling());
		}

		SetVisibleMode(true);

		if (m_isWindowFullScreenEnable)
		{
			SetSize(width, height);
			Show();
		}

		if (!InitializeKeyboard(GetNativeHandle()))
			return false;

		m_pySystem.GetDisplaySettings();

		// Mouse
		if (m_pySystem.IsSoftwareCursor())
			SetCursorMode(CURSOR_MODE_SOFTWARE);
		else
			SetCursorMode(CURSOR_MODE_HARDWARE);

		// Network
		if (!m_netDevice.Create())
		{
			TraceError("NetDevice::Create failed");
			SET_EXCEPTION("CREATE_NETWORK");
			return false;
		}

		if (!m_grpDevice.IsFastTNL())
			CGrannyLODController::SetMinLODMode(true);

		m_pyItem.Create();

		// Other Modules
		DefaultFont_Startup();

		CPythonIME::Instance().Create(static_cast<HWND>(GetNativeHandle().value));
		CPythonIME::Instance().SetText("", 0);
		CPythonTextTail::Instance().Initialize();

		// Light Manager
		m_LightManager.Initialize();

		CGraphicImageInstance::CreateSystem(32);

		// ¹é¾÷
		DisableAccessibilityShortcuts();

		// SphereMap
		CGrannyMaterial::CreateSphereMap(0, "d:/ymir work/special/spheremap.jpg");
		CGrannyMaterial::CreateSphereMap(1, "d:/ymir work/special/spheremap01.jpg");
		return true;
}

void CPythonApplication::SetGlobalCenterPosition(int32_t x, int32_t y)
{
	CPythonBackground& rkBG=CPythonBackground::Instance();
	rkBG.GlobalPositionToLocalPosition(x, y);

	float z = CPythonBackground::Instance().GetHeight(x, y);

	CPythonApplication::Instance().SetCenterPosition(x, y, z);
}

void CPythonApplication::SetCenterPosition(float fx, float fy, float fz)
{
	m_v3CenterPosition.x = +fx;
	m_v3CenterPosition.y = -fy;
	m_v3CenterPosition.z = +fz;
}

void CPythonApplication::GetCenterPosition(TPixelPosition * pPixelPosition)
{
	pPixelPosition->x = +m_v3CenterPosition.x;
	pPixelPosition->y = -m_v3CenterPosition.y;
	pPixelPosition->z = +m_v3CenterPosition.z;
}


void CPythonApplication::SetServerTime(time_t tTime)
{
	m_dwStartLocalTime	= ELTimer_GetMSec();
	m_tServerTime		= tTime;
	m_tLocalStartTime	= time(0);
}

time_t CPythonApplication::GetServerTime()
{
	return (ELTimer_GetMSec() - m_dwStartLocalTime) + m_tServerTime;
}

// 2005.03.28 - MALL ¾ÆÀÌÅÛ¿¡ µé¾îÀÖ´Â ½Ã°£ÀÇ ´ÜÀ§°¡ ¼­¹ö¿¡¼­ time(0) À¸·Î ¸¸µé¾îÁö´Â
//              °ªÀÌ±â ¶§¹®¿¡ ´ÜÀ§¸¦ ¸ÂÃß±â À§ÇØ ½Ã°£ °ü·Ã Ã³¸®¸¦ º°µµ·Î Ãß°¡
time_t CPythonApplication::GetServerTimeStamp()
{
	return (time(0) - m_tLocalStartTime) + m_tServerTime;
}

float CPythonApplication::GetGlobalTime()
{
	return m_fGlobalTime;
}

float CPythonApplication::GetGlobalElapsedTime()
{
	return m_fGlobalElapsedTime;
}

void CPythonApplication::SetFPS(int iFPS)
{
	m_iFPS = iFPS;
}

int CPythonApplication::GetWidth()
{
	return m_dwWidth;
}

int CPythonApplication::GetHeight()
{
	return m_dwHeight;
}

void CPythonApplication::SetConnectData(const char * c_szIP, int iPort)
{
	m_strIP = c_szIP;
	m_iPort = iPort;
}

void CPythonApplication::GetConnectData(std::string & rstIP, int & riPort)
{
	rstIP	= m_strIP;
	riPort	= m_iPort;
}

void CPythonApplication::EnableSpecialCameraMode()
{
	m_isSpecialCameraMode = TRUE;
}

void CPythonApplication::SetCameraSpeed(int iPercentage)
{
	m_fCameraRotateSpeed = c_fDefaultCameraRotateSpeed * float(iPercentage) / 100.0f;
	m_fCameraPitchSpeed = c_fDefaultCameraPitchSpeed * float(iPercentage) / 100.0f;
	m_fCameraZoomSpeed = c_fDefaultCameraZoomSpeed * float(iPercentage) / 100.0f;
}

void CPythonApplication::SetForceSightRange(int iRange)
{
	m_iForceSightRange = iRange;
}

void CPythonApplication::Clear()
{
	m_pySystem.Clear();
}

void CPythonApplication::Destroy()
{
	// SphereMap
	CGrannyMaterial::DestroySphereMap();

	m_kWndMgr.Destroy();

	CPythonSystem::Instance().SaveConfig();

	m_pySystem.SaveInterfaceStatus();

	m_pyEventManager.Destroy();	
	m_FlyingManager.Destroy();

	m_pyMiniMap.Destroy();

	m_pyChat.Destroy();
	m_kChrMgr.Destroy();
	m_RaceManager.Destroy();

	m_pyItem.Destroy();
	m_kItemMgr.Destroy();
	m_pyTextTail.Destroy(); // ZiiNAN: Floating-text owners detach before the tail pool is released.

	m_pyBackground.Destroy();

	m_kEftMgr.Destroy();
	m_LightManager.Destroy();

    // A cancelled native prewarm can leave the map owned by LoadingWindow,
    // without GameWindow::Close. Release owners before the collision pool.
    DestroyCollisionInstanceSystem();

	// Game Thread Pool
	CGameThreadPool::Instance().Destroy();

	// DEFAULT_FONT
	DefaultFont_Cleanup();
	// END_OF_DEFAULT_FONT

	DestroySharedDeformBuffer();

	m_pyGraphic.Destroy();
	
#if defined(ENABLE_DISCORD_RPC)
	m_pyNetworkStream.Discord_Close();
#endif	
	
	//m_pyNetworkDatagram.Destroy();	

	m_pyRes.Destroy();

	m_kGuildMarkDownloader.Disconnect();

	CGrannyModelInstance::DestroySystem();
	CGraphicImageInstance::DestroySystem();

	m_terrainPresentation.reset();
    m_grpDevice.Destroy();


	CAttributeInstance::DestroySystem();
	CTextFileLoader::DestroySystem();
	DestroyCursors();

	CMSApplication::Destroy();

	RestoreAccessibilityShortcuts();
}
