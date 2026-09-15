file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(GameLib STATIC ${FILE_SOURCES})
if(M2_BUILD_RENDERER_TESTS)
    target_compile_definitions(GameLib PRIVATE M2_RENDERER_DIAGNOSTICS=1)
endif()

target_link_libraries(GameLib
	lzo2
	mio
)

GroupSourcesByFolder(GameLib)
