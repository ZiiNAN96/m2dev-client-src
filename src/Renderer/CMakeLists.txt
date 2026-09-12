add_library(M2RendererInterface INTERFACE)
target_include_directories(M2RendererInterface INTERFACE "${PROJECT_SOURCE_DIR}/src")

add_library(M2RendererBootstrap STATIC RendererBootstrap.cpp TerrainPresentation.cpp)
target_link_libraries(M2RendererBootstrap PRIVATE M2RendererInterface)

if(M2_ENABLE_DILIGENT_D3D11)
    # Keep the existing default build independent of downloads and modern graphics APIs.
    include(FetchContent)
    set(DILIGENT_NO_DIRECT3D11 OFF CACHE BOOL "" FORCE)
    foreach(api DIRECT3D12 OPENGL VULKAN METAL WEBGPU ARCHIVER)
        set(DILIGENT_NO_${api} ON CACHE BOOL "" FORCE)
    endforeach()
    set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_BUILD_GOOGLE_TEST OFF CACHE BOOL "" FORCE)
    set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
    # Do not raise the client's CPU requirement to Diligent's optional AVX2 default.
    set(DILIGENT_MSVC_RELEASE_COMPILE_OPTIONS "/GL" CACHE STRING "" FORCE)
    # Diligent otherwise writes Debug into the shared cache for multi-config builds.
    if(CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
        set(CMAKE_BUILD_TYPE Release)
    endif()
    # CMAKE_MSVC_RUNTIME_LIBRARY is inherited from the client (/MT, /MTd).
    FetchContent_Declare(DiligentCore
        GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentCore.git
        GIT_TAG b036337d68be2353c9950a85929acf796b9a6d50 # v2.5.6
        GIT_SUBMODULES ThirdParty/xxHash
        GIT_SUBMODULES_RECURSE FALSE
    )
    FetchContent_MakeAvailable(DiligentCore)
    # Upstream Win32FileSystem uses common dialogs through windows.h. The client's
    # inherited LEAN_AND_MEAN define hides them; override it only for this dependency.
    if(TARGET Diligent-Win32Platform)
        target_compile_options(Diligent-Win32Platform PRIVATE /UWIN32_LEAN_AND_MEAN)
    endif()
    if(NOT TARGET Diligent-GraphicsEngineD3D11-static)
        message(FATAL_ERROR "Diligent D3D11 needs the Windows SDK and Visual C++ ATL components")
    endif()
    add_library(M2RendererDiligent STATIC DiligentD3D11Backend.cpp DiligentTerrainRenderer.cpp DiligentStaticObjectRenderer.cpp)
    # ZiiNAN: CPU-skinned actor submission reuses the existing D3D11 world renderer.
    target_sources(M2RendererDiligent PRIVATE DiligentActorRenderer.cpp)
    # ZiiNAN: Existing SpeedTree geometry/materials, no new vegetation runtime.
    target_sources(M2RendererDiligent PRIVATE DiligentTreeRenderer.cpp)
    target_include_directories(M2RendererDiligent PRIVATE "${diligentcore_SOURCE_DIR}")
    target_link_libraries(M2RendererDiligent PUBLIC M2RendererInterface PRIVATE Diligent-GraphicsEngineD3D11-static)
    target_compile_definitions(M2RendererBootstrap PRIVATE M2_ENABLE_DILIGENT_D3D11=1)
    target_link_libraries(M2RendererBootstrap PRIVATE M2RendererDiligent)
endif()
