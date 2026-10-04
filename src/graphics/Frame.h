#pragma once

// What a Canvas records during a frame, for the renderer to draw at its end.

#include <cstdint>
#include <memory>
#include <vector>

#include <easyforge/core/Matrix.h>
#include <easyforge/graphics/Canvas.h>

#include "Resources.h"

namespace easyforge::internal
{
    class RendererState;

    // One 2D shape as the shape shader reads it. The layout matches the
    // shader's Instance structure.
    struct ShapeInstance
    {
        float Origin[2] {};
        float AxisX[2] { 1.0f, 0.0f };
        float AxisY[2] { 0.0f, 1.0f };
        float Size[2] {};
        float Coordinates[4] {};
        float Fill[4] {};
        float Border[4] {};

        // Corner radius, border width, mode, and how many pixels the edge is
        // softened over.
        float Shape[4] {};

        // The clip area in pixels: left, top, right, bottom.
        float Clip[4] {};

        // A gradient's last color, and the line it runs along in the shape's own
        // pixels: start X and Y, end X and Y. A line of no length means no gradient.
        float Gradient[4] {};
        float GradientLine[4] {};
    };

    enum class ShapeMode
    {
        Solid = 0,
        Picture = 1,
        Glyph = 2,
        PremultipliedPicture = 3,
        Backdrop = 4,

        // Solid, without the rounded rectangle given in place of a gradient:
        // GradientLine holds its corner and size in the shape's pixels, and
        // Gradient its corner radius first.
        Holed = 5,
    };

    // One step of drawing a list: shapes in a row that use the same texture, or
    // a custom shader over an area.
    struct DrawStep
    {
        // Shapes.
        gpu::Texture* Texture = nullptr;
        gpu::Sampling Sampling = gpu::Sampling::LinearClamp;
        std::uint32_t First = 0;
        std::uint32_t Count = 0;

        // A shader, when Pipeline is set; Texture is then its content.
        gpu::Pipeline* Pipeline = nullptr;
        std::vector<std::uint8_t> Constants;

        // A blur of what the target holds under BlurArea (target pixels), made
        // in the first two pictures of BlurTargets and drawn back by shape First.
        // The third keeps what was there before.
        bool Backdrop = false;
        Rectangle BlurArea;
        float BlurRadius = 0.0f;
        gpu::Texture* BlurTargets[3] {};
    };

    // What is drawn into one target: the frame itself, or a layer's picture.
    struct DrawList
    {
        std::vector<ShapeInstance> Instances;
        std::vector<DrawStep> Steps;

        // Where the target is in the frame, in pixels, and its size.
        Vector2 Origin;
        Vector2 Size;

        // The layer's picture; null for the frame.
        gpu::Texture* Target = nullptr;
    };

    // A scene drawn into its own texture before the 2D shapes, as it was when
    // Canvas::Draw was called.
    struct ScenePass
    {
        struct Object
        {
            std::shared_ptr<ModelState> Model;
            Matrix4 World;
        };

        std::shared_ptr<SceneState> Scene;
        Camera ViewPoint;
        Sun Light;
        Color Ambient;
        Color Background;
        std::vector<Object> Objects;
        int Width = 0;
        int Height = 0;
    };

    class FrameRecorder
    {
    public:
        explicit FrameRecorder(RendererState& owner) : Owner(owner) {}

        void Begin(Vector2 pixelSize, float scale, float time);

        // Adds a shape to the list being drawn, joining the last step when it uses
        // the same texture. Positions are in frame pixels.
        void Add(ShapeInstance instance, gpu::Texture* texture, gpu::Sampling sampling);

        // Adds a custom shader step over an area given in frame pixels.
        // A content picture can be larger than the area; `contentShare` is the part
        // of it the area fills, from the top left.
        void AddShader(ShaderState& shader, Rectangle area, float pixelsPerPoint, gpu::Texture* content,
            const std::vector<ShaderValue>& values, Vector2 contentShare = { 1.0f, 1.0f });

        // Points, after the transforms, to frame pixels.
        Vector2 ToPixels(Vector2 point) const;
        float PixelsPerPoint() const;

        DrawList& Current() { return Lists[ListStack.back()]; }

        RendererState& Owner;
        bool Active = false;
        Vector2 PixelSize;
        float Scale = 1.0f;
        float Time = 0.0f;

        // The frame's list first, then each layer's.
        std::vector<DrawList> Lists;
        std::vector<std::size_t> ListStack;

        // Layers in the order they ended, which draws each before anything that uses it.
        std::vector<std::size_t> FinishedLayers;
        std::vector<Rectangle> LayerAreas;

        std::vector<ScenePass> Scenes;

        struct Placement
        {
            Vector2 Offset;
            float Scale = 1.0f;
        };
        std::vector<Placement> Transforms;
        std::vector<Rectangle> Clips;

        // Everything drawn this frame stays alive until the frame is sent.
        std::vector<std::shared_ptr<void>> Used;
    };
}
