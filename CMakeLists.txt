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
option(EASYFORGE_SANITIZE "Build with AddressSanitizer, so the tests catch memory errors" OFF)
option(EASYFORGE_BUILD_BENCHMARKS "Build the programs that time the libraries' heavy work" OFF)
option(BUILD_SHARED_LIBS "Build each easyforge library as a shared library (a DLL on Windows)" OFF)

# A program finds the DLLs it needs in its own folder, so a shared build puts
# every program and library in one folder, bin/<configuration>.
if(BUILD_SHARED_LIBS AND PROJECT_IS_TOP_LEVEL AND NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()

if(EASYFORGE_SANITIZE)
    if(MSVC)
        # MSVC's sanitizer cannot be used with its own run-time checks, and needs
        # whole programs linked at once.
        foreach(flags CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_DEBUG)
            string(REPLACE "/RTC1" "" ${flags} "${${flags}}")
        endforeach()
        add_compile_options(/fsanitize=address /Zi)
        add_link_options(/INCREMENTAL:NO /DEBUG)
    else()
        add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address,undefined)
    endif()
endif()

# Windows 10 is the oldest Windows easyforge runs on. Saying so makes every
# compiler's Windows headers declare what it adds, such as per-screen scaling.
if(WIN32)
    add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
endif()

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

if(EASYFORGE_BUILD_BENCHMARKS)
    add_subdirectory(tests/benchmarks)
endif()

# MSVC's sanitizer runtime is a DLL next to the compiler, which is on the PATH
# only in a developer prompt. A copy next to every program lets the tests and
# tools start from anywhere; without it they cannot start at all.
if(EASYFORGE_SANITIZE AND MSVC)
    get_filename_component(compiler_folder "${CMAKE_CXX_COMPILER}" DIRECTORY)
    file(GLOB sanitizer_runtimes "${compiler_folder}/clang_rt.asan*_dynamic-x86_64.dll")
    if(NOT sanitizer_runtimes)
        message(WARNING "The sanitizer runtime was not found next to ${CMAKE_CXX_COMPILER}")
    endif()
    # Adds the programs made in `directory` and below it to the list named `into`.
    function(easyforge_find_programs directory into)
        set(found ${${into}})
        get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target IN LISTS targets)
            get_target_property(type ${target} TYPE)
            if(type STREQUAL "EXECUTABLE")
                list(APPEND found ${target})
            endif()
        endforeach()
        get_property(folders DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
        foreach(folder IN LISTS folders)
            easyforge_find_programs("${folder}" found)
        endforeach()
        set(${into} ${found} PARENT_SCOPE)
    endfunction()
    set(sanitized_programs)
    easyforge_find_programs("${PROJECT_SOURCE_DIR}" sanitized_programs)
    set(copies)
    foreach(program IN LISTS sanitized_programs)
        list(APPEND copies
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${program}>"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${sanitizer_runtimes} "$<TARGET_FILE_DIR:${program}>")
    endforeach()
    if(sanitized_programs AND sanitizer_runtimes)
        add_custom_target(easyforge_sanitizer_runtime ALL ${copies} VERBATIM)
    endif()
endif()

easyforge_install_package()

message(STATUS "easyforge ${EASYFORGE_VERSION_TEXT}: building ${built_libraries}")
