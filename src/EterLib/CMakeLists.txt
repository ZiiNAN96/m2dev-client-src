file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

add_library(EterLib STATIC ${FILE_SOURCES})

target_link_libraries(EterLib
	sodium
	mio
	freetype
	M2Platform
	AssetRuntimeGR2
)

GroupSourcesByFolder(EterLib)
