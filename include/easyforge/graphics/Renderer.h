#pragma once

#include <memory>
#include <string>

#include <easyforge/assets/ImageData.h>
#include <easyforge/core/Color.h>
#include <easyforge/core/Host.h>
#include <easyforge/core/Vector.h>
#include <easyforge/graphics/Canvas.h>

namespace easyforge
{
    namespace internal
    {
        class RendererState;
    }

    // Which graphics hardware to draw with.
    enum class GraphicsAdapter
    {
        // The fastest GPU, usually the separate graphics card in a laptop that has two.
        HighPerformance,

        // The GPU that uses the least power, usually the one built into the processor.
        LowPower,

        // The system's software renderer, which runs on the processor. It is
        // slow, and draws exactly the same pixels on every computer, which is
        // what tests want.
        Software,
    };

    struct RendererSettings
    {
        GraphicsAdapter Adapter = GraphicsAdapter::HighPerformance;

        // For a renderer without a host: the size of the image it draws, in
        // pixels, and how many pixels make a point.
        int Width = 0;
        int Height = 0;
        float Scale = 1.0f;
    };

    // Draws into a window, or into an image in memory.
    //
    //     Renderer renderer = Renderer::New(window);
    //     window.OnFrame = [&](float deltaSeconds) {
    //         Canvas canvas = renderer.BeginFrame(Color::Hex("#1A1A20"));
    //         canvas.Rectangle({ .Position = { 100, 100 }, .Size = { 200, 80 }, .Color = Color::White });
    //         renderer.EndFrame();
    //     };
    //
    // A window has at most one renderer. Renderers share one GPU device for each
    // adapter, so textures and fonts are sent to the GPU once however many
    // windows draw them.
    //
    // Renderer is a handle: copies refer to the same renderer.
    class Renderer
    {
    public:
        // A renderer that draws into the host, such as a window.
        static Renderer New(std::shared_ptr<Host> host, const RendererSettings& settings = {});

        // A renderer that draws into an image of Width by Height pixels, read with Capture.
        static Renderer New(const RendererSettings& settings);

        // Not a renderer. Tests as false.
        Renderer();

        explicit operator bool() const;

        // Why making the renderer failed, or empty.
        const std::string& Error() const;

        // Starts a frame, clearing it to `clear`. Draw with the canvas, then call
        // EndFrame.
        Canvas BeginFrame(Color clear = Color::Black) const;

        // Sends the frame to the GPU and shows it.
        void EndFrame() const;

        // The last finished frame, read back from the GPU.
        ImageData Capture() const;

        // The size of what is drawn into, in points and in pixels, and pixels per point.
        Vector2 Size() const;
        Vector2 PixelSize() const;
        float Scale() const;

        // The graphics API and the adapter, such as "Direct3D 12 on Intel(R) HD Graphics 620".
        std::string Description() const;

    private:
        explicit Renderer(std::shared_ptr<internal::RendererState> state);

        std::shared_ptr<internal::RendererState> State;
    };
}
