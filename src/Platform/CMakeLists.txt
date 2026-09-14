# ZiiNAN: Platform abstraction
# ZiiNAN: Cross-platform bootstrap
add_library(M2PlatformCommon INTERFACE)
target_include_directories(M2PlatformCommon INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_compile_features(M2PlatformCommon INTERFACE cxx_std_20)
target_link_libraries(M2PlatformCommon INTERFACE M2FirstPartyArchitectureChecks)

if(M2_BUILD_WINDOWS_CLIENT)
add_library(M2Platform STATIC)
target_include_directories(M2Platform PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(M2Platform PRIVATE M2FirstPartyArchitectureChecks)
if(WIN32)
    target_sources(M2Platform PRIVATE
        Windows/Win32PlatformWindow.cpp
        Windows/Win32PlatformInput.cpp
        Windows/Win32Filesystem.cpp
        Windows/Win32Time.cpp
        Windows/Win32Networking.cpp
        Windows/Win32DynamicLibrary.cpp
        Windows/Win32Clipboard.cpp
        Windows/Win32Shell.cpp
    )
    target_link_libraries(M2Platform PRIVATE WindowsInput user32 gdi32 winmm ws2_32 shell32)

    add_library(M2PlatformWindowsFeatures STATIC Windows/Win32WebView.cpp)
    target_include_directories(M2PlatformWindowsFeatures PUBLIC "${PROJECT_SOURCE_DIR}/src")
    target_link_libraries(M2PlatformWindowsFeatures PRIVATE M2FirstPartyArchitectureChecks WebView ole32)
    set_target_properties(M2PlatformWindowsFeatures PROPERTIES FOLDER "Platform/Windows")
endif()
set_target_properties(M2Platform PROPERTIES FOLDER "Platform")
endif()

if(M2_BUILD_ANDROID_BOOTSTRAP)
    add_library(M2PlatformAndroid STATIC
        Android/AndroidWindow.cpp Android/AndroidInput.cpp Android/AndroidFilesystem.cpp
        Android/AndroidTime.cpp Android/AndroidDynamicLibrary.cpp)
    target_link_libraries(M2PlatformAndroid PUBLIC M2PlatformCommon PRIVATE android log dl)
    target_include_directories(M2PlatformAndroid PRIVATE "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    set_target_properties(M2PlatformAndroid PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "Platform/Android")
endif()
