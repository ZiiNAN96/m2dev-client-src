if(NOT ANDROID OR NOT M2_BUILD_ANDROID_BOOTSTRAP)
    message(FATAL_ERROR "Configure the repository root with the Android NDK toolchain.")
endif()

# ZiiNAN: Cross-platform bootstrap
set(native_glue "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
if(NOT EXISTS "${native_glue}/android_native_app_glue.c")
    message(FATAL_ERROR "The NDK native_app_glue source is required; install/configure a complete Android NDK.")
endif()
add_library(M2AndroidNativeGlue STATIC "${native_glue}/android_native_app_glue.c")
target_include_directories(M2AndroidNativeGlue PUBLIC "${native_glue}")
set_target_properties(M2AndroidNativeGlue PROPERTIES POSITION_INDEPENDENT_CODE ON)
add_library(M2AndroidBootstrap SHARED "${PROJECT_SOURCE_DIR}/src/Platform/Android/AndroidApplication.cpp")
target_link_libraries(M2AndroidBootstrap PRIVATE M2PlatformAndroid M2RendererVulkanBootstrap
    M2AndroidNativeGlue android log dl)
target_link_options(M2AndroidBootstrap PRIVATE "-uANativeActivity_onCreate")
set_target_properties(M2AndroidBootstrap PROPERTIES OUTPUT_NAME m2_android_bootstrap FOLDER "Platform/Android")
