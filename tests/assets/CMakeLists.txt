add_executable(easyforge-assets-tests
    Main.cpp
    CompressionTests.cpp
    DamagedFileTests.cpp
    FilesTests.cpp
    FontTests.cpp
    ImageTests.cpp
    ModelTests.cpp
    SoundTests.cpp)

target_link_libraries(easyforge-assets-tests PRIVATE easyforge::assets)
target_compile_definitions(easyforge-assets-tests PRIVATE
    EASYFORGE_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/"
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
easyforge_set_warnings(easyforge-assets-tests)

add_test(NAME assets COMMAND easyforge-assets-tests)
