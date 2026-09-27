# Fails when a library's code includes the header of a library it does not
# depend on, according to the table in EasyforgeLibraries.cmake. Linking would
# catch most of these, but not code that only uses inline functions.
#
#     cmake -D SOURCE_ROOT=<repository> -P CheckLibraryIncludes.cmake

set(PROJECT_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}")
include("${CMAKE_CURRENT_LIST_DIR}/EasyforgeLibraries.cmake")

set(problems "")
set(checked 0)
foreach(library IN LISTS EASYFORGE_LIBRARIES)
    easyforge_dependency_closure(${library} allowed)
    list(APPEND allowed version)

    file(GLOB_RECURSE files
        "${SOURCE_ROOT}/src/${library}/*.h"
        "${SOURCE_ROOT}/src/${library}/*.cpp"
        "${SOURCE_ROOT}/include/easyforge/${library}/*.h")
    if(EXISTS "${SOURCE_ROOT}/include/easyforge/${library}.h")
        list(APPEND files "${SOURCE_ROOT}/include/easyforge/${library}.h")
    endif()

    foreach(file IN LISTS files)
        math(EXPR checked "${checked} + 1")
        file(STRINGS "${file}" includes REGEX "^[ \t]*#[ \t]*include[ \t]*<easyforge/")
        foreach(line IN LISTS includes)
            string(REGEX MATCH "<easyforge/([A-Za-z0-9_]+)" match "${line}")
            set(included "${CMAKE_MATCH_1}")
            if(NOT included IN_LIST allowed)
                file(RELATIVE_PATH shown "${SOURCE_ROOT}" "${file}")
                list(APPEND problems "${shown} (library ${library}) includes <easyforge/${included}...>")
            endif()
        endforeach()
    endforeach()
endforeach()

if(problems)
    list(JOIN problems "\n  " listed)
    message(FATAL_ERROR "Libraries include headers of libraries they do not depend on:\n  ${listed}")
endif()

message(STATUS "Checked the includes of ${checked} files")
