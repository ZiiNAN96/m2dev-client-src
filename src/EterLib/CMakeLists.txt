file(GLOB_RECURSE FILE_SOURCES "*.h" "*.c" "*.cpp")

# ZiiNAN: No functional Legacy backend in the production library.
list(FILTER FILE_SOURCES EXCLUDE REGEX "/LegacyD3D9Backend[.](cpp|h)$")
add_library(EterLib STATIC ${FILE_SOURCES})

target_link_libraries(EterLib
	sodium
	mio
	freetype
)

GroupSourcesByFolder(EterLib)
