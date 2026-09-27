add_executable(easyforge-input-tests
    Main.cpp
    ControlsTests.cpp
    GamepadAndRecordingTests.cpp
    HostTests.cpp)

target_link_libraries(easyforge-input-tests PRIVATE easyforge::input)
target_compile_definitions(easyforge-input-tests PRIVATE
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
easyforge_set_warnings(easyforge-input-tests)

add_test(NAME input COMMAND easyforge-input-tests)
