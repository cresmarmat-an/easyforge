add_executable(easyforge_shader_tool main.cpp)
add_executable(easyforge::shader-tool ALIAS easyforge_shader_tool)
set_target_properties(easyforge_shader_tool PROPERTIES
    OUTPUT_NAME easyforge-shader
    EXPORT_NAME shader-tool)
if(NOT PROJECT_IS_TOP_LEVEL)
    set_target_properties(easyforge_shader_tool PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
target_link_libraries(easyforge_shader_tool PRIVATE easyforge::graphics)
easyforge_set_warnings(easyforge_shader_tool)
