add_library(M2RendererInterface INTERFACE)
target_include_directories(M2RendererInterface INTERFACE "${PROJECT_SOURCE_DIR}/src")

# ZiiNAN: Cross-platform bootstrap
if(M2_BUILD_WINDOWS_CLIENT)
    add_library(M2RendererBootstrap STATIC RendererBootstrap.cpp TerrainPresentation.cpp)
    target_link_libraries(M2RendererBootstrap PRIVATE M2RendererInterface M2FirstPartyArchitectureChecks M2Platform)
    include(Diligent)
    if(NOT TARGET Diligent-GraphicsEngineD3D11-static)
        message(FATAL_ERROR "Diligent D3D11 needs the Windows SDK and Visual C++ ATL components")
    endif()
    add_library(M2RendererDiligent STATIC DiligentD3D11Backend.cpp DiligentTerrainRenderer.cpp DiligentStaticObjectRenderer.cpp)
    # ZiiNAN: CPU-skinned actor submission reuses the existing D3D11 world renderer.
    target_sources(M2RendererDiligent PRIVATE DiligentActorRenderer.cpp)
    # ZiiNAN: Existing SpeedTree geometry/materials, no new vegetation runtime.
    target_sources(M2RendererDiligent PRIVATE DiligentTreeRenderer.cpp)
    # ZiiNAN: Existing CPU effects, no simulation or other backend changes.
    target_sources(M2RendererDiligent PRIVATE DiligentEffectRenderer.cpp)
    target_include_directories(M2RendererDiligent PRIVATE "${diligentcore_SOURCE_DIR}")
    target_link_libraries(M2RendererDiligent PUBLIC M2RendererInterface PRIVATE Diligent-GraphicsEngineD3D11-static M2FirstPartyArchitectureChecks)
    target_link_libraries(M2RendererBootstrap PRIVATE M2RendererDiligent)
elseif(M2_BUILD_ANDROID_BOOTSTRAP)
    include(Diligent)
    if(NOT TARGET Diligent-GraphicsEngineVk-static)
        message(FATAL_ERROR "The pinned Diligent Vulkan backend and its submodules are required for Android.")
    endif()
    add_library(M2RendererVulkanBootstrap STATIC Vulkan/VulkanBootstrap.cpp)
    option(M2_ANDROID_VULKAN_VALIDATION "Request installed Android Vulkan validation layers" OFF)
    target_compile_definitions(M2RendererVulkanBootstrap PRIVATE M2_ANDROID_VULKAN_VALIDATION=$<BOOL:${M2_ANDROID_VULKAN_VALIDATION}>)
    target_include_directories(M2RendererVulkanBootstrap PRIVATE "${diligentcore_SOURCE_DIR}")
    target_link_libraries(M2RendererVulkanBootstrap PUBLIC M2RendererInterface M2PlatformCommon
        PRIVATE Diligent-GraphicsEngineVk-static log)
    set_target_properties(M2RendererVulkanBootstrap PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "Renderer/Vulkan")
endif()
