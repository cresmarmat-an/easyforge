#pragma once

// What the handles in the public headers share, and the GPU copies made of them.

#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <easyforge/graphics/Font.h>
#include <easyforge/graphics/Model.h>
#include <easyforge/graphics/Scene.h>
#include <easyforge/graphics/Shader.h>
#include <easyforge/graphics/Texture.h>

#include "gpu/Gpu.h"
#include "shaders/ShaderCompiler.h"

namespace easyforge::internal
{
    // A GPU copy of something, made for one device. The device is kept alive by
    // its copies, and destroyed after them.
    template <typename Copy>
    struct DeviceCopy
    {
        std::shared_ptr<gpu::Device> Device;
        std::unique_ptr<Copy> Object;
        std::uint64_t Version = 0;
    };

    class TextureState
    {
    public:
        ImageData Image;
        TextureSettings Settings;
        std::string ErrorText;

        // Goes up each time the pixels change, so copies know to update.
        std::uint64_t Version = 1;

        // The texture on a device, sent or updated first if needed. Null for an
        // empty texture.
        gpu::Texture* On(const std::shared_ptr<gpu::Device>& device);

        gpu::Sampling Sampling() const;

    private:
        std::map<std::uint64_t, DeviceCopy<gpu::Texture>> Copies;
    };

    class FontState
    {
    public:
        FontData Data;
        std::string ErrorText;
        std::uint64_t Identifier = 0;
    };

    // Where a glyph at one size is in the atlas.
    struct GlyphPlacement
    {
        int Page = -1;
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;

        // Pixels from the pen position to the bitmap's left edge, and from the
        // baseline up to its top edge.
        int Left = 0;
        int Top = 0;
    };

    // Glyphs of every font and size, drawn once and kept in large one-channel
    // textures, packed in rows.
    class GlyphAtlas
    {
    public:
        static constexpr int PageSize = 2048;

        explicit GlyphAtlas(std::shared_ptr<gpu::Device> device) : Device(std::move(device)) {}

        const GlyphPlacement& Find(const FontState& font, int glyph, float pixelsPerEm);
        gpu::Texture* Page(int index) const;

    private:
        struct Key
        {
            std::uint64_t Font = 0;
            int Glyph = 0;
            int Size = 0;
            auto operator<=>(const Key&) const = default;
        };

        std::shared_ptr<gpu::Device> Device;
        std::vector<std::unique_ptr<gpu::Texture>> Pages;
        int RowX = 0;
        int RowY = 0;
        int RowHeight = 0;
        std::map<Key, GlyphPlacement> Glyphs;
    };

    // What every renderer on one device shares.
    class DeviceResources
    {
    public:
        explicit DeviceResources(std::shared_ptr<gpu::Device> device) : Device(device), Atlas(device) {}

        static std::shared_ptr<DeviceResources> For(const std::shared_ptr<gpu::Device>& device);

        std::shared_ptr<gpu::Device> Device;
        GlyphAtlas Atlas;
    };

    struct GpuMesh
    {
        std::unique_ptr<gpu::Buffer> Vertices;
        std::unique_ptr<gpu::Buffer> Indices;
        std::uint32_t IndexCount = 0;
        int MaterialIndex = -1;
    };

    struct GpuModel
    {
        std::shared_ptr<gpu::Device> Device;
        std::vector<GpuMesh> Meshes;
    };

    class ModelState
    {
    public:
        ModelData Data;
        std::string ErrorText;

        // One per material; empty where the material has no texture.
        std::vector<Texture> MaterialTextures;

        const GpuModel& On(const std::shared_ptr<gpu::Device>& device);

    private:
        std::map<std::uint64_t, GpuModel> Copies;
    };

    class ShaderState
    {
    public:
        std::string Name;
        std::string ErrorText;
        bool Made = false;
        CompiledShader Compiled;

        // The shader's pipeline on a device, made the first time it is drawn.
        // Null, with the problem logged once, if the device refuses it.
        gpu::Pipeline* On(const std::shared_ptr<gpu::Device>& device);

        // The constants the shader reads: the frame's, then each value, with the
        // given values in place of the starting ones.
        std::vector<std::uint8_t> Constants(const ShaderFrameConstants& frame, const std::vector<ShaderValue>& values) const;

    private:
        std::map<std::uint64_t, DeviceCopy<gpu::Pipeline>> Pipelines;
        bool Refused = false;
    };

    struct SceneObjectRecord
    {
        std::shared_ptr<ModelState> Model;
        Vector3 Position;
        Quaternion Rotation;
        Vector3 Scale { 1.0f, 1.0f, 1.0f };
        bool Visible = true;
        bool InScene = false;
    };

    class SceneState
    {
    public:
        bool Made = false;
        std::vector<std::shared_ptr<SceneObjectRecord>> Objects;
        easyforge::Camera ViewPoint;
        easyforge::Sun Light;
        Color Ambient = Color::Hex("#3A3F4A");
        Color Background = Color::Hex("#20242C");
    };
}
