#pragma once

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <easyforge/assets.h>

// Helpers for reading the files in tests/assets/data, made by generate.py.
namespace testdata
{
    inline std::string Path(const std::string& name)
    {
        return std::string(EASYFORGE_TEST_DATA) + name;
    }

    inline std::vector<std::uint8_t> Read(const std::string& name)
    {
        std::ifstream stream(std::filesystem::path(Path(name)), std::ios::binary);
        return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    }

    // A .rgba file: width, height, then RGBA bytes.
    inline easyforge::ImageData Expected(const std::string& name)
    {
        std::vector<std::uint8_t> bytes = Read(name);
        if (bytes.size() < 8)
        {
            return {};
        }
        std::uint32_t width = 0;
        std::uint32_t height = 0;
        std::memcpy(&width, bytes.data(), 4);
        std::memcpy(&height, bytes.data() + 4, 4);
        return easyforge::ImageData(static_cast<int>(width), static_cast<int>(height),
            std::vector<std::uint8_t>(bytes.begin() + 8, bytes.end()));
    }

    // A .f32 file: 32-bit floats.
    inline std::vector<float> Floats(const std::string& name)
    {
        std::vector<std::uint8_t> bytes = Read(name);
        std::vector<float> values(bytes.size() / 4);
        std::memcpy(values.data(), bytes.data(), values.size() * 4);
        return values;
    }

    // The largest difference between two images' bytes, or -1 when their sizes differ.
    inline int LargestDifference(const easyforge::ImageData& first, const easyforge::ImageData& second)
    {
        if (first.Width != second.Width || first.Height != second.Height || first.Pixels.size() != second.Pixels.size())
        {
            return -1;
        }
        int largest = 0;
        for (std::size_t index = 0; index < first.Pixels.size(); ++index)
        {
            int difference = first.Pixels[index] > second.Pixels[index] ? first.Pixels[index] - second.Pixels[index]
                                                                        : second.Pixels[index] - first.Pixels[index];
            largest = difference > largest ? difference : largest;
        }
        return largest;
    }

    // A clean folder for files a test writes.
    inline std::string OutputFolder(const std::string& name)
    {
        std::filesystem::path folder = std::filesystem::path(EASYFORGE_TEST_OUTPUT) / name;
        std::filesystem::remove_all(folder);
        std::filesystem::create_directories(folder);
        return folder.generic_string() + "/";
    }
}
