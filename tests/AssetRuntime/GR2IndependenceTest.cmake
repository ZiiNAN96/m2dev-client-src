file(GLOB reader_sources "${SOURCE_ROOT}/src/AssetRuntime/GR2/*.h" "${SOURCE_ROOT}/src/AssetRuntime/GR2/*.cpp")
foreach(path IN LISTS reader_sources)
    file(READ "${path}" source)
    if(source MATCHES "#[ \t]*include[ \t]*[<\"][^>\"]*(granny|Granny|windows|Windows|Diligent|d3d)" OR
       source MATCHES "granny_[A-Za-z0-9_]+|Granny[A-Za-z0-9_]+[ \t]*\\(")
        message(FATAL_ERROR "SDK/platform dependency in native GR2 reader: ${path}")
    endif()
endforeach()
message(STATUS "PASS: native GR2 core has no Granny/platform includes, types or calls")
