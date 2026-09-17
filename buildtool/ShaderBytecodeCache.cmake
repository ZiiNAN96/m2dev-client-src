# One production hook at the shared D3D11/FXC boundary, including internal FX calls.
execute_process(COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/buildtool/PrepareShaderBytecodeCache.py"
    "${m2_shader_audit_dir}" RESULT_VARIABLE m2_shader_cache_result)
if(NOT m2_shader_cache_result EQUAL 0)
    message(FATAL_ERROR "Could not prepare pinned shader bytecode cache integration")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/buildtool/PrepareShaderBytecodeCache.py")
# Fingerprint actual pinned compiler/hasher implementation, not only a human tag.
set(m2_shader_cache_identity "DiligentCore-b036337d68be2353c9950a85929acf796b9a6d50")
foreach(input Graphics/GraphicsEngineD3DBase/src/ShaderD3DBase.cpp
              Graphics/ShaderTools/src/HLSLUtils.cpp
              Graphics/GraphicsTools/src/XXH128Hasher.cpp
              Common/interface/HashUtils.hpp)
    file(SHA256 "${diligentcore_SOURCE_DIR}/${input}" hash)
    string(APPEND m2_shader_cache_identity "-${hash}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${diligentcore_SOURCE_DIR}/${input}")
endforeach()
file(GENERATE OUTPUT "${m2_shader_audit_dir}/ShaderCacheBuildIdentity.h"
    CONTENT "#pragma once\n#define M2_SHADER_CACHE_CORE_ID \"${m2_shader_cache_identity}\"\n")
target_sources(Diligent-GraphicsEngineD3DBase PRIVATE "${PROJECT_SOURCE_DIR}/src/Renderer/ShaderBytecodeCache.cpp")
target_include_directories(Diligent-GraphicsEngineD3DBase PRIVATE "${m2_shader_audit_dir}")
target_link_libraries(Diligent-GraphicsEngineD3DBase PRIVATE d3dcompiler shell32)
