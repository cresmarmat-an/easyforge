#include <easyforge/network/Identity.h>

#include <format>
#include <fstream>

#include <easyforge/core/Paths.h>

#include "Crypto.h"

namespace easyforge
{
    namespace internal::networking
    {
        std::string HexText(std::span<const std::uint8_t> bytes)
        {
            constexpr char Digits[] = "0123456789abcdef";
            std::string text;
            text.reserve(bytes.size() * 2);
            for (std::uint8_t byte : bytes)
            {
                text += Digits[byte >> 4];
                text += Digits[byte & 15];
            }
            return text;
        }

        bool BytesFromHex(std::string_view text, std::vector<std::uint8_t>& bytes)
        {
            auto digit = [](char character) -> int {
                if (character >= '0' && character <= '9')
                {
                    return character - '0';
                }
                if (character >= 'a' && character <= 'f')
                {
                    return character - 'a' + 10;
                }
                if (character >= 'A' && character <= 'F')
                {
                    return character - 'A' + 10;
                }
                return -1;
            };
            if (text.size() % 2 != 0)
            {
                return false;
            }
            bytes.clear();
            for (std::size_t index = 0; index < text.size(); index += 2)
            {
                int high = digit(text[index]);
                int low = digit(text[index + 1]);
                if (high < 0 || low < 0)
                {
                    return false;
                }
                bytes.push_back(static_cast<std::uint8_t>(high * 16 + low));
            }
            return true;
        }

        bool PublicKeyFromText(std::string_view text, PublicKey& key)
        {
            std::vector<std::uint8_t> bytes;
            if (!BytesFromHex(text, bytes) || bytes.size() != key.size())
            {
                return false;
            }
            std::copy(bytes.begin(), bytes.end(), key.begin());
            return true;
        }
    }

    namespace
    {
        constexpr std::string_view FirstLine = "easyforge network identity 1";
    }

    NetworkIdentity NetworkIdentity::New()
    {
        NetworkIdentity identity;
        identity.Held = internal::networking::SigningKey::Make();
        if (!identity.Held)
        {
            identity.ErrorText = "this system could not make a key";
        }
        return identity;
    }

    NetworkIdentity NetworkIdentity::Load(std::string_view path)
    {
        NetworkIdentity identity;
        std::ifstream file(PathFromText(path), std::ios::binary);
        if (!file)
        {
            identity.ErrorText = std::format("{}: the file could not be opened", path);
            return identity;
        }
        std::string first;
        std::string key;
        std::getline(file, first);
        std::getline(file, key);
        while (!key.empty() && (key.back() == '\r' || key.back() == ' '))
        {
            key.pop_back();
        }
        while (!first.empty() && first.back() == '\r')
        {
            first.pop_back();
        }
        std::vector<std::uint8_t> bytes;
        if (first != FirstLine || !internal::networking::BytesFromHex(key, bytes))
        {
            identity.ErrorText = std::format("{}: this is not an easyforge network identity", path);
            return identity;
        }
        identity.Held = internal::networking::SigningKey::FromPrivate(bytes);
        if (!identity.Held)
        {
            identity.ErrorText = std::format("{}: the key in it is damaged", path);
        }
        return identity;
    }

    NetworkIdentity::NetworkIdentity() = default;

    NetworkIdentity::operator bool() const
    {
        return Held != nullptr;
    }

    const std::string& NetworkIdentity::Error() const
    {
        return ErrorText;
    }

    Result<> NetworkIdentity::Save(std::string_view path) const
    {
        if (!Held)
        {
            return Failure("there is no identity to save");
        }
        std::string text = std::format("{}\n{}\n", FirstLine, internal::networking::HexText(Held->Private()));
        return WriteWholeFile(path, text);
    }

    std::string NetworkIdentity::PublicKey() const
    {
        return Held ? internal::networking::HexText(Held->Public()) : std::string();
    }
}
