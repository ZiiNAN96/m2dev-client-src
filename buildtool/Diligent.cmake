include(FetchContent)
include(ExternalDependency)
m2_use_external_dependency(DiligentCore DiligentCore)

# ZiiNAN: Cross-platform bootstrap
foreach(api DIRECT3D11 DIRECT3D12 OPENGL VULKAN METAL WEBGPU ARCHIVER)
    set(DILIGENT_NO_${api} ON CACHE BOOL "" FORCE)
endforeach()
set(m2_diligent_submodules ThirdParty/xxHash)
if(M2_BUILD_WINDOWS_CLIENT)
    set(DILIGENT_NO_DIRECT3D11 OFF CACHE BOOL "" FORCE)
    # Keep the production shader capabilities and CPU requirements unchanged.
    set(DILIGENT_MSVC_RELEASE_COMPILE_OPTIONS "/GL" CACHE STRING "" FORCE)
elseif(M2_BUILD_ANDROID_BOOTSTRAP)
    set(DILIGENT_NO_VULKAN OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_HLSL ON CACHE BOOL "Clear-present does not compile shaders" FORCE)
    set(DILIGENT_NO_GLSLANG ON CACHE BOOL "Clear-present does not compile shaders" FORCE)
    list(APPEND m2_diligent_submodules ThirdParty/Vulkan-Headers ThirdParty/volk
        ThirdParty/SPIRV-Headers ThirdParty/SPIRV-Cross)
endif()
set(DILIGENT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DILIGENT_BUILD_GOOGLE_TEST OFF CACHE BOOL "" FORCE)
set(DILIGENT_INSTALL_CORE OFF CACHE BOOL "" FORCE)
if(CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
FetchContent_Declare(DiligentCore
    BINARY_DIR "${CMAKE_BINARY_DIR}/deps/DiligentCore"
    GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentCore.git
    GIT_TAG b036337d68be2353c9950a85929acf796b9a6d50 # v2.5.6, unchanged from C2-X
    GIT_SUBMODULES ${m2_diligent_submodules}
    GIT_SUBMODULES_RECURSE FALSE
)
FetchContent_MakeAvailable(DiligentCore)
if(M2_BUILD_WINDOWS_CLIENT)
    include(ShaderLoadAudit)
    include(ShaderBytecodeCache)
endif()
if(TARGET Diligent-Win32Platform AND MSVC)
    target_compile_options(Diligent-Win32Platform PRIVATE /UWIN32_LEAN_AND_MEAN)
endif()
