#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <easyforge/core/Clock.h>
#include <easyforge/graphics/Renderer.h>

#include "Frame.h"

namespace easyforge::internal
{
    class RendererState
    {
    public:
        RendererState() = default;
        ~RendererState();

        Result<> Start(std::shared_ptr<Host> host, const RendererSettings& settings);

        Vector2 PixelSize() const;
        float Scale() const;

        void Finish();

        // The texture a scene drawn this frame goes into, made or resized as needed.
        gpu::Texture* SceneTarget(std::size_t index, int width, int height);

        // The same for a layer's picture.
        gpu::Texture* LayerTarget(std::size_t index, int width, int height);

        // The pictures a blur behind an area is made in: two for the blur, and one
        // for what was there before.
        std::array<gpu::Texture*, 3> BlurTargets(std::size_t index, int width, int height);

        std::string ErrorText;
        bool Made = false;
        std::shared_ptr<Host> TheHost;
        bool ClaimedSurface = false;
        RendererSettings Settings;

        std::shared_ptr<gpu::Device> Device;
        std::shared_ptr<DeviceResources> Resources;
        std::unique_ptr<gpu::Context> Context;
        std::unique_ptr<gpu::Surface> Surface;
        std::unique_ptr<gpu::Texture> Offscreen;
        std::unique_ptr<gpu::Pipeline> ShapePipeline;
        std::unique_ptr<gpu::Pipeline> MeshPipeline;
        std::unique_ptr<gpu::Pipeline> BlurPipeline;

        // The shape shader without blending, for shapes that replace what is under them.
        std::unique_ptr<gpu::Pipeline> ReplacePipeline;

        struct SceneTargets
        {
            std::unique_ptr<gpu::Texture> Color;
            std::unique_ptr<gpu::Texture> Depth;
            std::unique_ptr<gpu::Texture> Resolved;
        };
        std::vector<SceneTargets> SceneTargetPool;

        // Layer and blur pictures, kept between frames, and let go after going
        // unused for a while.
        struct PooledPicture
        {
            std::unique_ptr<gpu::Texture> Picture;
            std::uint64_t LastFrame = 0;
        };
        std::vector<PooledPicture> LayerTargetPool;
        std::vector<std::array<PooledPicture, 3>> BlurTargetPool;
        std::uint64_t FrameNumber = 0;

        // Blurs made so far this frame.
        std::size_t BlursRecorded = 0;

        // Time since the renderer was made, which shaders read as input.Time.
        Clock SinceStart;

        FrameRecorder Frame { *this };
        Color Clear;

    private:
        void DrawScene(gpu::Commands& commands, const ScenePass& pass, SceneTargets& targets);
        void DrawRecorded(gpu::Commands& commands, const internal::DrawList& list, gpu::Texture& target, Color clear);
        void DrawBlur(gpu::Commands& commands, const DrawStep& step, gpu::Texture& target);
        void LetGoOfUnusedPictures();
    };

    // Samples per pixel for scenes, which smooths the edges of models.
    inline constexpr int SceneSamples = 4;
}
