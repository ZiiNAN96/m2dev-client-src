include(FetchContent)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

# Engine v2.5.6 release set, compatible with the existing Core pin.
# Populate only: upstream's aggregate target also links glTF/USD/Imgui.
FetchContent_Declare(M2DiligentFXSource
    URL https://codeload.github.com/DiligentGraphics/DiligentFX/tar.gz/cb380ac52100672b5762f595acfb6609e0ecc248
    URL_HASH SHA256=7117a1d0067ef0c36315900647904116234cf55cadc11682f6f6e70064658236
)
FetchContent_GetProperties(M2DiligentFXSource)
if(NOT m2diligentfxsource_POPULATED)
    FetchContent_Populate(M2DiligentFXSource)
endif()

set(m2_fx_dir "${CMAKE_CURRENT_BINARY_DIR}/fx-subset")
execute_process(COMMAND "${Python3_EXECUTABLE}"
    "${PROJECT_SOURCE_DIR}/buildtool/PrepareDiligentFX.py"
    "${m2diligentfxsource_SOURCE_DIR}" "${m2_fx_dir}"
    RESULT_VARIABLE m2_fx_prepare_result)
if(NOT m2_fx_prepare_result EQUAL 0)
    message(FATAL_ERROR "Could not prepare the verified DiligentFX subset")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/buildtool/PrepareDiligentFX.py")

add_library(M2DiligentFX STATIC
    "${m2_fx_dir}/Components/src/ShadowMapManager.cpp"
    "${m2_fx_dir}/PostProcess/Common/src/PostFXContext.cpp"
    "${m2_fx_dir}/PostProcess/Common/src/PostFXRenderTechnique.cpp"
    "${m2_fx_dir}/PostProcess/ScreenSpaceAmbientOcclusion/src/ScreenSpaceAmbientOcclusion.cpp"
    "${m2_fx_dir}/PostProcess/Bloom/src/Bloom.cpp"
    "${m2_fx_dir}/Utilities/src/DiligentFXShaderSourceStreamFactory.cpp"
)
target_include_directories(M2DiligentFX PUBLIC "${m2_fx_dir}"
    "${diligentcore_SOURCE_DIR}/Common/interface" PRIVATE
    "${diligentcore_SOURCE_DIR}"
    "${diligentcore_SOURCE_DIR}/Common/interface"
    "${diligentcore_SOURCE_DIR}/Graphics/GraphicsAccessories/interface"
    "${m2_fx_dir}/Components/interface"
    "${m2_fx_dir}/PostProcess/Common/interface"
    "${m2_fx_dir}/PostProcess/ScreenSpaceAmbientOcclusion/interface"
    "${m2_fx_dir}/PostProcess/Bloom/interface"
)
target_link_libraries(M2DiligentFX PUBLIC Diligent-GraphicsTools
    PRIVATE Diligent-BuildSettings)

# Use the upstream shader embedding utility: no runtime file paths or downloads.
include("${m2diligentfxsource_SOURCE_DIR}/BuildUtils.cmake")
file(STRINGS "${m2_fx_dir}/shader-manifest.txt" m2_fx_shaders)
set(m2_fx_shader_dir "${CMAKE_CURRENT_BINARY_DIR}/fx-shaders")
convert_shaders_to_headers("${m2_fx_shaders}" "${m2_fx_shader_dir}"
    "${m2_fx_shader_dir}/shaders_list.h" m2_fx_shader_headers)
target_sources(M2DiligentFX PRIVATE ${m2_fx_shader_headers})
target_include_directories(M2DiligentFX PRIVATE "${m2_fx_shader_dir}")
set_target_properties(M2DiligentFX PROPERTIES FOLDER "Renderer/DiligentFX")
