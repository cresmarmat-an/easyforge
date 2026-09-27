#pragma once

#include <cstdint>
#include <span>

#include <easyforge/assets/ImageData.h>
#include <easyforge/core/Result.h>

namespace easyforge::internal
{
    Result<ImageData> DecodePng(std::span<const std::uint8_t> bytes);
    Result<ImageData> DecodeJpeg(std::span<const std::uint8_t> bytes);
    Result<ImageData> DecodeBmp(std::span<const std::uint8_t> bytes);
    Result<ImageData> DecodeTga(std::span<const std::uint8_t> bytes);
    Result<ImageData> DecodeQoi(std::span<const std::uint8_t> bytes);

    // Checks a size read from a file before any memory is reserved for it.
    Result<> CheckImageSize(std::int64_t width, std::int64_t height);
}
