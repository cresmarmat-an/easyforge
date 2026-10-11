#include "../Crypto.h"

#include <algorithm>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <bcrypt.h>

#ifndef NT_SUCCESS
#define NT_SUCCESS(status) (static_cast<NTSTATUS>(status) >= 0)
#endif

// The raw shared secret of an ECDH agreement; older headers lack the name.
#ifndef BCRYPT_KDF_RAW_SECRET
#define BCRYPT_KDF_RAW_SECRET L"TRUNCATE"
#endif

namespace easyforge::internal::networking
{
    namespace
    {
        // The system's algorithms, opened once for the whole program. CNG allows
        // an algorithm handle to be used from several threads at once.
        struct Algorithms
        {
            BCRYPT_ALG_HANDLE Hmac = nullptr;
            BCRYPT_ALG_HANDLE Hash = nullptr;
            BCRYPT_ALG_HANDLE Exchange = nullptr;
            BCRYPT_ALG_HANDLE Signing = nullptr;
            BCRYPT_ALG_HANDLE Cipher = nullptr;

            Algorithms()
            {
                Open(Hmac, BCRYPT_SHA256_ALGORITHM, BCRYPT_ALG_HANDLE_HMAC_FLAG);
                Open(Hash, BCRYPT_SHA256_ALGORITHM, 0);
                Open(Exchange, BCRYPT_ECDH_P256_ALGORITHM, 0);
                Open(Signing, BCRYPT_ECDSA_P256_ALGORITHM, 0);
                if (Open(Cipher, BCRYPT_AES_ALGORITHM, 0))
                {
                    static wchar_t mode[] = BCRYPT_CHAIN_MODE_GCM;
                    if (!NT_SUCCESS(BCryptSetProperty(Cipher, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(mode), sizeof(mode), 0)))
                    {
                        BCryptCloseAlgorithmProvider(Cipher, 0);
                        Cipher = nullptr;
                    }
                }
            }

            ~Algorithms()
            {
                for (BCRYPT_ALG_HANDLE handle : { Hmac, Hash, Exchange, Signing, Cipher })
                {
                    if (handle)
                    {
                        BCryptCloseAlgorithmProvider(handle, 0);
                    }
                }
            }

            static bool Open(BCRYPT_ALG_HANDLE& handle, const wchar_t* name, ULONG flags)
            {
                if (!NT_SUCCESS(BCryptOpenAlgorithmProvider(&handle, name, nullptr, flags)))
                {
                    handle = nullptr;
                    return false;
                }
                return true;
            }
        };

        const Algorithms& System()
        {
            static const Algorithms algorithms;
            return algorithms;
        }

        ULONG Count(std::size_t size)
        {
            return static_cast<ULONG>(size);
        }

        // CNG takes input as non-const pointers it does not write through.
        PUCHAR Input(const std::uint8_t* bytes)
        {
            static std::uint8_t nothing = 0;
            return const_cast<PUCHAR>(bytes ? bytes : &nothing);
        }

        using KeyBlob = std::array<std::uint8_t, sizeof(BCRYPT_ECCKEY_BLOB) + PublicKeySize>;

        bool ExportPublic(BCRYPT_KEY_HANDLE key, PublicKey& into)
        {
            KeyBlob blob {};
            ULONG written = 0;
            if (!NT_SUCCESS(BCryptExportKey(key, nullptr, BCRYPT_ECCPUBLIC_BLOB, blob.data(), Count(blob.size()), &written, 0)) ||
                written != blob.size())
            {
                return false;
            }
            std::memcpy(into.data(), blob.data() + sizeof(BCRYPT_ECCKEY_BLOB), PublicKeySize);
            return true;
        }

        // Null when the bytes are not a point on the curve.
        BCRYPT_KEY_HANDLE ImportPublic(BCRYPT_ALG_HANDLE algorithm, ULONG magic, const PublicKey& key)
        {
            if (!algorithm)
            {
                return nullptr;
            }
            KeyBlob blob {};
            BCRYPT_ECCKEY_BLOB header { magic, 32 };
            std::memcpy(blob.data(), &header, sizeof header);
            std::memcpy(blob.data() + sizeof header, key.data(), PublicKeySize);
            BCRYPT_KEY_HANDLE handle = nullptr;
            if (!NT_SUCCESS(BCryptImportKeyPair(algorithm, nullptr, BCRYPT_ECCPUBLIC_BLOB, &handle, blob.data(), Count(blob.size()), 0)))
            {
                return nullptr;
            }
            return handle;
        }

        Digest Sha256(std::span<const std::uint8_t> data)
        {
            Digest digest {};
            if (System().Hash)
            {
                BCryptHash(System().Hash, nullptr, 0, Input(data.data()), Count(data.size()), digest.data(), Count(digest.size()));
            }
            return digest;
        }

        // The 96-bit GCM nonce: the counter, then zeros.
        std::array<std::uint8_t, 12> Nonce(std::uint64_t counter)
        {
            std::array<std::uint8_t, 12> nonce {};
            for (int index = 0; index < 8; ++index)
            {
                nonce[static_cast<std::size_t>(index)] = static_cast<std::uint8_t>(counter >> (8 * index));
            }
            return nonce;
        }

        void Destroy(BCRYPT_KEY_HANDLE& key)
        {
            if (key)
            {
                BCryptDestroyKey(key);
                key = nullptr;
            }
        }
    }

    bool RandomBytes(std::span<std::uint8_t> bytes)
    {
        return bytes.empty() ||
               NT_SUCCESS(BCryptGenRandom(nullptr, bytes.data(), Count(bytes.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG));
    }

    Digest Authenticate(std::span<const std::uint8_t> key, std::span<const std::uint8_t> data)
    {
        Digest digest {};
        if (System().Hmac)
        {
            BCryptHash(System().Hmac, Input(key.data()), Count(key.size()), Input(data.data()), Count(data.size()), digest.data(),
                Count(digest.size()));
        }
        return digest;
    }

    void DeriveKeys(std::span<const std::uint8_t> secret, std::span<const std::uint8_t> salt, std::string_view purpose,
        std::span<std::uint8_t> output)
    {
        // RFC 5869: extract a key from the secret, then expand it block by block.
        Digest noSalt {};
        Digest pseudorandom = Authenticate(salt.empty() ? std::span<const std::uint8_t>(noSalt) : salt, secret);
        Digest previous {};
        std::vector<std::uint8_t> block;
        std::size_t done = 0;
        for (std::uint8_t counter = 1; done < output.size(); ++counter)
        {
            block.clear();
            if (counter > 1)
            {
                block.insert(block.end(), previous.begin(), previous.end());
            }
            block.insert(block.end(), purpose.begin(), purpose.end());
            block.push_back(counter);
            previous = Authenticate(pseudorandom, block);
            std::size_t take = std::min(previous.size(), output.size() - done);
            std::memcpy(output.data() + done, previous.data(), take);
            done += take;
        }
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

    // ---- Key exchange ----------------------------------------------------------------

    struct KeyExchange::State
    {
        BCRYPT_KEY_HANDLE Key = nullptr;
        ~State() { Destroy(Key); }
    };

    KeyExchange::KeyExchange() : Held(std::make_unique<State>())
    {
        BCRYPT_KEY_HANDLE key = nullptr;
        if (!System().Exchange || !NT_SUCCESS(BCryptGenerateKeyPair(System().Exchange, &key, 256, 0)))
        {
            return;
        }
        if (!NT_SUCCESS(BCryptFinalizeKeyPair(key, 0)) || !ExportPublic(key, OwnPublic))
        {
            BCryptDestroyKey(key);
            return;
        }
        Held->Key = key;
    }

    KeyExchange::~KeyExchange() = default;

    KeyExchange::operator bool() const
    {
        return Held && Held->Key;
    }

    bool KeyExchange::Agree(const PublicKey& other, SecretKey& shared) const
    {
        if (!*this)
        {
            return false;
        }
        BCRYPT_KEY_HANDLE theirs = ImportPublic(System().Exchange, BCRYPT_ECDH_PUBLIC_P256_MAGIC, other);
        if (!theirs)
        {
            return false;
        }
        BCRYPT_SECRET_HANDLE secret = nullptr;
        bool agreed = NT_SUCCESS(BCryptSecretAgreement(Held->Key, theirs, &secret, 0));
        if (agreed)
        {
            ULONG written = 0;
            agreed = NT_SUCCESS(BCryptDeriveKey(secret, BCRYPT_KDF_RAW_SECRET, nullptr, shared.data(), Count(shared.size()), &written, 0)) &&
                     written == shared.size();
            BCryptDestroySecret(secret);
        }
        BCryptDestroyKey(theirs);
        return agreed;
    }

    // ---- Signing ---------------------------------------------------------------------

    struct SigningKey::State
    {
        BCRYPT_KEY_HANDLE Key = nullptr;
        ~State() { Destroy(Key); }
    };

    std::shared_ptr<SigningKey> SigningKey::Make()
    {
        BCRYPT_KEY_HANDLE key = nullptr;
        if (!System().Signing || !NT_SUCCESS(BCryptGenerateKeyPair(System().Signing, &key, 256, 0)))
        {
            return nullptr;
        }
        std::shared_ptr<SigningKey> made(new SigningKey());
        made->Held = std::make_unique<State>();
        made->Held->Key = key;
        if (!NT_SUCCESS(BCryptFinalizeKeyPair(key, 0)) || !ExportPublic(key, made->OwnPublic))
        {
            return nullptr;
        }
        return made;
    }

    std::shared_ptr<SigningKey> SigningKey::FromPrivate(std::span<const std::uint8_t> bytes)
    {
        // X, Y, and the private number d, 32 bytes each.
        if (!System().Signing || bytes.size() != PublicKeySize + 32)
        {
            return nullptr;
        }
        std::vector<std::uint8_t> blob(sizeof(BCRYPT_ECCKEY_BLOB) + bytes.size());
        BCRYPT_ECCKEY_BLOB header { BCRYPT_ECDSA_PRIVATE_P256_MAGIC, 32 };
        std::memcpy(blob.data(), &header, sizeof header);
        std::memcpy(blob.data() + sizeof header, bytes.data(), bytes.size());
        BCRYPT_KEY_HANDLE key = nullptr;
        if (!NT_SUCCESS(BCryptImportKeyPair(System().Signing, nullptr, BCRYPT_ECCPRIVATE_BLOB, &key, blob.data(), Count(blob.size()), 0)))
        {
            return nullptr;
        }
        std::shared_ptr<SigningKey> made(new SigningKey());
        made->Held = std::make_unique<State>();
        made->Held->Key = key;
        std::memcpy(made->OwnPublic.data(), bytes.data(), PublicKeySize);
        return made;
    }

    SigningKey::SigningKey() = default;
    SigningKey::~SigningKey() = default;

    std::vector<std::uint8_t> SigningKey::Private() const
    {
        std::vector<std::uint8_t> blob(sizeof(BCRYPT_ECCKEY_BLOB) + PublicKeySize + 32);
        ULONG written = 0;
        if (!Held || !NT_SUCCESS(BCryptExportKey(Held->Key, nullptr, BCRYPT_ECCPRIVATE_BLOB, blob.data(), Count(blob.size()), &written, 0)) ||
            written != blob.size())
        {
            return {};
        }
        return std::vector<std::uint8_t>(blob.begin() + sizeof(BCRYPT_ECCKEY_BLOB), blob.end());
    }

    bool SigningKey::Sign(std::span<const std::uint8_t> data, Signature& signature) const
    {
        if (!Held || !Held->Key)
        {
            return false;
        }
        Digest digest = Sha256(data);
        ULONG written = 0;
        return NT_SUCCESS(BCryptSignHash(Held->Key, nullptr, digest.data(), Count(digest.size()), signature.data(),
                   Count(signature.size()), &written, 0)) &&
               written == signature.size();
    }

    bool Verify(const PublicKey& key, std::span<const std::uint8_t> data, const Signature& signature)
    {
        BCRYPT_KEY_HANDLE handle = ImportPublic(System().Signing, BCRYPT_ECDSA_PUBLIC_P256_MAGIC, key);
        if (!handle)
        {
            return false;
        }
        Digest digest = Sha256(data);
        bool valid = NT_SUCCESS(BCryptVerifySignature(handle, nullptr, digest.data(), Count(digest.size()),
            Input(signature.data()), Count(signature.size()), 0));
        BCryptDestroyKey(handle);
        return valid;
    }

    // ---- Sealing ---------------------------------------------------------------------

    struct Sealer::State
    {
        BCRYPT_KEY_HANDLE Key = nullptr;
        ~State() { Destroy(Key); }
    };

    Sealer::Sealer() : Held(std::make_unique<State>())
    {
    }

    Sealer::~Sealer() = default;

    bool Sealer::SetKey(const SecretKey& key)
    {
        Destroy(Held->Key);
        if (!System().Cipher)
        {
            return false;
        }
        if (!NT_SUCCESS(BCryptGenerateSymmetricKey(System().Cipher, &Held->Key, nullptr, 0, Input(key.data()), Count(key.size()), 0)))
        {
            Held->Key = nullptr;
            return false;
        }
        return true;
    }

    bool Sealer::Seal(std::uint64_t counter, std::span<const std::uint8_t> associated, std::span<const std::uint8_t> plain,
        std::vector<std::uint8_t>& output) const
    {
        if (!Held->Key)
        {
            return false;
        }
        std::array<std::uint8_t, 12> nonce = Nonce(counter);
        std::size_t start = output.size();
        output.resize(start + plain.size() + TagSize);
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO information;
        BCRYPT_INIT_AUTH_MODE_INFO(information);
        information.pbNonce = nonce.data();
        information.cbNonce = Count(nonce.size());
        information.pbAuthData = associated.empty() ? nullptr : Input(associated.data());
        information.cbAuthData = Count(associated.size());
        information.pbTag = output.data() + start + plain.size();
        information.cbTag = Count(TagSize);
        ULONG written = 0;
        if (!NT_SUCCESS(BCryptEncrypt(Held->Key, Input(plain.data()), Count(plain.size()), &information, nullptr, 0,
                plain.empty() ? nullptr : output.data() + start, Count(plain.size()), &written, 0)))
        {
            output.resize(start);
            return false;
        }
        return true;
    }

    bool Sealer::Open(std::uint64_t counter, std::span<const std::uint8_t> associated, std::span<const std::uint8_t> sealed,
        std::vector<std::uint8_t>& plain) const
    {
        if (!Held->Key || sealed.size() < TagSize)
        {
            return false;
        }
        std::size_t length = sealed.size() - TagSize;
        std::array<std::uint8_t, 12> nonce = Nonce(counter);
        plain.resize(length);
        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO information;
        BCRYPT_INIT_AUTH_MODE_INFO(information);
        information.pbNonce = nonce.data();
        information.cbNonce = Count(nonce.size());
        information.pbAuthData = associated.empty() ? nullptr : Input(associated.data());
        information.cbAuthData = Count(associated.size());
        information.pbTag = Input(sealed.data() + length);
        information.cbTag = Count(TagSize);
        ULONG written = 0;
        if (!NT_SUCCESS(BCryptDecrypt(Held->Key, Input(sealed.data()), Count(length), &information, nullptr, 0,
                length == 0 ? nullptr : plain.data(), Count(length), &written, 0)))
        {
            plain.clear();
            return false;
        }
        return true;
    }
}
