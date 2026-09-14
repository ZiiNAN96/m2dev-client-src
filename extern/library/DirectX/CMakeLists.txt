# ZiiNAN: Platform abstraction
if(NOT WIN32)
    return()
endif()
add_library(WindowsInput INTERFACE)
target_include_directories(WindowsInput INTERFACE "${CMAKE_SOURCE_DIR}/extern/include")
target_link_libraries(WindowsInput INTERFACE "${CMAKE_CURRENT_LIST_DIR}/dinput8.lib" "${CMAKE_CURRENT_LIST_DIR}/dxguid.lib")
