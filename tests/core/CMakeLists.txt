add_executable(easyforge-core-tests
    Main.cpp
    ColorTests.cpp
    FrameTimerTests.cpp
    JobsStressTests.cpp
    JobsTests.cpp
    KeyTests.cpp
    LanguageTests.cpp
    LogTests.cpp
    MatrixTests.cpp
    PathTests.cpp
    PropertyTests.cpp
    QuaternionTests.cpp
    RandomTests.cpp
    RectangleTests.cpp
    ResultTests.cpp
    TimeAndVersionTests.cpp
    TransformTests.cpp
    VectorTests.cpp)

target_link_libraries(easyforge-core-tests PRIVATE easyforge::core)
target_compile_definitions(easyforge-core-tests PRIVATE
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
easyforge_set_warnings(easyforge-core-tests)

add_test(NAME core COMMAND easyforge-core-tests)
