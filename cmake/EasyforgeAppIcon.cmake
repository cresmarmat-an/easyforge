# easyforge_app_icon(<target> <image>)
#
# Builds <image> into the program as its icon: the one Explorer, the taskbar,
# and shortcuts show. A window without its own Icon setting uses it too.
#
#     add_executable(notes main.cpp)
#     easyforge_app_icon(notes icon.png)
#
# The image can be any format the assets library reads; a square PNG of at least
# 256 by 256 pixels gives the best results. On Windows it becomes an .ico file
# with every size from 16 to 256 pixels. On other platforms the function does
# nothing yet; their icon files arrive with those platforms.
function(easyforge_app_icon target image)
    if(NOT WIN32)
        return()
    endif()

    if(TARGET easyforge_icon_tool)
        set(tool easyforge_icon_tool)
        set(tool_dependency easyforge_icon_tool)
    elseif(TARGET easyforge::icon-tool)
        set(tool easyforge::icon-tool)
        set(tool_dependency "$<TARGET_FILE:easyforge::icon-tool>")
    else()
        message(FATAL_ERROR "easyforge_app_icon needs the easyforge assets library, which is switched off.")
    endif()

    cmake_path(ABSOLUTE_PATH image BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE source)
    set(folder "${CMAKE_CURRENT_BINARY_DIR}/easyforge-app-icon/${target}")
    set(icon "${folder}/${target}.ico")
    set(resource "${folder}/${target}.rc")

    add_custom_command(
        OUTPUT "${icon}"
        COMMAND "$<TARGET_FILE:${tool}>" "${source}" "${icon}"
        DEPENDS "${source}" ${tool_dependency}
        COMMENT "Making the icon for ${target}"
        VERBATIM)

    file(CONFIGURE OUTPUT "${resource}" CONTENT "1 ICON \"@icon@\"\n" @ONLY)
    target_sources(${target} PRIVATE "${resource}" "${icon}")
    set_source_files_properties("${resource}" PROPERTIES OBJECT_DEPENDS "${icon}")
endfunction()
