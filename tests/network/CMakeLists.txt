add_executable(easyforge-network-tests
    Main.cpp
    CongestionTests.cpp
    ConnectionTests.cpp
    DamagedPacketTests.cpp
    DeliveryTests.cpp
    MessageTests.cpp
    RequestTests.cpp
    SecurityTests.cpp
    SmoothingTests.cpp
    ThreadTests.cpp)

target_link_libraries(easyforge-network-tests PRIVATE easyforge::network)
# The congestion and security tests drive the library's own parts, which are
# not public.
target_include_directories(easyforge-network-tests PRIVATE "${PROJECT_SOURCE_DIR}/tests/common"
    "${PROJECT_SOURCE_DIR}/src/network")
if(WIN32)
    # The damaged packet test runs a relay of its own over Winsock.
    target_link_libraries(easyforge-network-tests PRIVATE ws2_32)
endif()
target_compile_definitions(easyforge-network-tests PRIVATE
    EASYFORGE_TEST_OUTPUT="${CMAKE_CURRENT_BINARY_DIR}/output/")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/output")
easyforge_set_warnings(easyforge-network-tests)

add_test(NAME network COMMAND easyforge-network-tests)
