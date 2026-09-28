add_executable(easyforge-core-tests
    Main.cpp
    ColorTests.cpp
    JobsTests.cpp
    KeyTests.cpp
    LanguageTests.cpp
    LogTests.cpp
    MatrixTests.cpp
    PropertyTests.cpp
    QuaternionTests.cpp
    RandomTests.cpp
    RectangleTests.cpp
    ResultTests.cpp
    TimeAndVersionTests.cpp
    TransformTests.cpp
    VectorTests.cpp)

target_link_libraries(easyforge-core-tests PRIVATE easyforge::core)
easyforge_set_warnings(easyforge-core-tests)

add_test(NAME core COMMAND easyforge-core-tests)
