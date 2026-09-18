# Dependency sources are pinned submodules; generated files stay in the build tree.
function(m2_use_external_dependency name directory)
    string(TOUPPER "${name}" key)
    if(NOT FETCHCONTENT_SOURCE_DIR_${key})
        set(source "${PROJECT_SOURCE_DIR}/external/${directory}")
        if(NOT EXISTS "${source}/CMakeLists.txt")
            message(FATAL_ERROR
                "Missing external/${directory}. Run git submodule update --init external/${directory}; "
                "see external/README.md for nested platform dependencies.")
        endif()
        set(FETCHCONTENT_SOURCE_DIR_${key} "${source}" PARENT_SCOPE)
    endif()
endfunction()
