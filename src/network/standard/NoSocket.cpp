// Platforms whose sockets arrive in a later stage.

#include "../Socket.h"

namespace easyforge::internal::networking
{
    std::unique_ptr<UdpSocket> OpenSocket(const SocketSettings&, std::string& error)
    {
        error = "easyforge network cannot open sockets on this system yet";
        return nullptr;
    }

    std::optional<SocketAddress> FindAddress(std::string_view, std::uint16_t, std::string& error)
    {
        error = "easyforge network cannot look up addresses on this system yet";
        return std::nullopt;
    }
}
