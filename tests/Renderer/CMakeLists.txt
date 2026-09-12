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

# ZiiNAN: Read-only original SpeedTree SDK probe; real asset paths are explicit arguments.
add_executable(SpeedTreeAssetProbe SpeedTreeAssetProbe.cpp)
target_link_libraries(SpeedTreeAssetProbe PRIVATE SpeedTree user32)

add_executable(RendererSmokeTest RendererSmokeTest.cpp)
get_target_property(client_libraries UserInterface LINK_LIBRARIES)
add_executable(TerrainTextureCpuTest TerrainTextureCpuTest.cpp)
target_link_libraries(TerrainTextureCpuTest PRIVATE ${client_libraries})
add_test(NAME Renderer.TerrainTextureCpu COMMAND TerrainTextureCpuTest)
target_link_libraries(RendererSmokeTest PRIVATE ${client_libraries})
if(M2_ENABLE_DILIGENT_D3D11)
    target_compile_definitions(RendererSmokeTest PRIVATE M2_ENABLE_DILIGENT_D3D11=1)
    get_target_property(diligent_includes M2RendererDiligent INCLUDE_DIRECTORIES)
    target_include_directories(RendererSmokeTest PRIVATE ${diligent_includes})
    target_link_libraries(RendererSmokeTest PRIVATE M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
    add_test(NAME Renderer.DiligentD3D11 COMMAND RendererSmokeTest diligent-d3d11)
    set_tests_properties(Renderer.DiligentD3D11 PROPERTIES LABELS "gpu" TIMEOUT 45)
    add_executable(TerrainGpuTest TerrainGpuTest.cpp)
    target_include_directories(TerrainGpuTest PRIVATE ${diligent_includes})
    target_link_libraries(TerrainGpuTest PRIVATE ${client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface)
    add_test(NAME Renderer.TerrainGpuParity COMMAND TerrainGpuTest)
    set_tests_properties(Renderer.TerrainGpuParity PROPERTIES LABELS "gpu" TIMEOUT 60)
endif()
add_test(NAME Renderer.LegacyD3D9 COMMAND RendererSmokeTest legacy-d3d9)
set_tests_properties(Renderer.LegacyD3D9 PROPERTIES LABELS "gpu" TIMEOUT 45)
