add_executable(easyforge_icon_tool main.cpp)
add_executable(easyforge::icon-tool ALIAS easyforge_icon_tool)
set_target_properties(easyforge_icon_tool PROPERTIES
    OUTPUT_NAME easyforge-icon
    EXPORT_NAME icon-tool)
if(NOT PROJECT_IS_TOP_LEVEL)
    set_target_properties(easyforge_icon_tool PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
target_link_libraries(easyforge_icon_tool PRIVATE easyforge::assets)
easyforge_set_warnings(easyforge_icon_tool)
