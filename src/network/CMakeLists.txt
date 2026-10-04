easyforge_add_library(network
    SOURCES
        Client.cpp
        Connection.cpp
        Discovery.cpp
        Endpoint.cpp
        Message.cpp
        Peer.cpp
        Server.cpp)

if(WIN32)
    target_sources(easyforge_network PRIVATE windows/WinsockSocket.cpp)
    target_link_libraries(easyforge_network PRIVATE ws2_32)
else()
    # Sockets on Linux, Apple platforms, Android, and the web arrive with those
    # platforms.
    target_sources(easyforge_network PRIVATE standard/NoSocket.cpp)
endif()
