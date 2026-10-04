# easyforge_add_scripts(<target> <file.script>...)
#
# Checks scripts in the easyforge script language when the target is built,
# with the types written in them, so a mistake stops the build with the file,
# line, and column. The scripts are copied next to the program.
#
#     easyforge_add_scripts(game menu.script enemies.script)

function(easyforge_add_scripts target)
    if(NOT ARGN)
        return()
    endif()

    if(TARGET easyforge_script_tool)
        set(tool easyforge_script_tool)
        set(tool_dependency easyforge_script_tool)
    elseif(TARGET easyforge::script-tool)
        set(tool easyforge::script-tool)
        set(tool_dependency "$<TARGET_FILE:easyforge::script-tool>")
    else()
        message(FATAL_ERROR "easyforge_add_scripts needs the easyforge script library, which is not built.")
    endif()

    set(sources "")
    foreach(script IN LISTS ARGN)
        cmake_path(ABSOLUTE_PATH script BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE OUTPUT_VARIABLE source)
        list(APPEND sources "${source}")
    endforeach()

    set(stamp "${CMAKE_CURRENT_BINARY_DIR}/easyforge-scripts/${target}.checked")
    add_custom_command(
        OUTPUT "${stamp}"
        COMMAND "$<TARGET_FILE:${tool}>" check ${sources}
        COMMAND ${CMAKE_COMMAND} -E touch "${stamp}"
        DEPENDS ${sources} ${tool_dependency}
        COMMENT "Checking the scripts of ${target}"
        VERBATIM)
    target_sources(${target} PRIVATE "${stamp}")

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${sources} "$<TARGET_FILE_DIR:${target}>"
        VERBATIM)
endfunction()