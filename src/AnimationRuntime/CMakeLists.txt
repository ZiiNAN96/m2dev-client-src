add_library(AnimationRuntime STATIC AnimationRuntime.cpp)
target_include_directories(AnimationRuntime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(AnimationRuntime PUBLIC cxx_std_20)
target_link_libraries(AnimationRuntime PRIVATE M2FirstPartyArchitectureChecks)
