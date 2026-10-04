add_executable(easyforge_script_tool main.cpp)
add_executable(easyforge::script-tool ALIAS easyforge_script_tool)
set_target_properties(easyforge_script_tool PROPERTIES
    OUTPUT_NAME easyforge-script
    EXPORT_NAME script-tool)
if(NOT PROJECT_IS_TOP_LEVEL)
    set_target_properties(easyforge_script_tool PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
target_link_libraries(easyforge_script_tool PRIVATE easyforge::script)
easyforge_set_warnings(easyforge_script_tool)