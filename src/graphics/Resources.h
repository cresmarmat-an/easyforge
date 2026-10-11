#pragma once

// What the handles in the public headers share, and the GPU copies made of them.

#include <compare>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <easyforge/core/Vector.h>
#include <easyforge/graphics/Font.h>
#include <easyforge/graphics/Model.h>
#include <easyforge/graphics/Scene.h>
#include <easyforge/graphics/Shader.h>
#include <easyforge/graphics/Texture.h>

#include "gpu/Gpu.h"
#include "shaders/ShaderCompiler.h"

namespace easyforge::internal
{
    // Objects made from files, shared by everything that names the same file
    // while any of them is in use. Safe to use from several threads.
    template <typename State>
    class SharedByPath
    {
    public:
        // The object for `path`: the one in use, or a new one from `make`, kept
        // for others when `keep` says it was made well.
        template <typename Make, typename Keep>
        std::shared_ptr<State> Find(const std::string& path, Make make, Keep keep)
        {
            std::scoped_lock lock(Mutex);
            auto found = Objects.find(path);
            if (found != Objects.end())
            {
                if (std::shared_ptr<State> state = found->second.lock())
                {
                    return state;
                }
            }
            std::shared_ptr<State> state = make();
            std::erase_if(Objects, [](const auto& entry) { return entry.second.expired(); });
            if (keep(*state))
            {
                Objects[path] = state;
            }
            return state;
        }

    private:
        std::mutex Mutex;
        std::map<std::string, std::weak_ptr<State>> Objects;
    };

    // A GPU copy of something, made for one device. The device is kept alive by
    // its copies, and destroyed after them.
    template <typename Copy>
    struct DeviceCopy
    {
        std::shared_ptr<gpu::Device> Device;
        std::unique_ptr<Copy> Object;
        std::uint64_t Version = 0;
    };

    // Lets go of copies made on a device that was lost, so the device can be
    // freed. Called when a copy for another device is about to be made.
    template <typename Copies>
    void ForgetLostDevices(Copies& copies)
    {
        std::erase_if(copies, [](const auto& entry) { return entry.second.Device && entry.second.Device->IsLost(); });
    }

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

    // A text laid out in its font's own units: each glyph with its pen position
    // on the baseline, and the size of the whole. Any size is this times the
    // size over the font's units per em.
    struct TextRun
    {
        std::vector<std::pair<int, Vector2>> Glyphs;
        Vector2 Size;
    };

    class FontState
    {
    public:
        FontData Data;
        std::string ErrorText;
        std::uint64_t Identifier = 0;

        // The text laid out, from the runs kept when it was laid out before.
        // An interface draws and measures the same texts frame after frame,
        // and this skips the font's tables for them. Safe from any thread.
        std::shared_ptr<const TextRun> Run(std::string_view text) const;

    private:
        // Lookups by string_view without making a string.
        struct TextHash
        {
            using is_transparent = void;
            std::size_t operator()(std::string_view text) const { return std::hash<std::string_view> {}(text); }
        };

        // Forgotten all at once when there are more than this many.
        static constexpr std::size_t MostRuns = 4096;

        mutable std::mutex RunsMutex;
        mutable std::unordered_map<std::string, std::shared_ptr<const TextRun>, TextHash, std::equal_to<>> Runs;
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
            bool operator==(const Key&) const = default;
        };

        struct KeyHash
        {
            std::size_t operator()(const Key& key) const
            {
                std::uint64_t mixed = key.Font * 0x9E3779B97F4A7C15ull;
                mixed ^= (static_cast<std::uint64_t>(static_cast<std::uint32_t>(key.Glyph)) << 32) |
                         static_cast<std::uint32_t>(key.Size);
                mixed ^= mixed >> 29;
                mixed *= 0xBF58476D1CE4E5B9ull;
                mixed ^= mixed >> 32;
                return static_cast<std::size_t>(mixed);
            }
        };

        std::shared_ptr<gpu::Device> Device;
        std::vector<std::unique_ptr<gpu::Texture>> Pages;
        int RowX = 0;
        int RowY = 0;
        int RowHeight = 0;
        // Looked up for every glyph drawn, so hashed rather than sorted. What
        // Find returns stays where it is as the table grows.
        std::unordered_map<Key, GlyphPlacement, KeyHash> Glyphs;
    };

    // What every renderer on one device shares.
    class DeviceResources
    {
    public:
        explicit DeviceResources(const std::shared_ptr<gpu::Device>& device) : Device(device), Atlas(device) {}

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
