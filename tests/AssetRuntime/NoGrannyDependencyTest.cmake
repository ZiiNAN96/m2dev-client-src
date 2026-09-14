# Historical CGranny* consumer names are intentionally retained. SDK symbols,
# includes, targets and artifacts are forbidden throughout the production tree.
file(GLOB_RECURSE production_sources "${SOURCE_ROOT}/src/*.h" "${SOURCE_ROOT}/src/*.cpp"
    "${SOURCE_ROOT}/src/*.c" "${SOURCE_ROOT}/src/CMakeLists.txt"
    "${SOURCE_ROOT}/extern/library/CMakeLists.txt")
list(APPEND production_sources "${SOURCE_ROOT}/CMakeLists.txt")
foreach(path IN LISTS production_sources)
    file(READ "${path}" source)
    if(source MATCHES "(^|[^A-Za-z0-9_])(granny_[A-Za-z0-9_]+|Granny[A-Za-z0-9_]+)[ \t\r\n]*(\\(|[;*&])" OR
       source MATCHES "#[ \t]*include[^\r\n]*(granny|Granny)" OR
       source MATCHES "granny2|AssetRuntimeGranny|BUILDING_GRANNY_STATIC|GRANNY_THREADED" OR
       source MATCHES "(add_subdirectory|target_link_libraries)[^\r\n]*Granny")
        message(FATAL_ERROR "Removed SDK dependency in production: ${path}")
    endif()
endforeach()
foreach(artifact extern/include/granny.h extern/include/granny2_spu_samplemodel.h
    extern/library/Granny/granny2_static.lib src/AssetRuntime/Granny/GrannyAssetProvider.cpp)
    if(EXISTS "${SOURCE_ROOT}/${artifact}")
        message(FATAL_ERROR "Removed SDK artifact is reachable: ${artifact}")
    endif()
endforeach()
message(STATUS "PASS: production SDK includes, types, calls, libraries and targets zero")
