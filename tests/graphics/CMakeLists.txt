add_executable(easyforge-graphics-tests
    Main.cpp
    CanvasTests.cpp
    EffectTests.cpp
    ShaderTests.cpp
    TextAndSceneTests.cpp)

target_link_libraries(easyforge-graphics-tests PRIVATE easyforge::graphics)
target_compile_definitions(easyforge-graphics-tests PRIVATE
    EASYFORGE_TEST_FONT="${PROJECT_SOURCE_DIR}/tests/assets/data/fonts/test_regular.ttf"
    EASYFORGE_TEST_MODEL="${PROJECT_SOURCE_DIR}/tests/assets/data/models/cube.obj"
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")

# Drawing into windows is tested when the window library is built too.
if(TARGET easyforge::window)
    target_sources(easyforge-graphics-tests PRIVATE WindowTests.cpp)
    target_link_libraries(easyforge-graphics-tests PRIVATE easyforge::window)
endif()

easyforge_set_warnings(easyforge-graphics-tests)

add_test(NAME graphics COMMAND easyforge-graphics-tests)

# The shader tool accepts a good shader and refuses one with problems.
if(TARGET easyforge_shader_tool)
    add_test(NAME shader-tool-accepts COMMAND easyforge_shader_tool "${CMAKE_CURRENT_SOURCE_DIR}/data/ripple.shader")
    add_test(NAME shader-tool-refuses COMMAND easyforge_shader_tool "${CMAKE_CURRENT_SOURCE_DIR}/data/broken.shader")
    set_tests_properties(shader-tool-refuses PROPERTIES WILL_FAIL TRUE)
endif()
