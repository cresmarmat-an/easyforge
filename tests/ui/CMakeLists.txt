add_executable(easyforge-ui-tests
    Main.cpp
    DragTests.cpp
    DrawTests.cpp
    ElementTests.cpp
    EventTests.cpp
    LayoutTests.cpp
    ListTests.cpp
    PopupTests.cpp
    TabTests.cpp
    TextFieldTests.cpp)

target_link_libraries(easyforge-ui-tests PRIVATE easyforge::ui)
target_compile_definitions(easyforge-ui-tests PRIVATE
    EASYFORGE_TEST_FONT="${PROJECT_SOURCE_DIR}/tests/assets/data/fonts/test_regular.ttf"
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
easyforge_set_warnings(easyforge-ui-tests)

# An interface in a real window, when the window library is built too.
if(TARGET easyforge::window)
    target_sources(easyforge-ui-tests PRIVATE WindowTests.cpp)
    target_link_libraries(easyforge-ui-tests PRIVATE easyforge::window)
endif()

add_test(NAME ui COMMAND easyforge-ui-tests)
