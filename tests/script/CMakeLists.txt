add_executable(easyforge-script-tests
    Main.cpp
    LanguageTests.cpp)

target_link_libraries(easyforge-script-tests PRIVATE easyforge::script)
target_compile_definitions(easyforge-script-tests PRIVATE
    EASYFORGE_TEST_SCRIPTS="${CMAKE_CURRENT_SOURCE_DIR}/data/"
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
easyforge_set_warnings(easyforge-script-tests)

add_test(NAME script COMMAND easyforge-script-tests)

# The C interface, from a program written in C, where there is a C compiler.
include(CheckLanguage)
check_language(C)
if(CMAKE_C_COMPILER)
    enable_language(C)
    add_executable(easyforge-script-c-test CInterfaceTest.c)
    target_link_libraries(easyforge-script-c-test PRIVATE easyforge::script)
    set_target_properties(easyforge-script-c-test PROPERTIES LINKER_LANGUAGE CXX)
    easyforge_set_warnings(easyforge-script-c-test)
    add_test(NAME script-c COMMAND easyforge-script-c-test)
endif()
