# The easyforge libraries and what each one may depend on.
#
# This table is the only place dependencies are written down. A library links
# exactly the libraries listed for it, and the library-includes test fails if its
# code includes the header of any library outside this list.

set(EASYFORGE_LIBRARIES
    core
    assets
    data
    input
    network
    physics
    script
    window
    graphics
    sound
    ui)

set(EASYFORGE_DEPENDENCIES_core "")
set(EASYFORGE_DEPENDENCIES_assets core)
set(EASYFORGE_DEPENDENCIES_data core)
set(EASYFORGE_DEPENDENCIES_input core)
set(EASYFORGE_DEPENDENCIES_network core)
set(EASYFORGE_DEPENDENCIES_physics core)
set(EASYFORGE_DEPENDENCIES_script core)
set(EASYFORGE_DEPENDENCIES_window core assets)
set(EASYFORGE_DEPENDENCIES_graphics core assets)
set(EASYFORGE_DEPENDENCIES_sound core assets)
set(EASYFORGE_DEPENDENCIES_ui core assets data graphics)

set(EASYFORGE_GENERATED_INCLUDE_DIRECTORY "${PROJECT_BINARY_DIR}/generated/include")

# Sets `output` to `library` followed by every library it depends on, directly or not.
function(easyforge_dependency_closure library output)
    set(result ${library})
    set(pending ${EASYFORGE_DEPENDENCIES_${library}})
    while(pending)
        list(POP_FRONT pending next)
        if(NOT next IN_LIST result)
            list(APPEND result ${next})
            list(APPEND pending ${EASYFORGE_DEPENDENCIES_${next}})
        endif()
    endwhile()
    set(${output} ${result} PARENT_SCOPE)
endfunction()

# Compiler warnings for easyforge's own code. They are private, so programs that
# use easyforge keep their own warning settings.
function(easyforge_set_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /WX /permissive- /utf-8 /Zc:__cplusplus)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
endfunction()

# Adds every library that has source code and is switched on.
function(easyforge_add_libraries)
    if(EASYFORGE_ONLY)
        if(NOT EASYFORGE_ONLY IN_LIST EASYFORGE_LIBRARIES)
            list(JOIN EASYFORGE_LIBRARIES ", " names)
            message(FATAL_ERROR "EASYFORGE_ONLY is '${EASYFORGE_ONLY}', which is not an easyforge library. "
                "The libraries are ${names}.")
        endif()
        easyforge_dependency_closure(${EASYFORGE_ONLY} wanted)
    else()
        set(wanted ${EASYFORGE_LIBRARIES})
    endif()

    foreach(library IN LISTS EASYFORGE_LIBRARIES)
        if(NOT EXISTS "${PROJECT_SOURCE_DIR}/src/${library}/CMakeLists.txt")
            continue()
        endif()
        string(TOUPPER ${library} upper)
        option(EASYFORGE_BUILD_${upper} "Build the easyforge ${library} library" ON)
        if(EASYFORGE_BUILD_${upper} AND library IN_LIST wanted)
            foreach(dependency IN LISTS EASYFORGE_DEPENDENCIES_${library})
                if(NOT TARGET easyforge::${dependency})
                    string(TOUPPER ${dependency} dependency_upper)
                    message(FATAL_ERROR "The ${library} library needs ${dependency}, which is switched off. "
                        "Turn on EASYFORGE_BUILD_${dependency_upper} or turn off EASYFORGE_BUILD_${upper}.")
                endif()
            endforeach()
            add_subdirectory(src/${library})
        endif()
    endforeach()
endfunction()

# Creates the target for one library. Called from src/<library>/CMakeLists.txt:
#
#     easyforge_add_library(core SOURCES Color.cpp Jobs.cpp ...)
function(easyforge_add_library library)
    cmake_parse_arguments(PARSE_ARGV 1 LIBRARY "" "" "SOURCES")

    set(target easyforge_${library})
    add_library(${target} STATIC ${LIBRARY_SOURCES})
    add_library(easyforge::${library} ALIAS ${target})

    set_target_properties(${target} PROPERTIES
        EXPORT_NAME ${library}
        OUTPUT_NAME easyforge-${library})

    # When another project fetches easyforge, only the libraries it links get built.
    if(NOT PROJECT_IS_TOP_LEVEL)
        set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()

    target_compile_features(${target} PUBLIC cxx_std_20)
    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${EASYFORGE_GENERATED_INCLUDE_DIRECTORY}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

    foreach(dependency IN LISTS EASYFORGE_DEPENDENCIES_${library})
        target_link_libraries(${target} PUBLIC easyforge::${dependency})
    endforeach()

    easyforge_set_warnings(${target})

    if(EASYFORGE_INSTALL)
        install(TARGETS ${target}
            EXPORT easyforgeTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()

    set_property(GLOBAL APPEND PROPERTY EASYFORGE_BUILT_LIBRARIES ${library})
endfunction()

# Installs headers, libraries, and the files find_package(easyforge) reads.
function(easyforge_install_package)
    if(NOT EASYFORGE_INSTALL)
        return()
    endif()
    include(CMakePackageConfigHelpers)

    get_property(built_libraries GLOBAL PROPERTY EASYFORGE_BUILT_LIBRARIES)

    install(DIRECTORY "${PROJECT_SOURCE_DIR}/include/easyforge" DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(FILES "${EASYFORGE_GENERATED_INCLUDE_DIRECTORY}/easyforge/version.h"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/easyforge)

    install(TARGETS easyforge_easyforge EXPORT easyforgeTargets)
    if(TARGET easyforge_icon_tool)
        install(TARGETS easyforge_icon_tool EXPORT easyforgeTargets RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(EXPORT easyforgeTargets
        NAMESPACE easyforge::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/easyforge)

    set(EASYFORGE_INSTALLED_LIBRARIES "${built_libraries}")
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/easyforgeConfig.cmake.in"
        "${PROJECT_BINARY_DIR}/easyforgeConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/easyforge)

    # Before 1.0 any release can change the interface, so find_package only
    # accepts the exact version that was asked for.
    write_basic_package_version_file(
        "${PROJECT_BINARY_DIR}/easyforgeConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion)

    install(FILES
        "${PROJECT_BINARY_DIR}/easyforgeConfig.cmake"
        "${PROJECT_BINARY_DIR}/easyforgeConfigVersion.cmake"
        "${PROJECT_SOURCE_DIR}/cmake/EasyforgeAppIcon.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/easyforge)
endfunction()
