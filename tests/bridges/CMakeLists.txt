add_executable(easyforge-bridge-tests Main.cpp)
target_link_libraries(easyforge-bridge-tests PRIVATE easyforge::data)

if(TARGET easyforge::script)
    target_sources(easyforge-bridge-tests PRIVATE DataScriptTests.cpp)
    target_link_libraries(easyforge-bridge-tests PRIVATE easyforge::script)
    if(TARGET easyforge::ui)
        target_sources(easyforge-bridge-tests PRIVATE UiScriptTests.cpp)
        target_link_libraries(easyforge-bridge-tests PRIVATE easyforge::ui)
    endif()
endif()

if(TARGET easyforge::network)
    target_sources(easyforge-bridge-tests PRIVATE DataNetworkTests.cpp)
    target_link_libraries(easyforge-bridge-tests PRIVATE easyforge::network)
endif()

easyforge_set_warnings(easyforge-bridge-tests)

add_test(NAME bridges COMMAND easyforge-bridge-tests)
