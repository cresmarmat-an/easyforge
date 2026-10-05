#pragma once

// Damaged copies of good input, for checking that a reader never crashes,
// hangs, or reads outside its data, whatever it is given. It may fail or give
// a wrong result; Debug builds check every vector access, and the sanitized
// build checks every other memory access, so a stray read fails loudly.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>

#include <easyforge/core/Random.h>

namespace easyforge::testing
{
    // Calls read(copy) for copies of `original` cut short at every length up
    // to 300 bytes and every 97 bytes after that, then for `attempts` copies
    // with a few bytes changed, flipped, put in, taken out, or repeated. Works
    // with std::string and std::vector<std::uint8_t>.
    template <typename Bytes, typename Read>
    void Damage(const Bytes& original, Read read, std::uint64_t seed, int attempts = 300)
    {
        for (std::size_t length = 0; length < original.size(); length += length < 300 ? 1 : 97)
        {
            read(Bytes(original.begin(), original.begin() + static_cast<std::ptrdiff_t>(length)));
        }

        // Values that tend to find mistakes: sizes of zero and of everything,
        // signs, and the characters that open and close things in text.
        constexpr std::uint8_t awkward[] = { 0x00, 0xFF, 0x7F, 0x80, 0x01, 0xFE, '"', '{', '}', '(', ')', '[', ']', '\n', '#', '-', '.', '9' };

        Random random(seed);
        for (int attempt = 0; attempt < attempts; ++attempt)
        {
            Bytes changed = original;
            int changes = random.IntegerBetween(1, 6);
            for (int change = 0; change < changes; ++change)
            {
                auto pick = [&] {
                    return changed.empty() ? std::size_t { 0 }
                                           : static_cast<std::size_t>(random.IntegerBetween(0, static_cast<int>(changed.size()) - 1));
                };
                using Unit = typename Bytes::value_type;
                switch (random.IntegerBetween(0, 5))
                {
                case 0:
                    if (!changed.empty())
                    {
                        changed[pick()] = static_cast<Unit>(random.Next());
                    }
                    break;
                case 1:
                    if (!changed.empty())
                    {
                        std::size_t place = pick();
                        changed[place] = static_cast<Unit>(static_cast<std::uint8_t>(changed[place]) ^ (1u << random.IntegerBetween(0, 7)));
                    }
                    break;
                case 2:
                    changed.insert(changed.begin() + static_cast<std::ptrdiff_t>(pick()),
                        static_cast<Unit>(awkward[random.IntegerBetween(0, static_cast<int>(std::size(awkward)) - 1)]));
                    break;
                case 3:
                    if (!changed.empty())
                    {
                        changed.erase(changed.begin() + static_cast<std::ptrdiff_t>(pick()));
                    }
                    break;
                case 4:
                    if (!changed.empty())
                    {
                        // A run of bytes repeated in place, as a duplicated line
                        // or record would be.
                        std::size_t start = pick();
                        std::size_t length = std::min<std::size_t>(changed.size() - start, static_cast<std::size_t>(random.IntegerBetween(1, 64)));
                        Bytes run(changed.begin() + static_cast<std::ptrdiff_t>(start),
                            changed.begin() + static_cast<std::ptrdiff_t>(start + length));
                        changed.insert(changed.begin() + static_cast<std::ptrdiff_t>(start), run.begin(), run.end());
                    }
                    break;
                default:
                    if (!changed.empty())
                    {
                        changed[pick()] = static_cast<Unit>(awkward[random.IntegerBetween(0, static_cast<int>(std::size(awkward)) - 1)]);
                    }
                    break;
                }
            }
            read(changed);
        }
    }
}
