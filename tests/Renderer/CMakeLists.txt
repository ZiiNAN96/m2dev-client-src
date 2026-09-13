add_executable(RendererStartupOptionsTest StartupOptionsTest.cpp)
target_link_libraries(RendererStartupOptionsTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.StartupOptions COMMAND RendererStartupOptionsTest)

# ZiiNAN: Body-only category/scope safety does not depend on the Diligent build option.
add_executable(RendererActorPolicyTest ActorPolicyTest.cpp)
target_link_libraries(RendererActorPolicyTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.ActorPolicy COMMAND RendererActorPolicyTest)

# ZiiNAN: Tree integration boundaries remain testable without Diligent.
add_executable(RendererTreePolicyTest TreePolicyTest.cpp)
target_link_libraries(RendererTreePolicyTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.TreePolicy COMMAND RendererTreePolicyTest)

# CPU effect contracts and lifetime remain independent of GPU execution.
add_executable(RendererEffectPolicyTest EffectPolicyTest.cpp)
target_link_libraries(RendererEffectPolicyTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.EffectPolicy COMMAND RendererEffectPolicyTest)

# CPU water/world ownership and defaults.
add_executable(RendererWorldPolicyTest WorldPolicyTest.cpp)
target_link_libraries(RendererWorldPolicyTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.WorldPolicy COMMAND RendererWorldPolicyTest)

# ZiiNAN: UI contract remains testable without Diligent or Python UI startup.
add_executable(RendererUIPolicyTest UIPolicyTest.cpp)
target_link_libraries(RendererUIPolicyTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.UIPolicy COMMAND RendererUIPolicyTest)

add_executable(RendererTextPolicyTest TextPolicyTest.cpp)
target_link_libraries(RendererTextPolicyTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.TextPolicy COMMAND RendererTextPolicyTest)

# ZiiNAN: Read-only original SpeedTree SDK probe; real asset paths are explicit arguments.
add_executable(SpeedTreeAssetProbe SpeedTreeAssetProbe.cpp)
target_link_libraries(SpeedTreeAssetProbe PRIVATE SpeedTree user32)

add_executable(RendererSmokeTest RendererSmokeTest.cpp)
get_target_property(client_libraries UserInterface LINK_LIBRARIES)
add_executable(RendererResourceSourceTest ResourceSourceTest.cpp)
target_link_libraries(RendererResourceSourceTest PRIVATE ${client_libraries})
add_test(NAME Renderer.ResourceSource COMMAND RendererResourceSourceTest)
set_tests_properties(Renderer.ResourceSource PROPERTIES LABELS "gpu" TIMEOUT 45)
add_executable(RendererNativeStateViewTest NativeStateViewTest.cpp)
target_link_libraries(RendererNativeStateViewTest PRIVATE ${client_libraries})
add_test(NAME Renderer.CpuDrawState COMMAND RendererNativeStateViewTest)
add_executable(RendererScreenshotJPEGTest ScreenshotJPEGTest.cpp)
target_link_libraries(RendererScreenshotJPEGTest PRIVATE EterImageLib M2RendererInterface)
add_test(NAME Renderer.ScreenshotJPEG COMMAND RendererScreenshotJPEGTest)
set_tests_properties(Renderer.CpuDrawState PROPERTIES TIMEOUT 45)
add_executable(TerrainTextureCpuTest TerrainTextureCpuTest.cpp)
target_link_libraries(TerrainTextureCpuTest PRIVATE ${client_libraries})
add_test(NAME Renderer.TerrainTextureCpu COMMAND TerrainTextureCpuTest)
target_link_libraries(RendererSmokeTest PRIVATE ${client_libraries})
if(M2_ENABLE_DILIGENT_D3D11)
    target_compile_definitions(RendererResourceSourceTest PRIVATE M2_ENABLE_DILIGENT_D3D11=1)
    target_link_libraries(RendererResourceSourceTest PRIVATE M2RendererDiligent)
    target_compile_definitions(RendererSmokeTest PRIVATE M2_ENABLE_DILIGENT_D3D11=1)
    get_target_property(diligent_includes M2RendererDiligent INCLUDE_DIRECTORIES)
    target_include_directories(RendererSmokeTest PRIVATE ${diligent_includes})
    target_link_libraries(RendererSmokeTest PRIVATE M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
    add_test(NAME Renderer.DiligentD3D11 COMMAND RendererSmokeTest diligent-d3d11)
    set_tests_properties(Renderer.DiligentD3D11 PROPERTIES LABELS "gpu" TIMEOUT 45)
    add_library(FloatingTextTestSupport STATIC ${CMAKE_SOURCE_DIR}/src/UserInterface/PythonTextTail.cpp FloatingTextTestDependencies.cpp)
    target_compile_options(FloatingTextTestSupport PRIVATE /Gy)
    target_link_libraries(FloatingTextTestSupport PRIVATE ${client_libraries})
    add_executable(TerrainGpuTest DiligentGpuTest.cpp)
    target_compile_options(TerrainGpuTest PRIVATE /Gy)
    target_link_options(TerrainGpuTest PRIVATE /OPT:REF)
    target_include_directories(TerrainGpuTest PRIVATE ${diligent_includes})
    target_link_libraries(TerrainGpuTest PRIVATE FloatingTextTestSupport ${client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
    add_test(NAME Renderer.ProductionGpu COMMAND TerrainGpuTest)
    set_tests_properties(Renderer.ProductionGpu PROPERTIES LABELS "gpu" TIMEOUT 60)
endif()

# ZiiNAN: Enforce the single-backend production boundary independently of GPU tests.
add_test(NAME Renderer.NoLegacyArchitecture COMMAND ${CMAKE_COMMAND}
    -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/NoLegacyArchitecture.cmake)
