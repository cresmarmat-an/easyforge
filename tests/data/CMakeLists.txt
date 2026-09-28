add_executable(easyforge-data-tests
    Main.cpp
    ChangeTests.cpp
    TableTests.cpp
    TreeTextTests.cpp
    ValueTests.cpp)

target_link_libraries(easyforge-data-tests PRIVATE easyforge::data)
target_compile_definitions(easyforge-data-tests PRIVATE
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
easyforge_set_warnings(easyforge-data-tests)

add_test(NAME data COMMAND easyforge-data-tests)
