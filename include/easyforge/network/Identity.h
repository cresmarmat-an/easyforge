#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <easyforge/core/Result.h>
#include <easyforge/network/Export.h>

namespace easyforge
{
    namespace internal::networking
    {
        class SigningKey;
    }

    // What lets a server prove it is the server a client expects, so nobody
    // can stand in for it: a key pair, kept by the server, whose public half
    // clients are given ahead of time.
    //
    //     // Once, when setting the server up:
    //     NetworkIdentity identity = NetworkIdentity::New();
    //     identity.Save("server.identity");
    //     std::string key = identity.PublicKey();   // ship this with the clients
    //
    //     // Every time the server starts:
    //     Server server = Server::New({ .Identity = NetworkIdentity::Load("server.identity") });
    //
    //     // In the client:
    //     Client client = Client::New({ .Address = "play.example.com", .ServerKey = key });
    //
    // The file holds the private half: keep it on the server only. NetworkIdentity
    // is a handle: copies share the key.
    class EASYFORGE_NETWORK_EXPORT NetworkIdentity
    {
    public:
        // A new, random identity.
        [[nodiscard]] static NetworkIdentity New();

        // An identity saved with Save. Tests as false when the file cannot be
        // read or is not an identity, and Error() says why.
        [[nodiscard]] static NetworkIdentity Load(std::string_view path);

        // No identity: a server with none proves nothing about itself. Tests
        // as false.
        NetworkIdentity();

        explicit operator bool() const;
        const std::string& Error() const;

        // Writes the identity, private half included, to a file.
        Result<> Save(std::string_view path) const;

        // The public half as 128 hexadecimal digits, for ClientSettings::ServerKey.
        std::string PublicKey() const;

        // Used by the network library itself.
        const std::shared_ptr<internal::networking::SigningKey>& Key() const { return Held; }

    private:
        std::shared_ptr<internal::networking::SigningKey> Held;
        std::string ErrorText;
    };
}
