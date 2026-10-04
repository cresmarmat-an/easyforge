add_executable(easyforge-sound-tests
    Main.cpp
    MixerTests.cpp
    EffectsTests.cpp
    DeviceTests.cpp)

target_link_libraries(easyforge-sound-tests PRIVATE easyforge::sound)
target_compile_definitions(easyforge-sound-tests PRIVATE
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
easyforge_set_warnings(easyforge-sound-tests)

add_test(NAME sound COMMAND easyforge-sound-tests)
