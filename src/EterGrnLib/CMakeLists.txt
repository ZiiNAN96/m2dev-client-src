file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

list(FILTER FILE_SOURCES EXCLUDE REGEX "/Deform[.]cpp$")
add_library(EterGrnLib STATIC ${FILE_SOURCES})

target_link_libraries(EterGrnLib
	mio
	AssetRuntime
	AssetRuntimeGranny
)

GroupSourcesByFolder(EterGrnLib)
