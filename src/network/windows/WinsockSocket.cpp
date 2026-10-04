// UDP sockets through Winsock.

#include "../Socket.h"

#include <format>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <mstcpip.h>

// Documented with mstcpip.h, though not every SDK defines it there.
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

namespace easyforge::internal::networking
{
    namespace
    {
        // Winsock starts the first time a socket is needed, and stays started
        // until the program ends.
        bool StartWinsock(std::string& error)
        {
            static const int result = [] {
                WSADATA data {};
                return WSAStartup(MAKEWORD(2, 2), &data);
            }();
            if (result != 0)
            {
                error = std::format("Winsock could not start (error {})", result);
                return false;
            }
            return true;
        }

        sockaddr_in ToSystem(const SocketAddress& address)
        {
            sockaddr_in system {};
            system.sin_family = AF_INET;
            system.sin_port = htons(address.Port);
            system.sin_addr.s_addr = htonl(address.Host);
            return system;
        }

        SocketAddress FromSystem(const sockaddr_in& system)
        {
            return { ntohl(system.sin_addr.s_addr), ntohs(system.sin_port) };
        }

        template <typename Value>
        void SetOption(SOCKET handle, int level, int option, Value value)
        {
            setsockopt(handle, level, option, reinterpret_cast<const char*>(&value), static_cast<int>(sizeof value));
        }

        class WinsockSocket final : public UdpSocket
        {
        public:
            WinsockSocket(SOCKET handle, std::uint16_t port) : Handle(handle), BoundPort(port) {}

            WinsockSocket(const WinsockSocket&) = delete;
            WinsockSocket& operator=(const WinsockSocket&) = delete;

            ~WinsockSocket() override { closesocket(Handle); }

            void Send(const SocketAddress& to, std::span<const std::uint8_t> bytes) override
            {
                sockaddr_in address = ToSystem(to);
                sendto(Handle, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0,
                    reinterpret_cast<const sockaddr*>(&address), static_cast<int>(sizeof address));
            }

            std::optional<std::size_t> Receive(SocketAddress& from, std::span<std::uint8_t> buffer) override
            {
                while (true)
                {
                    sockaddr_in address {};
                    int addressSize = static_cast<int>(sizeof address);
                    int received = recvfrom(Handle, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                        reinterpret_cast<sockaddr*>(&address), &addressSize);
                    if (received == SOCKET_ERROR)
                    {
                        int problem = WSAGetLastError();
                        // A packet larger than any easyforge sends, or news of one
                        // that could not be delivered: skipped.
                        if (problem == WSAEMSGSIZE || problem == WSAECONNRESET || problem == WSAENETRESET)
                        {
                            continue;
                        }
                        return std::nullopt;
                    }
                    from = FromSystem(address);
                    return static_cast<std::size_t>(received);
                }
            }

            void Wait(int milliseconds) override
            {
                WSAPOLLFD entry {};
                entry.fd = Handle;
                entry.events = POLLRDNORM;
                WSAPoll(&entry, 1, milliseconds);
            }

            std::uint16_t Port() const override { return BoundPort; }

        private:
            SOCKET Handle;
            std::uint16_t BoundPort;
        };
    }

    std::unique_ptr<UdpSocket> OpenSocket(const SocketSettings& settings, std::string& error)
    {
        if (!StartWinsock(error))
        {
            return nullptr;
        }
        SOCKET handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (handle == INVALID_SOCKET)
        {
            error = std::format("could not open a socket (Winsock error {})", WSAGetLastError());
            return nullptr;
        }

        // Otherwise a packet sent to a closed port makes a later receive fail.
        BOOL report = FALSE;
        DWORD returned = 0;
        WSAIoctl(handle, SIO_UDP_CONNRESET, &report, sizeof report, nullptr, 0, &returned, nullptr, nullptr);

        u_long nonblocking = 1;
        ioctlsocket(handle, FIONBIO, &nonblocking);

        // Room for the packets that arrive between two updates.
        SetOption(handle, SOL_SOCKET, SO_RCVBUF, 1 << 20);
        SetOption(handle, SOL_SOCKET, SO_SNDBUF, 1 << 20);

        // No other program can bind the same port while this socket has it.
        SetOption(handle, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, BOOL { TRUE });
        if (settings.Broadcast)
        {
            SetOption(handle, SOL_SOCKET, SO_BROADCAST, BOOL { TRUE });
        }

        sockaddr_in address = ToSystem({ settings.ThisComputerOnly ? LoopbackHost : 0u, settings.Port });
        if (bind(handle, reinterpret_cast<const sockaddr*>(&address), static_cast<int>(sizeof address)) == SOCKET_ERROR)
        {
            int problem = WSAGetLastError();
            closesocket(handle);
            if (problem == WSAEADDRINUSE || problem == WSAEACCES)
            {
                error = std::format("port {} is in use by another program", settings.Port);
            }
            else
            {
                error = std::format("could not use port {} (Winsock error {})", settings.Port, problem);
            }
            return nullptr;
        }

        sockaddr_in bound {};
        int boundSize = static_cast<int>(sizeof bound);
        getsockname(handle, reinterpret_cast<sockaddr*>(&bound), &boundSize);
        return std::make_unique<WinsockSocket>(handle, ntohs(bound.sin_port));
    }

    std::optional<SocketAddress> FindAddress(std::string_view name, std::uint16_t port, std::string& error)
    {
        if (!StartWinsock(error))
        {
            return std::nullopt;
        }
        addrinfo hints {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        addrinfo* found = nullptr;
        std::string text(name);
        if (text.empty() || getaddrinfo(text.c_str(), nullptr, &hints, &found) != 0 || !found)
        {
            error = std::format("could not find the address \"{}\"", name);
            return std::nullopt;
        }
        SocketAddress address = FromSystem(*reinterpret_cast<const sockaddr_in*>(found->ai_addr));
        address.Port = port;
        freeaddrinfo(found);
        return address;
    }
}
