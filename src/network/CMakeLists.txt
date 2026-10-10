easyforge_add_library(network
    SOURCES
        Client.cpp
        Connection.cpp
        Discovery.cpp
        Endpoint.cpp
        Identity.cpp
        Message.cpp
        Peer.cpp
        Server.cpp)

if(WIN32)
    # Sockets from Winsock, and cryptography from CNG.
    target_sources(easyforge_network PRIVATE windows/WinsockSocket.cpp windows/CngCrypto.cpp)
    target_link_libraries(easyforge_network PRIVATE ws2_32 bcrypt)
else()
    # Sockets and cryptography on Linux, Apple platforms, Android, and the web
    # arrive with those platforms.
    target_sources(easyforge_network PRIVATE standard/NoSocket.cpp standard/NoCrypto.cpp)
endif()
