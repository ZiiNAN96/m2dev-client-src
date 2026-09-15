if(NOT WIN32)
    message(FATAL_ERROR "The retained legacy SPT extraction binary requires Windows; VegetationRuntime does not.")
endif()
set(ZIINAN_LEGACY_SPT_SDK_ROOT "" CACHE PATH "External SDK used only by the optional offline SPT converter")
foreach(file include/SpeedTreeRT.h lib/speedtree_static.lib lib/speedtree_staticd.lib)
    if(NOT EXISTS "${ZIINAN_LEGACY_SPT_SDK_ROOT}/${file}")
        message(FATAL_ERROR "Optional SPT converter needs ZIINAN_LEGACY_SPT_SDK_ROOT with ${file}; the production client never needs it.")
    endif()
endforeach()
add_library(LegacySPTSDK STATIC IMPORTED)
set_target_properties(LegacySPTSDK PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${ZIINAN_LEGACY_SPT_SDK_ROOT}/include"
    IMPORTED_LOCATION "${ZIINAN_LEGACY_SPT_SDK_ROOT}/lib/speedtree_static.lib"
    IMPORTED_LOCATION_DEBUG "${ZIINAN_LEGACY_SPT_SDK_ROOT}/lib/speedtree_staticd.lib")
add_executable(ZiiNANSPTProbe LegacySPTProbe.cpp)
target_link_libraries(ZiiNANSPTProbe PRIVATE LegacySPTSDK user32 M2FirstPartyArchitectureChecks)
target_include_directories(ZiiNANSPTProbe SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/extern/include)
if(NOT TARGET AssetToolCore)
    message(FATAL_ERROR "Enable M2_BUILD_ASSET_TOOL to reuse the existing GLB writer for SPT conversion.")
endif()
add_library(LegacySPTExtraction STATIC LegacySPTExtractor.cpp)
target_include_directories(LegacySPTExtraction PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(LegacySPTExtraction PRIVATE LegacySPTSDK user32 PUBLIC VegetationRuntime AssetToolCore)
add_executable(ZiiNANVegetationTool Main.cpp)
target_link_libraries(ZiiNANVegetationTool PRIVATE LegacySPTExtraction M2FirstPartyArchitectureChecks)
add_test(NAME Vegetation.ConverterFailures COMMAND ${CMAKE_COMMAND}
    -DTOOL=$<TARGET_FILE:ZiiNANVegetationTool> -DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/failures-$<CONFIG>
    -P ${CMAKE_SOURCE_DIR}/tests/Vegetation/ConverterFailureTest.cmake)
set_tests_properties(Vegetation.ConverterFailures PROPERTIES TIMEOUT 25 LABELS "vegetation;offline;fast;hx")
