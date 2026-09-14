add_executable(AssetToolTest AssetToolTest.cpp)
target_link_libraries(AssetToolTest PRIVATE AssetToolCore AssetRuntimeGlTF M2FirstPartyArchitectureChecks)
set(asset_tool_fixture_argument "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
set(asset_tool_roundtrip_argument "${CMAKE_CURRENT_BINARY_DIR}/roundtrip-$<CONFIG>")
set(asset_tool_cli_argument "${CMAKE_CURRENT_BINARY_DIR}/cli-$<CONFIG>")
if(CYGWIN)
    # Native Windows CMake launches a POSIX-path executable in this test directory.
    file(RELATIVE_PATH asset_tool_fixture_argument "${CMAKE_CURRENT_BINARY_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")
    set(asset_tool_roundtrip_argument "roundtrip-$<CONFIG>")
    set(asset_tool_cli_argument "cli-$<CONFIG>")
endif()
add_test(NAME AssetTool.Unit COMMAND AssetToolTest --unit)
add_test(NAME AssetTool.ImportRoundtrip COMMAND AssetToolTest --import
    "${asset_tool_fixture_argument}" "${asset_tool_roundtrip_argument}")
add_test(NAME AssetTool.CLI COMMAND ${CMAKE_COMMAND}
    "-DTOOL=$<TARGET_FILE:ZiiNANAssetTool>"
    "-DFIXTURES=${asset_tool_fixture_argument}"
    "-DOUTPUT=${asset_tool_cli_argument}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/CLITest.cmake")
add_test(NAME AssetTool.RuntimeIsolation COMMAND ${CMAKE_COMMAND}
    "-DSOURCE_ROOT=${CMAKE_SOURCE_DIR}" -P "${CMAKE_CURRENT_SOURCE_DIR}/RuntimeIsolationTest.cmake")
set_tests_properties(AssetTool.Unit AssetTool.ImportRoundtrip AssetTool.CLI AssetTool.RuntimeIsolation
    PROPERTIES LABELS "assets;offline;portable;fast;e2x" TIMEOUT 30)
if(TARGET GlTFRenderTest)
    add_test(NAME AssetTool.ConvertRenderFixture COMMAND ${CMAKE_COMMAND}
        "-DTOOL=$<TARGET_FILE:ZiiNANAssetTool>" "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/fixtures/market_stall.obj"
        "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/e2x-market-stall-$<CONFIG>.glb"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/ConvertFixture.cmake")
    set_tests_properties(AssetTool.ConvertRenderFixture PROPERTIES LABELS "assets;offline;fast;e2x" TIMEOUT 30 FIXTURES_SETUP "e2x-converted")
    add_test(NAME AssetTool.DiligentRoundtrip COMMAND GlTFRenderTest
        "${CMAKE_CURRENT_BINARY_DIR}/e2x-market-stall-$<CONFIG>.glb" --offline-asset)
    set_tests_properties(AssetTool.DiligentRoundtrip PROPERTIES LABELS "assets;offline;gpu;fast;e2x" TIMEOUT 60
        FIXTURES_REQUIRED "e2x-converted" RUN_SERIAL TRUE)
endif()
