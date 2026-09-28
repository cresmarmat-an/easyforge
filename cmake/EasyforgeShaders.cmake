# easyforge_add_shaders(<target> <shader>...)
#
# Checks each shader when <target> is built, and copies it next to the program,
# where Shader::Load finds it:
#
#     add_executable(notes main.cpp)
#     easyforge_add_shaders(notes ripple.shader shaders/glow.shader)
#
# A shader with a problem stops the build with its file, line, and column. The
# check runs again only when a shader changes.
function(easyforge_add_shaders target)
    if(NOT ARGN)
        return()
    endif()

    if(TARGET easyforge_shader_tool)
        set(tool easyforge_shader_tool)
        set(tool_dependency easyforge_shader_tool)
    elseif(TARGET easyforge::shader-tool)
        set(tool easyforge::shader-tool)
        set(tool_dependency "$<TARGET_FILE:easyforge::shader-tool>")
    else()
        message(FATAL_ERROR "easyforge_add_shaders needs the easyforge graphics library, which is not built.")
    endif()

    set(sources "")
    foreach(shader IN LISTS ARGN)
        cmake_path(ABSOLUTE_PATH shader BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE source)
        list(APPEND sources "${source}")
    endforeach()

    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/easyforge-shaders/${target}.checked")
    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND "$<TARGET_FILE:${tool}>" ${sources}
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS ${sources} ${tool_dependency}
        COMMENT "Checking the shaders of ${target}"
        VERBATIM)
    target_sources(${target} PRIVATE "${stamp}")

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${sources} "$<TARGET_FILE_DIR:${target}>"
        VERBATIM)
endfunction()
