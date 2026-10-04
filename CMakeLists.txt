cmake_minimum_required(VERSION 3.22...4.4)

project(easyforge
    VERSION 0.0.1
    DESCRIPTION "C++ libraries for windows, graphics, interfaces, sound, physics, networking, data, and scripting"
    HOMEPAGE_URL "https://cresmarmat-an.github.io/easyforge/"
    LANGUAGES CXX)

# The part of the version after the numbers. Empty for an official release.
set(EASYFORGE_VERSION_LABEL "")

if(EASYFORGE_VERSION_LABEL)
    set(EASYFORGE_VERSION_TEXT "${PROJECT_VERSION}-${EASYFORGE_VERSION_LABEL}")
else()
    set(EASYFORGE_VERSION_TEXT "${PROJECT_VERSION}")
endif()

include(GNUInstallDirs)
include(cmake/EasyforgeLibraries.cmake)
include(cmake/EasyforgeAppIcon.cmake)
include(cmake/EasyforgeShaders.cmake)
include(cmake/EasyforgeScripts.cmake)

# easyforge does not use C++20 modules, so CMake does not need to look for them.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)

option(EASYFORGE_BUILD_TESTS "Build the easyforge tests" ${PROJECT_IS_TOP_LEVEL})
option(EASYFORGE_INSTALL "Add the rules that install easyforge" ${PROJECT_IS_TOP_LEVEL})
set(EASYFORGE_ONLY "" CACHE STRING
    "Build only this library and the libraries it depends on, for example 'network'")

configure_file(cmake/version.h.in "${EASYFORGE_GENERATED_INCLUDE_DIRECTORY}/easyforge/version.h" @ONLY)

easyforge_add_libraries()
add_subdirectory(tools)

# One target that links every library that was built.
add_library(easyforge_easyforge INTERFACE)
add_library(easyforge::easyforge ALIAS easyforge_easyforge)
set_target_properties(easyforge_easyforge PROPERTIES EXPORT_NAME easyforge)
get_property(built_libraries GLOBAL PROPERTY EASYFORGE_BUILT_LIBRARIES)
foreach(library IN LISTS built_libraries)
    target_link_libraries(easyforge_easyforge INTERFACE easyforge::${library})
endforeach()

if(EASYFORGE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

easyforge_install_package()

message(STATUS "easyforge ${EASYFORGE_VERSION_TEXT}: building ${built_libraries}")
