add_executable(easyforge-network-tests
    Main.cpp
    ConnectionTests.cpp
    DamagedPacketTests.cpp
    DeliveryTests.cpp
    MessageTests.cpp
    RequestTests.cpp
    SmoothingTests.cpp
    ThreadTests.cpp)

target_link_libraries(easyforge-network-tests PRIVATE easyforge::network)
target_include_directories(easyforge-network-tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/common")
if(WIN32)
    # The damaged packet test runs a relay of its own over Winsock.
    target_link_libraries(easyforge-network-tests PRIVATE ws2_32)
endif()
easyforge_set_warnings(easyforge-network-tests)

add_test(NAME network COMMAND easyforge-network-tests)
