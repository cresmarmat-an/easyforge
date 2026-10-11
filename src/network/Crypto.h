#pragma once

// The cryptography connections use, all from the operating system (CNG on
// Windows): random bytes, HMAC-SHA256, HKDF, ECDH and ECDSA on the P-256 curve,
// and AES-256-GCM. Nothing here is cryptography of easyforge's own; this file
// only puts the system's in the shape the network needs.

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/network/Export.h>

// The library's own tests use what is marked for export here, so it stays
// reachable when the library is a DLL.
namespace easyforge::internal::networking
{
    // A P-256 public key as its X and Y coordinates, and an ECDSA signature as
    // its R and S, each 32 bytes, big-endian.
    inline constexpr std::size_t PublicKeySize = 64;
    inline constexpr std::size_t SignatureSize = 64;

    // AES-256 keys, GCM tags, and the share of an HMAC kept in a cookie.
    inline constexpr std::size_t KeySize = 32;
    inline constexpr std::size_t TagSize = 16;

    using PublicKey = std::array<std::uint8_t, PublicKeySize>;
    using Signature = std::array<std::uint8_t, SignatureSize>;
    using SecretKey = std::array<std::uint8_t, KeySize>;
    using Digest = std::array<std::uint8_t, 32>;

    // Fills `bytes` from the system's random generator. False only when the
    // system has no cryptography, as on platforms network does not run on yet.
    EASYFORGE_NETWORK_EXPORT bool RandomBytes(std::span<std::uint8_t> bytes);

    // HMAC-SHA256 of `data` with `key`.
    EASYFORGE_NETWORK_EXPORT Digest Authenticate(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data);

    // HKDF with SHA-256 (RFC 5869): `output.size()` bytes of key material from a
    // shared secret, made specific to `salt` and `purpose`.
    EASYFORGE_NETWORK_EXPORT void DeriveKeys(std::span<const std::uint8_t> secret, std::span<const std::uint8_t> salt,
        std::string_view purpose, std::span<std::uint8_t> output);

    // Whether two byte sequences are equal, taking the same time whatever they
    // hold, for comparing authentication codes.
    EASYFORGE_NETWORK_EXPORT bool SameBytes(std::span<const std::uint8_t> first, std::span<const std::uint8_t> second);

    // Bytes as lowercase hexadecimal digits, and back. BytesFromHex is false for
    // text that is not whole bytes of hexadecimal.
    EASYFORGE_NETWORK_EXPORT std::string HexText(std::span<const std::uint8_t> bytes);
    EASYFORGE_NETWORK_EXPORT bool BytesFromHex(std::string_view text, std::vector<std::uint8_t>& bytes);
    EASYFORGE_NETWORK_EXPORT bool PublicKeyFromText(std::string_view text, PublicKey& key);

    // A key pair made for one connection, for agreeing on a shared secret with
    // the other end (ECDH on P-256).
    class EASYFORGE_NETWORK_EXPORT KeyExchange
    {
    public:
        KeyExchange();
        ~KeyExchange();
        KeyExchange(const KeyExchange&) = delete;
        KeyExchange& operator=(const KeyExchange&) = delete;

        // False when the key pair could not be made.
        explicit operator bool() const;
        const PublicKey& Public() const { return OwnPublic; }

        // The secret both ends arrive at from their own key pair and the other's
        // public key. False when `other` is not a point on the curve.
        bool Agree(const PublicKey& other, SecretKey& shared) const;

    private:
        struct State;
        std::unique_ptr<State> Held;
        PublicKey OwnPublic {};
    };

    // A long-term key a server proves who it is with (ECDSA on P-256).
    class EASYFORGE_NETWORK_EXPORT SigningKey
    {
    public:
        // A new random key.
        static std::shared_ptr<SigningKey> Make();

        // From what Private gave, or null when it is not a key.
        static std::shared_ptr<SigningKey> FromPrivate(std::span<const std::uint8_t> bytes);

        ~SigningKey();

        const PublicKey& Public() const { return OwnPublic; }

        // The private key, to keep in a file and give to FromPrivate later.
        std::vector<std::uint8_t> Private() const;

        // Signs the SHA-256 hash of `data`.
        bool Sign(std::span<const std::uint8_t> data, Signature& signature) const;

        struct State;

    private:
        SigningKey();
        std::unique_ptr<State> Held;
        PublicKey OwnPublic {};
    };

    // Whether `signature` is `key`'s, over `data`.
    EASYFORGE_NETWORK_EXPORT bool Verify(const PublicKey& key, std::span<const std::uint8_t> data,
        const Signature& signature);

    // Seals and opens packets with one AES-256-GCM key. Each packet's nonce is
    // its 64-bit counter, which must never repeat under one key.
    class EASYFORGE_NETWORK_EXPORT Sealer
    {
    public:
        Sealer();
        ~Sealer();
        Sealer(const Sealer&) = delete;
        Sealer& operator=(const Sealer&) = delete;

        bool SetKey(const SecretKey& key);

        // Appends `plain`, sealed, and its tag to `output`. `associated` is
        // authenticated without being hidden, such as the packet's header.
        bool Seal(std::uint64_t counter, std::span<const std::uint8_t> associated, std::span<const std::uint8_t> plain,
            std::vector<std::uint8_t>& output) const;

        // Opens what Seal made into `plain`. False when anything was changed,
        // or it was sealed with another key or counter.
        bool Open(std::uint64_t counter, std::span<const std::uint8_t> associated, std::span<const std::uint8_t> sealed,
            std::vector<std::uint8_t>& plain) const;

    private:
        struct State;
        std::unique_ptr<State> Held;
    };
}
