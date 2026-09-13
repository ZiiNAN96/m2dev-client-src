add_executable(RendererStartupOptionsTest StartupOptionsTest.cpp)
target_link_libraries(RendererStartupOptionsTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.StartupOptions COMMAND RendererStartupOptionsTest)
add_executable(RendererMathTest MathTest.cpp)
target_link_libraries(RendererMathTest PRIVATE M2RendererInterface)
add_test(NAME Renderer.Math COMMAND RendererMathTest)

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
add_executable(RendererDrawStateTest DrawStateTest.cpp)
target_link_libraries(RendererDrawStateTest PRIVATE ${client_libraries})
add_test(NAME Renderer.CpuDrawState COMMAND RendererDrawStateTest)
add_executable(RendererScreenshotJPEGTest ScreenshotJPEGTest.cpp)
target_link_libraries(RendererScreenshotJPEGTest PRIVATE EterImageLib M2RendererInterface)
add_test(NAME Renderer.ScreenshotJPEG COMMAND RendererScreenshotJPEGTest)
set_tests_properties(Renderer.CpuDrawState PROPERTIES TIMEOUT 45)
add_executable(TerrainTextureCpuTest TerrainTextureCpuTest.cpp)
target_link_libraries(TerrainTextureCpuTest PRIVATE ${client_libraries})
add_test(NAME Renderer.TerrainTextureCpu COMMAND TerrainTextureCpuTest)
target_link_libraries(RendererSmokeTest PRIVATE ${client_libraries})
    target_link_libraries(RendererResourceSourceTest PRIVATE M2RendererDiligent)
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


# ZiiNAN: Enforce the single-backend production boundary independently of GPU tests.
set(M2_SKINNING_ASSET_ROOT "${CMAKE_SOURCE_DIR}/../m2dev-client/assets" CACHE PATH "Real original GR2 fixtures for B2 validation")
add_executable(RendererSkinningDataTest SkinningDataTest.cpp
    ${CMAKE_SOURCE_DIR}/src/EterGrnLib/SkinningDataAdapter.cpp
    ${CMAKE_SOURCE_DIR}/src/EterGrnLib/Deform.cpp)
target_link_libraries(RendererSkinningDataTest PRIVATE M2RendererInterface Granny winmm)
add_test(NAME Renderer.SkinningData COMMAND RendererSkinningDataTest "${M2_SKINNING_ASSET_ROOT}")
set_tests_properties(Renderer.SkinningData PROPERTIES LABELS "skinning;real-assets" TIMEOUT 120)
add_executable(RendererSkinningInstanceTest SkinningInstanceTest.cpp)
target_link_libraries(RendererSkinningInstanceTest PRIVATE ${client_libraries} M2RendererDiligent)
add_test(NAME Renderer.SkinningInstance COMMAND RendererSkinningInstanceTest "${M2_SKINNING_ASSET_ROOT}")
set_tests_properties(Renderer.SkinningInstance PROPERTIES LABELS "skinning;real-assets" TIMEOUT 60)

add_test(NAME Renderer.NoLegacyArchitecture COMMAND ${CMAKE_COMMAND}
    -DSOURCE_ROOT=${CMAKE_SOURCE_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/NoLegacyArchitecture.cmake)

# ZiiNAN: Diligent GPU skinning prototype
add_executable(RendererSkinningPrototypeTest SkinningPrototypeTest.cpp)
target_include_directories(RendererSkinningPrototypeTest PRIVATE ${diligent_includes})
target_link_libraries(RendererSkinningPrototypeTest PRIVATE ${client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
add_test(NAME Renderer.SkinningPrototype COMMAND RendererSkinningPrototypeTest "${M2_SKINNING_ASSET_ROOT}" "${CMAKE_BINARY_DIR}/phase-b3/parity-$<CONFIG>")
set_tests_properties(Renderer.SkinningPrototype PROPERTIES LABELS "gpu;skinning;real-assets" TIMEOUT 120)

# ZiiNAN: GPU skinning actor coverage
add_executable(RendererSkinningCoverageTest SkinningCoverageTest.cpp)
target_include_directories(RendererSkinningCoverageTest PRIVATE ${diligent_includes})
target_link_libraries(RendererSkinningCoverageTest PRIVATE ${client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
add_test(NAME Renderer.SkinningCoverage COMMAND RendererSkinningCoverageTest "${M2_SKINNING_ASSET_ROOT}")
set_tests_properties(Renderer.SkinningCoverage PROPERTIES LABELS "gpu;skinning;real-assets" TIMEOUT 900)
add_test(NAME Renderer.HairLodParity COMMAND RendererSkinningCoverageTest "${M2_SKINNING_ASSET_ROOT}" --lod-only)
set_tests_properties(Renderer.HairLodParity PROPERTIES LABELS "gpu;skinning;real-assets" TIMEOUT 300)

# ZiiNAN: GPU skinning stability validation
add_executable(RendererSkinningStabilityTest SkinningStabilityTest.cpp)
target_include_directories(RendererSkinningStabilityTest PRIVATE ${diligent_includes})
target_link_libraries(RendererSkinningStabilityTest PRIVATE ${client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
foreach(mode parity stress)
    add_test(NAME Renderer.SkinningStability.${mode} COMMAND RendererSkinningStabilityTest
        "${M2_SKINNING_ASSET_ROOT}" "${CMAKE_BINARY_DIR}/phase-b5x/${mode}-$<CONFIG>" ${mode})
    set_tests_properties(Renderer.SkinningStability.${mode} PROPERTIES LABELS "gpu;skinning;real-assets;b5x" TIMEOUT 1800 RUN_SERIAL TRUE)
endforeach()

add_executable(RendererSkinningFallbackParityTest SkinningFallbackParityTest.cpp)
target_include_directories(RendererSkinningFallbackParityTest PRIVATE ${diligent_includes})
target_link_libraries(RendererSkinningFallbackParityTest PRIVATE ${client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
add_test(NAME Renderer.SkinningFallbackParity COMMAND RendererSkinningFallbackParityTest "${M2_SKINNING_ASSET_ROOT}")
set_tests_properties(Renderer.SkinningFallbackParity PROPERTIES LABELS "gpu;skinning;real-assets;b5x" TIMEOUT 120 RUN_SERIAL TRUE)

# ZiiNAN: GPU skinning production path — correctness smoke, full timing run is explicit.
add_executable(RendererSkinningBenchmarkTest SkinningBenchmarkTest.cpp)
target_include_directories(RendererSkinningBenchmarkTest PRIVATE ${diligent_includes})
target_link_libraries(RendererSkinningBenchmarkTest PRIVATE ${client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
add_test(NAME Renderer.SkinningBenchmark COMMAND RendererSkinningBenchmarkTest "${M2_SKINNING_ASSET_ROOT}" "${CMAKE_BINARY_DIR}/phase-b6x/smoke-$<CONFIG>.csv" --quick)
set_tests_properties(Renderer.SkinningBenchmark PROPERTIES LABELS "gpu;skinning;real-assets;b6x" TIMEOUT 120 RUN_SERIAL TRUE)
