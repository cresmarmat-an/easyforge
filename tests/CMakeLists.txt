get_property(built_libraries GLOBAL PROPERTY EASYFORGE_BUILT_LIBRARIES)

foreach(library IN LISTS built_libraries)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${library}/CMakeLists.txt")
        add_subdirectory(${library})
    endif()
endforeach()

add_subdirectory(headers)

# Public headers must not use names that windows.h turns into macros.
add_test(NAME public-header-names
    COMMAND ${CMAKE_COMMAND}
        -D "HEADER_DIRECTORY=${PROJECT_SOURCE_DIR}/include"
        -P "${PROJECT_SOURCE_DIR}/cmake/CheckHeaderNames.cmake")

# A library's code may only include headers of the libraries it depends on.
add_test(NAME library-includes
    COMMAND ${CMAKE_COMMAND}
        -D "SOURCE_ROOT=${PROJECT_SOURCE_DIR}"
        -P "${PROJECT_SOURCE_DIR}/cmake/CheckLibraryIncludes.cmake")
