// Platforms whose cryptography arrives with their sockets, in a later stage.
// Every function reports failure, so no connection is ever made unprotected.

#include "../Crypto.h"

#include <algorithm>

namespace easyforge::internal::networking
{
    bool RandomBytes(std::span<std::uint8_t> bytes)
    {
        return bytes.empty();
    }

    Digest Authenticate(std::span<const std::uint8_t>, std::span<const std::uint8_t>)
    {
        return {};
    }

    void DeriveKeys(std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::string_view, std::span<std::uint8_t> output)
    {
        std::fill(output.begin(), output.end(), std::uint8_t { 0 });
    }

    bool SameBytes(std::span<const std::uint8_t> first, std::span<const std::uint8_t> second)
    {
        if (first.size() != second.size())
        {
            return false;
        }
        std::uint8_t difference = 0;
        for (std::size_t index = 0; index < first.size(); ++index)
        {
            difference = static_cast<std::uint8_t>(difference | (first[index] ^ second[index]));
        }
        return difference == 0;
    }

    struct KeyExchange::State
    {
    };

    KeyExchange::KeyExchange() = default;
    KeyExchange::~KeyExchange() = default;

    KeyExchange::operator bool() const
    {
        return false;
    }

    bool KeyExchange::Agree(const PublicKey&, SecretKey&) const
    {
        return false;
    }

    struct SigningKey::State
    {
    };

    std::shared_ptr<SigningKey> SigningKey::Make()
    {
        return nullptr;
    }

    std::shared_ptr<SigningKey> SigningKey::FromPrivate(std::span<const std::uint8_t>)
    {
        return nullptr;
    }

    SigningKey::~SigningKey() = default;

    std::vector<std::uint8_t> SigningKey::Private() const
    {
        return {};
    }

    bool SigningKey::Sign(std::span<const std::uint8_t>, Signature&) const
    {
        return false;
    }

    bool Verify(const PublicKey&, std::span<const std::uint8_t>, const Signature&)
    {
        return false;
    }

    struct Sealer::State
    {
    };

    Sealer::Sealer() = default;
    Sealer::~Sealer() = default;

    bool Sealer::SetKey(const SecretKey&)
    {
        return false;
    }

    bool Sealer::Seal(std::uint64_t, std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::vector<std::uint8_t>&) const
    {
        return false;
    }

    bool Sealer::Open(std::uint64_t, std::span<const std::uint8_t>, std::span<const std::uint8_t>, std::vector<std::uint8_t>&) const
    {
        return false;
    }
}
