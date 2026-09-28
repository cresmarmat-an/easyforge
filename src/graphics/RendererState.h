#pragma once

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

        struct SceneTargets
        {
            std::unique_ptr<gpu::Texture> Color;
            std::unique_ptr<gpu::Texture> Depth;
            std::unique_ptr<gpu::Texture> Resolved;
        };
        std::vector<SceneTargets> SceneTargetPool;
        std::vector<std::unique_ptr<gpu::Texture>> LayerTargetPool;

        // Time since the renderer was made, which shaders read as input.Time.
        Clock SinceStart;

        FrameRecorder Frame { *this };
        Color Clear;

    private:
        void DrawScene(gpu::Commands& commands, const ScenePass& pass, SceneTargets& targets);
        void DrawRecorded(gpu::Commands& commands, const internal::DrawList& list, gpu::Texture& target, Color clear);
    };

    // Samples per pixel for scenes, which smooths the edges of models.
    inline constexpr int SceneSamples = 4;
}
