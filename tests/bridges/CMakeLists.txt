add_executable(easyforge-bridge-tests
    Main.cpp
    DataScriptTests.cpp)

target_link_libraries(easyforge-bridge-tests PRIVATE easyforge::script easyforge::data)
if(TARGET easyforge::ui)
    target_sources(easyforge-bridge-tests PRIVATE UiScriptTests.cpp)
    target_link_libraries(easyforge-bridge-tests PRIVATE easyforge::ui)
endif()
easyforge_set_warnings(easyforge-bridge-tests)

add_test(NAME bridges COMMAND easyforge-bridge-tests)
