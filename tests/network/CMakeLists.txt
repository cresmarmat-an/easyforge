add_executable(easyforge-network-tests
    Main.cpp
    ConnectionTests.cpp
    DeliveryTests.cpp
    MessageTests.cpp
    RequestTests.cpp
    ThreadTests.cpp)

target_link_libraries(easyforge-network-tests PRIVATE easyforge::network)
easyforge_set_warnings(easyforge-network-tests)

add_test(NAME network COMMAND easyforge-network-tests)
