file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

# ZiiNAN: Platform abstraction
list(FILTER FILE_SOURCES EXCLUDE REGEX "/Windows/")
add_executable(UserInterface WIN32 ${FILE_SOURCES} ${CMAKE_SOURCE_DIR}/src/UserInterface/UserInterface.rc)
if(WIN32)
    target_sources(UserInterface PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Windows/Win32MoviePlayer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Windows/Win32ApplicationLogo.cpp"
    )
endif()
if(M2_BUILD_RENDERER_TESTS)
    # ZiiNAN: Local floating-text fixture entry points are absent from non-test builds.
    set_source_files_properties(PythonCharacterModule.cpp PythonBackgroundModule.cpp PythonSystemModule.cpp PythonApplication.cpp PROPERTIES COMPILE_DEFINITIONS M2_RENDERER_DIAGNOSTICS)
endif()
set_target_properties(UserInterface PROPERTIES LINK_FLAGS "/level='requireAdministrator' /uiAccess='false'"
	RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
	OUTPUT_NAME_DEBUG "Metin2_Debug"
	OUTPUT_NAME_RELEASE "Metin2_Release"
	OUTPUT_NAME_RELWITHDEBINFO "Metin2_RelWithDebInfo"
)

target_link_libraries(UserInterface 
	M2GraphicsSettings
	M2RendererBootstrap
    VegetationRuntime
	AudioLib
	Discord
	EffectLib 
	EterBase 
	EterGrnLib 
	EterImageLib 
	EterLib
	EterLocale
	EterPythonLib
	GameLib
	PRTerrainLib
	PythonModules
	ScriptLib
	SphereLib
	PackLib

	lzo2
	libzstd_static
	mio
	
	M2Platform
	M2PlatformWindowsFeatures
	Python
	
	ws2_32 
	strmiids
	amstrmid
	dmoguids
	ddraw
	version
	Dbghelp
	shell32
)

GroupSourcesByFolder(UserInterface)
