add_library(AssetRuntime STATIC AssetRuntime.cpp)
target_include_directories(AssetRuntime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(AssetRuntime PUBLIC cxx_std_20)
target_link_libraries(AssetRuntime PRIVATE M2FirstPartyArchitectureChecks)

add_library(AssetRuntimeGlTF STATIC GlTF/GlTFAssetProvider.cpp GlTF/CgltfImplementation.cpp)
target_link_libraries(AssetRuntimeGlTF PUBLIC AssetRuntime PRIVATE M2FirstPartyArchitectureChecks)
target_include_directories(AssetRuntimeGlTF SYSTEM PRIVATE "${CMAKE_SOURCE_DIR}/vendor/cgltf")

add_library(AssetRuntimeGR2 STATIC GR2/GR2File.cpp GR2/GR2Compression.cpp GR2/GR2Types.cpp
    GR2/GR2ModelReader.cpp GR2/GR2MeshReader.cpp GR2/GR2MaterialReader.cpp GR2/GR2AnimationReader.cpp GR2/GR2AssetProvider.cpp)
target_link_libraries(AssetRuntimeGR2 PUBLIC AssetRuntime AnimationRuntime PRIVATE M2FirstPartyArchitectureChecks)
add_executable(GR2CorpusScanner ../../tools/GR2/CorpusScanner.cpp)
target_link_libraries(GR2CorpusScanner PRIVATE AssetRuntimeGR2 M2FirstPartyArchitectureChecks)

if(M2_BUILD_WINDOWS_CLIENT)
    add_library(AssetRuntimeGranny STATIC Granny/GrannyAssetProvider.cpp Granny/GrannyAnimationAdapter.cpp ../EterGrnLib/Deform.cpp)
    target_link_libraries(AssetRuntimeGranny PUBLIC AssetRuntime AnimationRuntime PRIVATE Granny bcrypt M2FirstPartyArchitectureChecks)
    target_include_directories(AssetRuntimeGranny PRIVATE "${CMAKE_SOURCE_DIR}/extern/include")
    add_library(AssetRuntimeProviders STATIC Providers.cpp)
    target_link_libraries(AssetRuntimeProviders PUBLIC AssetRuntimeGranny AssetRuntimeGlTF AssetRuntimeGR2 PRIVATE M2FirstPartyArchitectureChecks)
endif()
