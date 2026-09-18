#include "StdAfx.h"
#include "PythonSystem.h"
#include "Platform/PlatformFilesystem.h"
#include "PythonApplication.h"
#include "Graphics/GraphicsSettingsFile.h"
#include "Renderer/GraphicsConfig.h"
#include "DisplayConfiguration.h"
#include "Platform/PlatformTime.h"

bool CPythonSystem::ApplyGraphicsSettings(Graphics::GraphicsSettings settings)
{
    if (!m_graphics.IsOwnerThread()) return false;
    if (!DisplayConfiguration::Equal(settings, GetGraphicsSettings()) && m_displayReady)
    {
        if (m_displayQueued || m_displayPending) return false;
        const auto window = static_cast<HWND>(CPythonApplication::Instance().GetNativeHandle().value);
        const auto monitor = DisplayConfiguration::Query(window);
        if (settings.displayMode == Graphics::DisplayMode::Borderless)
        {
            settings.resolutionWidth = monitor.desktop.first;
            settings.resolutionHeight = monitor.desktop.second;
        }
        if (!DisplayConfiguration::Normalize(settings, monitor, false)) return false;
        if (!DisplayConfiguration::Equal(settings, GetGraphicsSettings()))
        {
            m_displayPrevious = GetGraphicsSettings();
            m_displayQueued = true;
        }
    }
    return m_graphics.ApplyGraphicsSettings(settings);
}

bool CPythonSystem::ApplyGraphicsPreset(Graphics::GraphicsPreset preset)
{
    return m_graphics.ApplyGraphicsPreset(preset);
}

bool CPythonSystem::LoadGraphicsSettings()
{
    if (!m_graphics.IsOwnerThread()) return false;
    std::string error;
    auto defaults = Graphics::MigrateLegacy(m_Config.iShadowLevel, m_Config.iFogLevel);
    defaults.resolutionWidth = m_Config.width; defaults.resolutionHeight = m_Config.height;
    defaults.displayMode = m_Config.bWindowed ? Graphics::DisplayMode::Windowed : Graphics::DisplayMode::Borderless;
    const auto monitor = DisplayConfiguration::Query(m_displayReady ?
        static_cast<HWND>(CPythonApplication::Instance().GetNativeHandle().value) : nullptr);
    if (defaults.displayMode == Graphics::DisplayMode::Borderless)
    { defaults.resolutionWidth = monitor.desktop.first; defaults.resolutionHeight = monitor.desktop.second; }
    auto loaded = Graphics::LoadGraphicsSettingsFile(
        Platform::Filesystem::Join(Platform::Filesystem::ConfigDirectory(), "graphics.cfg"),
        defaults, error);
    if (!DisplayConfiguration::Normalize(loaded.settings, monitor, true)) return false;
    if (m_displayReady) return error.empty() && ApplyGraphicsSettings(loaded.settings);
    m_graphics.LoadGraphicsSettings(loaded.settings);
    if (!error.empty()) TraceError("Graphics Settings: %s", error.c_str());
    return error.empty();
}

bool CPythonSystem::SaveGraphicsSettings()
{
    if (!m_graphics.IsOwnerThread()) return false;
    std::string error;
    auto settings = GetGraphicsSettings();
    // Closing, autosaving or exiting must never persist an unconfirmed preview.
    if (m_displayQueued || m_displayPending) DisplayConfiguration::Copy(settings, m_displayPrevious);
    const bool saved = Graphics::SaveGraphicsSettingsFile(
        Platform::Filesystem::Join(Platform::Filesystem::ConfigDirectory(), "graphics.cfg"), settings, error);
    if (!saved) TraceError("Graphics Settings: %s", error.c_str());
    return saved;
}

void CPythonSystem::FlushGraphicsSettings()
{
    if (!m_graphics.IsOwnerThread()) return;
    if (m_displayReady)
    {
        auto& app = CPythonApplication::Instance();
        if (m_displayQueued)
        {
            app.RememberDisplayWindow();
            m_displayQueued = false;
            m_displayPending = true;
            m_displayDeadline = Platform::Time::MonotonicNanoseconds() + 15000000000ULL;
            if (!m_displayCancel && !app.ApplyDisplayConfiguration(GetGraphicsSettings())) m_displayCancel = true;
        }
        if (m_displayPending && (m_displayCancel || Platform::Time::MonotonicNanoseconds() >= m_displayDeadline))
        {
            auto previous = GetGraphicsSettings();
            DisplayConfiguration::Copy(previous, m_displayPrevious);
            const auto monitor = DisplayConfiguration::Query(static_cast<HWND>(app.GetNativeHandle().value));
            DisplayConfiguration::Normalize(previous, monitor, true);
            if (!app.ApplyDisplayConfiguration(previous, true))
            {
                previous.displayMode = Graphics::DisplayMode::Windowed;
                previous.resolutionWidth = monitor.safeWindow.first;
                previous.resolutionHeight = monitor.safeWindow.second;
                if (!app.ApplyDisplayConfiguration(previous))
                {
                    TraceError("Display rollback and safe window resize failed");
                    app.Exit();
                }
            }
            m_graphics.LoadGraphicsSettings(previous);
            m_displayPending = m_displayCancel = false;
            m_displayDeadline = 0;
        }
    }
    const auto event = m_graphics.ConsumeChanges();
    if (!event.fields) return;
    Renderer::ApplyGraphicsRuntimeConfig(event.runtime);
    if (event.Has(Graphics::ShadowsChanged)) CPythonBackground::Instance().RefreshShadowLevel();
    if (event.Has(Graphics::ViewDistanceChanged))
        CPythonBackground::Instance().SetViewDistanceSet(0, event.runtime.viewDistance);
}

void CPythonSystem::InitializeDisplaySettings()
{
    m_displayReady = true;
    auto settings = GetGraphicsSettings();
    const auto monitor = DisplayConfiguration::Query(static_cast<HWND>(CPythonApplication::Instance().GetNativeHandle().value));
    DisplayConfiguration::Normalize(settings, monitor, true);
    AdoptDisplaySettings(settings);
}

void CPythonSystem::AdoptDisplaySettings(const Graphics::GraphicsSettings& settings)
{
    auto current = GetGraphicsSettings();
    DisplayConfiguration::Copy(current, settings);
    m_graphics.LoadGraphicsSettings(current);
}

int CPythonSystem::DisplayConfirmationSeconds() const
{
    if (!m_displayPending || m_displayCancel) return 0;
    const auto now = Platform::Time::MonotonicNanoseconds();
    return now >= m_displayDeadline ? 0 : int((m_displayDeadline - now + 999999999ULL) / 1000000000ULL);
}

bool CPythonSystem::ConfirmDisplaySettings()
{
    if (!m_displayPending || !DisplayConfirmationSeconds()) return false;
    std::string error;
    if (!Graphics::SaveGraphicsSettingsFile(
        Platform::Filesystem::Join(Platform::Filesystem::ConfigDirectory(), "graphics.cfg"), GetGraphicsSettings(), error))
    {
        TraceError("Display confirmation save: %s", error.c_str());
        return false;
    }
    m_displayPending = false;
    m_displayDeadline = 0;
    return true;
}

void CPythonSystem::CancelDisplaySettings()
{
    if (m_displayQueued || m_displayPending) m_displayCancel = true;
}

#define DEFAULT_VALUE_ALWAYS_SHOW_NAME		true

void CPythonSystem::SetInterfaceHandler(PyObject * poHandler)
{
// NOTE : 레퍼런스 카운트는 바꾸지 않는다. 레퍼런스가 남아 있어 Python에서 완전히 지워지지 않기 때문.
//        대신에 __del__때 Destroy를 호출해 Handler를 NULL로 셋팅한다. - [levites]
//	if (m_poInterfaceHandler)
//		Py_DECREF(m_poInterfaceHandler);

	m_poInterfaceHandler = poHandler;

//	if (m_poInterfaceHandler)
//		Py_INCREF(m_poInterfaceHandler);
}

void CPythonSystem::DestroyInterfaceHandler()
{
	m_poInterfaceHandler = NULL;
}

void CPythonSystem::SaveWindowStatus(int iIndex, int iVisible, int iMinimized, int ix, int iy, int iHeight)
{
	m_WindowStatus[iIndex].isVisible	= iVisible;
	m_WindowStatus[iIndex].isMinimized	= iMinimized;
	m_WindowStatus[iIndex].ixPosition	= ix;
	m_WindowStatus[iIndex].iyPosition	= iy;
	m_WindowStatus[iIndex].iHeight		= iHeight;
}

void CPythonSystem::GetDisplaySettings()
{
	memset(m_ResolutionList, 0, sizeof(TResolution) * RESOLUTION_MAX_NUM);
	m_ResolutionCount = 0;

    // ZiiNAN: Display modes are OS data, not a reason to create a D3D9 device.
    DEVMODEW mode{}; mode.dmSize=sizeof(mode);
    for(DWORD index=0; EnumDisplaySettingsW(nullptr,index,&mode); ++index)
    {
        if(mode.dmPelsWidth<800 || mode.dmPelsHeight<600 || mode.dmBitsPerPel!=32) continue;
        struct { DWORD Width,Height,RefreshRate; } DisplayMode{
            mode.dmPelsWidth,mode.dmPelsHeight,mode.dmDisplayFrequency};
        const DWORD bpp=32;
		int check_res = false;

		for (int i = 0; !check_res && i < m_ResolutionCount; ++i)
		{
			if (m_ResolutionList[i].bpp != bpp ||
				m_ResolutionList[i].width != DisplayMode.Width ||
				m_ResolutionList[i].height != DisplayMode.Height)
				continue;

			int check_fre = false;

			// 프리퀀시만 다르므로 프리퀀시만 셋팅해준다.
			for (int j = 0; j < m_ResolutionList[i].frequency_count; ++j)
			{
				if (m_ResolutionList[i].frequency[j] == DisplayMode.RefreshRate)
				{
					check_fre = true;
					break;
				}
			}

			if (!check_fre)
				if (m_ResolutionList[i].frequency_count < FREQUENCY_MAX_NUM)
					m_ResolutionList[i].frequency[m_ResolutionList[i].frequency_count++] = DisplayMode.RefreshRate;

			check_res = true;
		}

		if (!check_res)
		{
			// 새로운 거니까 추가해주자.
			if (m_ResolutionCount < RESOLUTION_MAX_NUM)
			{
				m_ResolutionList[m_ResolutionCount].width			= DisplayMode.Width;
				m_ResolutionList[m_ResolutionCount].height			= DisplayMode.Height;
				m_ResolutionList[m_ResolutionCount].bpp				= bpp;
				m_ResolutionList[m_ResolutionCount].frequency[0]	= DisplayMode.RefreshRate;
				m_ResolutionList[m_ResolutionCount].frequency_count	= 1;

				++m_ResolutionCount;
			}
		}
	}
}

int	CPythonSystem::GetResolutionCount()
{
	return m_ResolutionCount;
}

int CPythonSystem::GetFrequencyCount(int index)
{
	if (index >= m_ResolutionCount)
		return 0;

    return m_ResolutionList[index].frequency_count;
}

bool CPythonSystem::GetResolution(int index, OUT DWORD *width, OUT DWORD *height, OUT DWORD *bpp)
{
	if (index >= m_ResolutionCount)
		return false;

	*width = m_ResolutionList[index].width;
	*height = m_ResolutionList[index].height;
	*bpp = m_ResolutionList[index].bpp;
	return true;
}

bool CPythonSystem::GetFrequency(int index, int freq_index, OUT DWORD *frequncy)
{
	if (index >= m_ResolutionCount)
		return false;

	if (freq_index >= m_ResolutionList[index].frequency_count)
		return false;

	*frequncy = m_ResolutionList[index].frequency[freq_index];
	return true;
}

int	CPythonSystem::GetResolutionIndex(DWORD width, DWORD height, DWORD bit)
{
	DWORD re_width, re_height, re_bit;
	int i = 0;

	while (GetResolution(i, &re_width, &re_height, &re_bit))
	{
		if (re_width == width)
			if (re_height == height)
				if (re_bit == bit)
					return i;
		i++;
	}

	return 0;
}

int	CPythonSystem::GetFrequencyIndex(int res_index, DWORD frequency)
{
	DWORD re_frequency;
	int i = 0;

	while (GetFrequency(res_index, i, &re_frequency))
	{
		if (re_frequency == frequency)
			return i;

		i++;
	}

	return 0;
}

DWORD CPythonSystem::GetWidth()
{
	return GetGraphicsSettings().resolutionWidth;
}

DWORD CPythonSystem::GetHeight()
{
	return GetGraphicsSettings().resolutionHeight;
}
DWORD CPythonSystem::GetBPP()
{
	return m_Config.bpp;
}
DWORD CPythonSystem::GetFrequency()
{
	return m_Config.frequency;
}

bool CPythonSystem::IsNoSoundCard()
{
	return m_Config.bNoSoundCard;
}

bool CPythonSystem::IsSoftwareCursor()
{
	return m_Config.is_software_cursor;
}

float CPythonSystem::GetMusicVolume()
{
	return m_Config.music_volume;
}

float CPythonSystem::GetSoundVolume()
{
	return m_Config.voice_volume;
}

void CPythonSystem::SetMusicVolume(float fVolume)
{
	m_Config.music_volume = fVolume;
}

void CPythonSystem::SetSoundVolume(float fVolume)
{
	m_Config.voice_volume = fVolume;
}

int CPythonSystem::GetDistance()
{
	return m_Config.iDistance;
}

int CPythonSystem::GetShadowLevel()
{
	return int(GetGraphicsSettings().shadows);
}

void CPythonSystem::SetShadowLevel(unsigned int level)
{
    auto settings = GetGraphicsSettings();
    settings.shadows = static_cast<Graphics::ShadowQuality>(std::min(level, 5u));
    ApplyGraphicsSettings(settings);
}

// MR-14: Fog update by Alaric
int CPythonSystem::GetFogLevel()
{
	return GetGraphicsSettings().fogLevel;
}

void CPythonSystem::SetFogLevel(unsigned int level)
{
    auto settings = GetGraphicsSettings();
    settings.fogLevel = int(std::min(level, 2u));
    ApplyGraphicsSettings(settings);
}
// MR-14: -- END OF -- Fog update by Alaric

int CPythonSystem::IsSaveID()
{
	return m_Config.isSaveID;
}

const char * CPythonSystem::GetSaveID()
{
	return m_Config.SaveID;
}

bool CPythonSystem::isViewCulling()
{
	return m_Config.is_object_culling;
}

void CPythonSystem::SetSaveID(int iValue, const char * c_szSaveID)
{
	if (iValue != 1)
		return;
	
	m_Config.isSaveID = iValue;
	strncpy(m_Config.SaveID, c_szSaveID, sizeof(m_Config.SaveID) - 1);
}

CPythonSystem::TConfig * CPythonSystem::GetConfig()
{
    // Compatibility mirror for the old Python config tuple; never a renderer source.
    m_Config.iShadowLevel = GetShadowLevel();
    m_Config.iFogLevel = GetFogLevel();
	return &m_Config;
}

void CPythonSystem::SetConfig(TConfig * pNewConfig)
{
	m_Config = *pNewConfig;
    auto settings = GetGraphicsSettings();
    settings.shadows = static_cast<Graphics::ShadowQuality>(m_Config.iShadowLevel);
    settings.fogLevel = m_Config.iFogLevel;
    ApplyGraphicsSettings(settings);
}

void CPythonSystem::SetDefaultConfig()
{
	memset(&m_Config, 0, sizeof(m_Config));

	m_Config.width				= 1024;
	m_Config.height				= 768;
	m_Config.bpp				= 32;

	m_Config.bWindowed			= false;

	m_Config.is_software_cursor	= false;
	m_Config.is_object_culling	= true;
	m_Config.iDistance			= 3;

	m_Config.gamma				= 3;
	m_Config.music_volume		= 1.0f;
	m_Config.voice_volume		= 1.0f;

	m_Config.bDecompressDDS		= 0;
	m_Config.bSoftwareTiling	= 0;
	m_Config.iShadowLevel		= 3;
	// MR-14: Fog update by Alaric
	m_Config.iFogLevel			= 0;
	// MR-14: -- END OF -- Fog update by Alaric
	m_Config.bViewChat			= true;
	m_Config.bAlwaysShowName	= DEFAULT_VALUE_ALWAYS_SHOW_NAME;
	m_Config.bShowDamage		= true;
	m_Config.bShowSalesText		= true;
    m_OldConfig = m_Config;
}

bool CPythonSystem::IsWindowed()
{
	return GetGraphicsSettings().displayMode == Graphics::DisplayMode::Windowed;
}

bool CPythonSystem::IsViewChat()
{
	return m_Config.bViewChat;
}

void CPythonSystem::SetViewChatFlag(int iFlag)
{
	m_Config.bViewChat = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsAlwaysShowName()
{
	return m_Config.bAlwaysShowName;
}

void CPythonSystem::SetAlwaysShowNameFlag(int iFlag)
{
	m_Config.bAlwaysShowName = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsShowDamage()
{
	return m_Config.bShowDamage;
}

void CPythonSystem::SetShowDamageFlag(int iFlag)
{
	m_Config.bShowDamage = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsShowSalesText()
{
	return m_Config.bShowSalesText;
}

void CPythonSystem::SetShowSalesTextFlag(int iFlag)
{
	m_Config.bShowSalesText = iFlag == 1 ? true : false;
}

bool CPythonSystem::IsAutoTiling()
{
	if (m_Config.bSoftwareTiling == 0)
		return true;

	return false;
}

void CPythonSystem::SetSoftwareTiling(bool isEnable)
{
	if (isEnable)
		m_Config.bSoftwareTiling=1;
	else
		m_Config.bSoftwareTiling=2;
}

bool CPythonSystem::IsSoftwareTiling()
{
	if (m_Config.bSoftwareTiling==1)
		return true;

	return false;
}

bool CPythonSystem::IsUseDefaultIME()
{
	return m_Config.bUseDefaultIME;
}

bool CPythonSystem::LoadConfig()
{
	FILE * fp = NULL;

	if (NULL == (fp = Platform::Filesystem::OpenCFile(Platform::Filesystem::Join(Platform::Filesystem::ConfigDirectory(), "metin2.cfg").c_str(), "rt")))
		return false;

	char buf[256];
	char command[256];
	char value[256];

	while (fgets(buf, 256, fp))
	{
		if (sscanf(buf, " %255s %255s", command, value) != 2)
			continue;

		if (!stricmp(command, "WIDTH"))
			m_Config.width		= atoi(value);
		else if (!stricmp(command, "HEIGHT"))
			m_Config.height	= atoi(value);
		else if (!stricmp(command, "BPP"))
			m_Config.bpp		= atoi(value);
		else if (!stricmp(command, "FREQUENCY"))
			m_Config.frequency = atoi(value);
		else if (!stricmp(command, "SOFTWARE_CURSOR"))
			m_Config.is_software_cursor = atoi(value) ? true : false;
		else if (!stricmp(command, "OBJECT_CULLING"))
			m_Config.is_object_culling = atoi(value) ? true : false;
		else if (!stricmp(command, "VISIBILITY"))
			m_Config.iDistance = atoi(value);
		else if (!stricmp(command, "MUSIC_VOLUME"))
			m_Config.music_volume = atof(value);
		else if (!stricmp(command, "VOICE_VOLUME"))
			m_Config.voice_volume = atof(value);
		else if (!stricmp(command, "GAMMA"))
			m_Config.gamma = atoi(value);
		else if (!stricmp(command, "IS_SAVE_ID"))
			m_Config.isSaveID = atoi(value);
		else if (!stricmp(command, "SAVE_ID"))
        {
            strncpy(m_Config.SaveID, value, sizeof(m_Config.SaveID) - 1);
            m_Config.SaveID[sizeof(m_Config.SaveID) - 1] = '\0';
        }
		else if (!stricmp(command, "PRE_LOADING_DELAY_TIME"))
			g_iLoadingDelayTime = atoi(value);
		else if (!stricmp(command, "WINDOWED"))
		{
			m_Config.bWindowed = atoi(value) == 1 ? true : false;
		}
		else if (!stricmp(command, "USE_DEFAULT_IME"))
			m_Config.bUseDefaultIME = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SOFTWARE_TILING"))
			m_Config.bSoftwareTiling = atoi(value);
		else if (!stricmp(command, "SHADOW_LEVEL"))
			m_Config.iShadowLevel = atoi(value);
		// MR-14: Fog update by Alaric
		else if (!stricmp(command, "FOG_LEVEL"))
			m_Config.iFogLevel = atoi(value);
		// MR-14: -- END OF -- Fog update by Alaric
		else if (!stricmp(command, "DECOMPRESSED_TEXTURE"))
			m_Config.bDecompressDDS = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "NO_SOUND_CARD"))
			m_Config.bNoSoundCard = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "VIEW_CHAT"))
			m_Config.bViewChat = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "ALWAYS_VIEW_NAME"))
			m_Config.bAlwaysShowName = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SHOW_DAMAGE"))
			m_Config.bShowDamage = atoi(value) == 1 ? true : false;
		else if (!stricmp(command, "SHOW_SALESTEXT"))
			m_Config.bShowSalesText = atoi(value) == 1 ? true : false;
	}

	if (m_Config.bWindowed)
	{
		unsigned screen_width = GetSystemMetrics(SM_CXFULLSCREEN);
		unsigned screen_height = GetSystemMetrics(SM_CYFULLSCREEN);

		if (m_Config.width >= screen_width)
		{
			m_Config.width = screen_width;
		}
		// if (m_Config.height >= screen_height)
		// {
			// m_Config.height = screen_height;
		// }
		if (m_Config.height >= screen_height) //Fix
		{
			int config_height = m_Config.height;
			int difference = (config_height-screen_height)+7;
			m_Config.height = config_height - difference;
		}
	}
	
	m_OldConfig = m_Config;

	fclose(fp);

//	Tracef("LoadConfig: Resolution: %dx%d %dBPP %dHZ Software Cursor: %d, Music/Voice Volume: %d/%d Gamma: %d\n",
//		m_Config.width,
//		m_Config.height,
//		m_Config.bpp,
//		m_Config.frequency,
//		m_Config.is_software_cursor,
//		m_Config.music_volume,
//		m_Config.voice_volume,
//		m_Config.gamma);

	return true;
}

bool CPythonSystem::SaveConfig()
{
    const bool graphicsSaved = SaveGraphicsSettings();
    m_Config.iShadowLevel = GetShadowLevel();
    m_Config.iFogLevel = GetFogLevel();
	FILE *fp;

	if (NULL == (fp = Platform::Filesystem::OpenCFile(Platform::Filesystem::Join(Platform::Filesystem::ConfigDirectory(), "metin2.cfg").c_str(), "wt")))
		return false;

	fprintf(fp, "WIDTH						%d\n"
				"HEIGHT						%d\n"
				"BPP						%d\n"
				"FREQUENCY					%d\n"
				"SOFTWARE_CURSOR			%d\n"
				"OBJECT_CULLING				%d\n"
				"VISIBILITY					%d\n"
				"MUSIC_VOLUME				%.3f\n"
				"VOICE_VOLUME				%.3f\n"
				"GAMMA						%d\n"
				"IS_SAVE_ID					%d\n"
				"SAVE_ID					%s\n"
				"PRE_LOADING_DELAY_TIME		%d\n"
				"DECOMPRESSED_TEXTURE		%d\n",
				m_Config.width,
				m_Config.height,
				m_Config.bpp,
				m_Config.frequency,
				m_Config.is_software_cursor,
				m_Config.is_object_culling,
				m_Config.iDistance,
				m_Config.music_volume,
				m_Config.voice_volume,
				m_Config.gamma,
				m_Config.isSaveID,
				m_Config.SaveID,
				g_iLoadingDelayTime,
				m_Config.bDecompressDDS);

	if (m_Config.bWindowed == 1)
		fprintf(fp, "WINDOWED			%d\n", m_Config.bWindowed);
	if (m_Config.bViewChat == 0)
		fprintf(fp, "VIEW_CHAT			%d\n", m_Config.bViewChat);
	if (m_Config.bAlwaysShowName != DEFAULT_VALUE_ALWAYS_SHOW_NAME)
		fprintf(fp, "ALWAYS_VIEW_NAME	%d\n", m_Config.bAlwaysShowName);
	if (m_Config.bShowDamage == 0)
		fprintf(fp, "SHOW_DAMAGE		%d\n", m_Config.bShowDamage);
	if (m_Config.bShowSalesText == 0)
		fprintf(fp, "SHOW_SALESTEXT		%d\n", m_Config.bShowSalesText);

	fprintf(fp, "USE_DEFAULT_IME		%d\n", m_Config.bUseDefaultIME);
	fprintf(fp, "SOFTWARE_TILING		%d\n", m_Config.bSoftwareTiling);
	fprintf(fp, "SHADOW_LEVEL			%d\n", m_Config.iShadowLevel);
	// MR-14: Fog update by Alaric
	fprintf(fp, "FOG_LEVEL				%d\n", m_Config.iFogLevel);
	// MR-14: -- END OF -- Fog update by Alaric
	fprintf(fp, "\n");

	fclose(fp);
	return graphicsSaved;
}

bool CPythonSystem::LoadInterfaceStatus()
{
	FILE * File;
	File = Platform::Filesystem::OpenCFile(Platform::Filesystem::Join(Platform::Filesystem::ConfigDirectory(), "interface.cfg").c_str(), "rb");

	if (!File)
		return false;

	fread(m_WindowStatus, 1, sizeof(TWindowStatus) * WINDOW_MAX_NUM, File);
	fclose(File);
	return true;
}

void CPythonSystem::SaveInterfaceStatus()
{
	if (!m_poInterfaceHandler)
		return;

	PyCallClassMemberFunc(m_poInterfaceHandler, "OnSaveInterfaceStatus", Py_BuildValue("()"));

	FILE * File;

	File = Platform::Filesystem::OpenCFile(Platform::Filesystem::Join(Platform::Filesystem::ConfigDirectory(), "interface.cfg").c_str(), "wb");

	if (!File)
	{
		TraceError("Cannot open interface.cfg");
		return;
	}

	fwrite(m_WindowStatus, 1, sizeof(TWindowStatus) * WINDOW_MAX_NUM, File);
	fclose(File);
}

bool CPythonSystem::isInterfaceConfig()
{
	return m_isInterfaceConfig;
}

const CPythonSystem::TWindowStatus & CPythonSystem::GetWindowStatusReference(int iIndex)
{
	return m_WindowStatus[iIndex];
}

void CPythonSystem::ApplyConfig() // 이전 설정과 현재 설정을 비교해서 바뀐 설정을 적용 한다.
{
	if (m_OldConfig.gamma != m_Config.gamma)
	{
		float val = 1.0f;
		
		switch (m_Config.gamma)
		{
			case 0: val = 0.4f;	break;
			case 1: val = 0.7f; break;
			case 2: val = 1.0f; break;
			case 3: val = 1.2f; break;
			case 4: val = 1.4f; break;
		}
		
		CPythonGraphic::Instance().SetGamma(val);
	}

	if (m_OldConfig.is_software_cursor != m_Config.is_software_cursor)
	{
		if (m_Config.is_software_cursor)
			CPythonApplication::Instance().SetCursorMode(CPythonApplication::CURSOR_MODE_SOFTWARE);
		else
			CPythonApplication::Instance().SetCursorMode(CPythonApplication::CURSOR_MODE_HARDWARE);
	}

	m_OldConfig = m_Config;

	ChangeSystem();
}

void CPythonSystem::ChangeSystem()
{
	// Shadow
	/*
	if (m_Config.is_shadow)
		CScreen::SetShadowFlag(true);
	else
		CScreen::SetShadowFlag(false);
	*/
	SoundEngine& rkSndMgr = SoundEngine::Instance();
	/*
	float fMusicVolume;
	if (0 == m_Config.music_volume)
		fMusicVolume = 0.0f;
	else
		fMusicVolume= (float)pow(10.0f, (-1.0f + (float)m_Config.music_volume / 5.0f));
		*/
	rkSndMgr.SetMusicVolume(m_Config.music_volume);

	/*
	float fVoiceVolume;
	if (0 == m_Config.voice_volume)
		fVoiceVolume = 0.0f;
	else
		fVoiceVolume = (float)pow(10.0f, (-1.0f + (float)m_Config.voice_volume / 5.0f));
	*/
	rkSndMgr.SetSoundVolume(m_Config.voice_volume);
}

void CPythonSystem::Clear()
{
	SetInterfaceHandler(NULL);
}

CPythonSystem::CPythonSystem()
{
	memset(&m_Config, 0, sizeof(TConfig));

	m_poInterfaceHandler = NULL;

	SetDefaultConfig();

	LoadConfig();
	LoadGraphicsSettings();

	ChangeSystem();

	if (LoadInterfaceStatus())
		m_isInterfaceConfig = true;
	else
		m_isInterfaceConfig = false;
}

CPythonSystem::~CPythonSystem()
{
	assert(m_poInterfaceHandler==NULL && "CPythonSystem MUST CLEAR!");
}
