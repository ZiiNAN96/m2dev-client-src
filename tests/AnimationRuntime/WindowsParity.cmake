if(M2_BUILD_WINDOWS_CLIENT)
    get_target_property(animation_client_libraries UserInterface LINK_LIBRARIES)
    get_target_property(animation_diligent_includes M2RendererDiligent INCLUDE_DIRECTORIES)
    add_executable(AnimationRuntimeGrannyParityTest GrannyParityTest.cpp)
    target_include_directories(AnimationRuntimeGrannyParityTest PRIVATE ${animation_diligent_includes})
    target_link_libraries(AnimationRuntimeGrannyParityTest PRIVATE AnimationRuntime AssetRuntimeGranny
        ${animation_client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface M2FirstPartyArchitectureChecks)
    add_test(NAME AnimationRuntime.GrannyParity COMMAND AnimationRuntimeGrannyParityTest
        "${CMAKE_SOURCE_DIR}/../m2dev-client/assets" "${CMAKE_BINARY_DIR}/phase-f1x/parity-$<CONFIG>.csv")
    set_tests_properties(AnimationRuntime.GrannyParity PROPERTIES LABELS "animation;gpu;real-assets;fast;f1x" TIMEOUT "$<IF:$<CONFIG:Debug>,300,180>" RUN_SERIAL TRUE)
    add_executable(AnimationRuntimeActorPathTest ActorPathTest.cpp)
    target_include_directories(AnimationRuntimeActorPathTest PRIVATE ${animation_diligent_includes})
    target_link_libraries(AnimationRuntimeActorPathTest PRIVATE AnimationRuntime AssetRuntimeGranny
        ${animation_client_libraries} M2RendererDiligent Diligent-GraphicsEngineD3D11Interface M2FirstPartyArchitectureChecks)
    add_test(NAME AnimationRuntime.ActorPath COMMAND AnimationRuntimeActorPathTest "${CMAKE_SOURCE_DIR}/../m2dev-client/assets")
    set_tests_properties(AnimationRuntime.ActorPath PROPERTIES LABELS "animation;gpu;real-assets;fast;f1x" TIMEOUT 120 RUN_SERIAL TRUE)
endif()
