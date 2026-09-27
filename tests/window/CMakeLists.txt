add_executable(easyforge-window-tests
    Main.cpp
    EventTests.cpp
    SystemTests.cpp
    ViewTests.cpp
    WindowTests.cpp)

target_link_libraries(easyforge-window-tests PRIVATE easyforge::window)
target_compile_definitions(easyforge-window-tests PRIVATE
    EASYFORGE_TEST_ICON="${PROJECT_SOURCE_DIR}/tests/assets/data/images/rgba8.png")
easyforge_set_warnings(easyforge-window-tests)

# Built with an icon, so the tests can check that windows use the program's icon.
easyforge_app_icon(easyforge-window-tests "${PROJECT_SOURCE_DIR}/tests/assets/data/images/rgba8.png")

add_test(NAME window COMMAND easyforge-window-tests)
