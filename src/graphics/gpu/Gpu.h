#pragma once

// The layer between the renderer and a graphics API. Each backend (Direct3D 12
// now, Vulkan, Metal, and WebGPU later) implements these classes, and the
// renderer is written once on top of them. The shape follows WebGPU, the most
// limited of the four, so every backend can provide it.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <easyforge/assets/ImageData.h>
#include <easyforge/core/Color.h>
#include <easyforge/core/Host.h>
#include <easyforge/core/Result.h>
#include <easyforge/graphics/Renderer.h>

namespace easyforge::internal::gpu
{
    enum class TextureFormat
    {
        // Red, green, blue, alpha, a byte each.
        Rgba8,

        // One byte, read as red: coverage for glyphs.
        R8,

        // The format of a window's surface.
        Surface,

        Depth32,
    };

    struct TextureSettings
    {
        int Width = 1;
        int Height = 1;
        TextureFormat Format = TextureFormat::Rgba8;

        // Drawn into by a pass.
        bool RenderTarget = false;

        // Samples per pixel, for anti-aliasing a render target. More than one
        // cannot be read by a shader; it is resolved into another texture.
        int SampleCount = 1;

        std::string_view Name;
    };

    class Texture
    {
    public:
        virtual ~Texture() = default;
        virtual int Width() const = 0;
        virtual int Height() const = 0;
        virtual TextureFormat Format() const = 0;
    };

    // Data that does not change after it is made, such as a model's vertices.
    class Buffer
    {
    public:
        virtual ~Buffer() = default;
        virtual std::size_t Size() const = 0;
    };

    enum class VertexFormat
    {
        Float1,
        Float2,
        Float3,
        Float4,
    };

    struct VertexAttribute
    {
        // The semantic name the shader uses, such as "POSITION".
        std::string_view Name;
        VertexFormat Format = VertexFormat::Float4;
        std::uint32_t Offset = 0;
    };

    enum class Blending
    {
        None,

        // Colors come out of the shader already multiplied by their alpha.
        Premultiplied,
    };

    enum class Sampling
    {
        LinearClamp,
        LinearRepeat,
        NearestClamp,
        NearestRepeat,
    };

    struct PipelineSettings
    {
        // Source code in the backend's own shading language: HLSL for Direct3D 12.
        std::string_view VertexShader;
        std::string_view PixelShader;
        std::string_view VertexEntry = "VertexMain";
        std::string_view PixelEntry = "PixelMain";

        std::span<const VertexAttribute> Attributes;
        std::uint32_t VertexStride = 0;

        // Attributes advance once per instance instead of once per vertex.
        bool PerInstance = false;

        // Triangle strips draw quads from four vertices.
        bool TriangleStrip = false;

        Blending Blend = Blending::None;
        TextureFormat ColorFormat = TextureFormat::Surface;
        bool DepthTest = false;
        bool CullBackFaces = false;
        int SampleCount = 1;
        std::string_view Name;
    };

    class Pipeline
    {
    public:
        virtual ~Pipeline() = default;
    };

    struct PassSettings
    {
        Texture* ColorTarget = nullptr;
        bool ClearColor = true;
        Color Clear = Color::Transparent;

        Texture* DepthTarget = nullptr;

        // A single-sample texture the multisampled color target is resolved into
        // when the pass ends.
        Texture* ResolveTarget = nullptr;
    };

    struct ScissorRectangle
    {
        int X = 0;
        int Y = 0;
        int Width = 0;
        int Height = 0;
    };

    // The commands of one frame. Everything set stays set until the pass ends.
    class Commands
    {
    public:
        virtual ~Commands() = default;

        virtual void BeginPass(const PassSettings& settings) = 0;
        virtual void EndPass() = 0;

        virtual void SetPipeline(Pipeline& pipeline) = 0;

        // Up to four textures, as t0 to t3 in the shaders, and how to sample them.
        virtual void SetTextures(std::span<Texture* const> textures, Sampling sampling) = 0;

        // Constants for the next draws, as the constant buffer b0. At most 4096 bytes.
        virtual void SetConstants(const void* data, std::size_t size) = 0;

        // Vertex data used only this frame, copied now.
        virtual void SetVertices(const void* data, std::size_t size) = 0;
        virtual void SetVertexBuffer(Buffer& buffer) = 0;
        virtual void SetIndexBuffer(Buffer& buffer) = 0;

        virtual void SetScissor(ScissorRectangle rectangle) = 0;

        // Copies part of a texture into another of the same format, outside a pass.
        virtual void CopyTexture(Texture& source, ScissorRectangle area, Texture& destination, int x, int y) = 0;

        virtual void Draw(std::uint32_t vertexCount, std::uint32_t instanceCount = 1, std::uint32_t firstVertex = 0,
            std::uint32_t firstInstance = 0) = 0;
        virtual void DrawIndexed(std::uint32_t indexCount, std::uint32_t firstIndex = 0, std::int32_t baseVertex = 0) = 0;
    };

    // What a window shows: a chain of images the device draws into and the
    // system displays.
    class Surface
    {
    public:
        virtual ~Surface() = default;

        // Matches the surface to the window's size in pixels.
        virtual void Resize(int width, int height) = 0;
        virtual int Width() const = 0;
        virtual int Height() const = 0;

        // The image to draw into this frame.
        virtual Texture& CurrentTexture() = 0;

        // Reads what was shown last, or an empty image before the first frame.
        virtual ImageData ReadLastShown() = 0;
    };

    // One renderer's frames: its commands, and the memory for data that lives
    // only for a frame. Two frames can be in flight at once.
    class Context
    {
    public:
        virtual ~Context() = default;

        // Waits until the frame that last used this slot is finished on the GPU.
        virtual Commands& BeginFrame() = 0;

        // Sends the frame to the GPU, then shows the surface, if one is given.
        virtual void EndFrame(Surface* surface, bool verticalSync) = 0;

        // Waits until everything this context sent is finished.
        virtual void WaitUntilIdle() = 0;
    };

    class Device
    {
    public:
        virtual ~Device() = default;

        // The graphics API and the adapter: "Direct3D 12 on Intel(R) HD Graphics 620".
        virtual std::string Description() const = 0;

        virtual std::unique_ptr<Texture> CreateTexture(const TextureSettings& settings) = 0;

        // Copies pixels into part of a texture. `stride` is the bytes from one row
        // of `pixels` to the next. The copy reaches the GPU before any frame that
        // is sent afterwards.
        virtual void UploadTexture(Texture& texture, int x, int y, int width, int height, const std::uint8_t* pixels,
            std::size_t stride) = 0;

        virtual std::unique_ptr<Buffer> CreateBuffer(std::span<const std::uint8_t> data, bool indices, std::string_view name) = 0;

        virtual Result<std::unique_ptr<Pipeline>> CreatePipeline(const PipelineSettings& settings) = 0;

        virtual Result<std::unique_ptr<Surface>> CreateSurface(const easyforge::Surface& native, int width, int height,
            bool transparent) = 0;

        virtual std::unique_ptr<Context> CreateContext() = 0;

        // Reads a single-sample texture back into memory, waiting for the GPU.
        virtual ImageData ReadTexture(Texture& texture) = 0;

        // A number no other device in this run of the program has, to keep
        // per-device copies of textures and fonts apart.
        virtual std::uint64_t Identifier() const = 0;
    };

    // The device for an adapter, shared by every renderer that asks for the same
    // adapter while any of them exists.
    Result<std::shared_ptr<Device>> SharedDevice(GraphicsAdapter adapter);

    // The renderer's own shaders, in the backend's shading language.
    struct BuiltInShaders
    {
        std::string_view Shape;
        std::string_view Mesh;
        std::string_view Blur;
    };

    BuiltInShaders ShadersOfBackend();
}
