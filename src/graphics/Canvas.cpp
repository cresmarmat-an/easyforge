#include <easyforge/graphics/Canvas.h>

#include <array>
#include <cmath>

#include <easyforge/core/Log.h>
#include <easyforge/core/Scalar.h>
#include <easyforge/core/Transform.h>
#include <easyforge/graphics/Font.h>
#include <easyforge/graphics/Scene.h>
#include <easyforge/graphics/Texture.h>

#include "RendererState.h"
#include "Text.h"

namespace easyforge
{
    namespace internal
    {
        void FrameRecorder::Begin(Vector2 pixelSize, float scale, float time)
        {
            Active = true;
            PixelSize = pixelSize;
            Scale = scale;
            Time = time;
            Lists.assign(1, DrawList {});
            Lists[0].Size = pixelSize;
            ListStack.assign(1, 0);
            FinishedLayers.clear();
            LayerAreas.clear();
            Scenes.clear();
            Used.clear();
            Transforms.assign(1, Placement {});
            Clips.assign(1, Rectangle { 0.0f, 0.0f, pixelSize.X, pixelSize.Y });
        }

        void FrameRecorder::Add(ShapeInstance instance, gpu::Texture* texture, gpu::Sampling sampling)
        {
            // The list's target starts at its origin in the frame.
            DrawList& list = Current();
            const Rectangle& clip = Clips.back();
            instance.Origin[0] -= list.Origin.X;
            instance.Origin[1] -= list.Origin.Y;
            instance.Clip[0] = clip.X - list.Origin.X;
            instance.Clip[1] = clip.Y - list.Origin.Y;
            instance.Clip[2] = clip.Right() - list.Origin.X;
            instance.Clip[3] = clip.Bottom() - list.Origin.Y;

            std::uint32_t index = static_cast<std::uint32_t>(list.Instances.size());
            list.Instances.push_back(instance);
            if (!list.Steps.empty() && !list.Steps.back().Pipeline && !list.Steps.back().Backdrop &&
                list.Steps.back().Texture == texture && list.Steps.back().Sampling == sampling)
            {
                ++list.Steps.back().Count;
                return;
            }
            DrawStep step;
            step.Texture = texture;
            step.Sampling = sampling;
            step.First = index;
            step.Count = 1;
            list.Steps.push_back(std::move(step));
        }

        void FrameRecorder::AddShader(ShaderState& shader, Rectangle area, float pixelsPerPoint, gpu::Texture* content,
            const std::vector<ShaderValue>& values)
        {
            gpu::Pipeline* pipeline = shader.On(Owner.Device);
            if (!pipeline)
            {
                return;
            }
            DrawList& list = Current();
            const Rectangle& clip = Clips.back();
            ShaderFrameConstants frame;
            frame.TargetSize[0] = list.Size.X;
            frame.TargetSize[1] = list.Size.Y;
            frame.AreaOrigin[0] = area.X - list.Origin.X;
            frame.AreaOrigin[1] = area.Y - list.Origin.Y;
            frame.AreaSize[0] = area.Width;
            frame.AreaSize[1] = area.Height;
            frame.PointSize[0] = area.Width / pixelsPerPoint;
            frame.PointSize[1] = area.Height / pixelsPerPoint;
            frame.Clip[0] = clip.X - list.Origin.X;
            frame.Clip[1] = clip.Y - list.Origin.Y;
            frame.Clip[2] = clip.Right() - list.Origin.X;
            frame.Clip[3] = clip.Bottom() - list.Origin.Y;
            frame.Time = Time;
            frame.Scale = pixelsPerPoint;
            frame.HasContent = content ? 1.0f : 0.0f;

            DrawStep step;
            step.Pipeline = pipeline;
            step.Texture = content;
            step.Constants = shader.Constants(frame, values);
            list.Steps.push_back(std::move(step));
        }

        Vector2 FrameRecorder::ToPixels(Vector2 point) const
        {
            const Placement& placement = Transforms.back();
            return (placement.Offset + point * placement.Scale) * Scale;
        }

        float FrameRecorder::PixelsPerPoint() const
        {
            return Transforms.back().Scale * Scale;
        }

        namespace
        {
            void StoreColor(float (&target)[4], Color color)
            {
                target[0] = color.Red;
                target[1] = color.Green;
                target[2] = color.Blue;
                target[3] = color.Alpha;
            }

            ShapeInstance Shape(Vector2 origin, Vector2 size, Color fill, float radius, float borderWidth, Color border,
                ShapeMode mode)
            {
                ShapeInstance instance;
                instance.Origin[0] = origin.X;
                instance.Origin[1] = origin.Y;
                instance.Size[0] = size.X;
                instance.Size[1] = size.Y;
                StoreColor(instance.Fill, fill);
                StoreColor(instance.Border, border);
                instance.Shape[0] = Max(radius, 0.0f);
                instance.Shape[1] = Max(borderWidth, 0.0f);
                instance.Shape[2] = static_cast<float>(mode);
                return instance;
            }

            // Gives a shape of `size` pixels a gradient along `angle` degrees, with
            // the first and last colors on the farthest corners.
            void ApplyGradient(ShapeInstance& instance, const LinearGradient& gradient, Vector2 size)
            {
                float angle = Radians(gradient.Angle);
                Vector2 direction { std::cos(angle), std::sin(angle) };
                float reach = std::abs(size.X * 0.5f * direction.X) + std::abs(size.Y * 0.5f * direction.Y);
                Vector2 center = size * 0.5f;
                Vector2 start = center - direction * reach;
                Vector2 end = center + direction * reach;
                StoreColor(instance.Fill, gradient.From);
                StoreColor(instance.Gradient, gradient.To);
                instance.GradientLine[0] = start.X;
                instance.GradientLine[1] = start.Y;
                instance.GradientLine[2] = end.X;
                instance.GradientLine[3] = end.Y;
            }
        }
    }

    using internal::FrameRecorder;
    using internal::ShapeInstance;
    using internal::ShapeMode;

    void Canvas::Rectangle(const RectangleStyle& style) const
    {
        if (!Recorder || !Recorder->Active || style.Size.X <= 0.0f || style.Size.Y <= 0.0f)
        {
            return;
        }
        float scale = Recorder->PixelsPerPoint();
        ShapeInstance instance = internal::Shape(Recorder->ToPixels(style.Position), style.Size * scale, style.Color,
            style.CornerRadius * scale, style.BorderWidth * scale, style.BorderColor, ShapeMode::Solid);
        if (style.Gradient)
        {
            internal::ApplyGradient(instance, *style.Gradient, style.Size * scale);
        }
        instance.Shape[3] = Max(style.Blur, 0.0f) * scale;
        Recorder->Add(instance, nullptr, internal::gpu::Sampling::LinearClamp);
    }

    void Canvas::Circle(Vector2 center, float radius, const CircleStyle& style) const
    {
        if (!Recorder || !Recorder->Active || radius <= 0.0f)
        {
            return;
        }
        float scale = Recorder->PixelsPerPoint();
        Vector2 corner = Recorder->ToPixels(center - Vector2 { radius, radius });
        float diameter = radius * 2.0f * scale;
        ShapeInstance instance = internal::Shape(corner, { diameter, diameter }, style.Color, radius * scale,
            style.BorderWidth * scale, style.BorderColor, ShapeMode::Solid);
        if (style.Gradient)
        {
            internal::ApplyGradient(instance, *style.Gradient, { diameter, diameter });
        }
        instance.Shape[3] = Max(style.Blur, 0.0f) * scale;
        Recorder->Add(instance, nullptr, internal::gpu::Sampling::LinearClamp);
    }

    void Canvas::Line(Vector2 from, Vector2 to, const LineStyle& style) const
    {
        if (!Recorder || !Recorder->Active || style.Width <= 0.0f)
        {
            return;
        }
        Vector2 start = Recorder->ToPixels(from);
        Vector2 end = Recorder->ToPixels(to);
        Vector2 along = end - start;
        float length = Length(along);
        if (length <= 0.0f)
        {
            return;
        }
        // A rectangle turned to lie along the line, as long as the line and as wide
        // as its width, centered on it.
        Vector2 direction = along / length;
        Vector2 across { -direction.Y, direction.X };
        float width = style.Width * Recorder->PixelsPerPoint();
        ShapeInstance instance = internal::Shape(start - across * (width * 0.5f), { length, width }, style.Color, 0.0f,
            0.0f, Color::Transparent, ShapeMode::Solid);
        instance.AxisX[0] = direction.X;
        instance.AxisX[1] = direction.Y;
        instance.AxisY[0] = across.X;
        instance.AxisY[1] = across.Y;
        Recorder->Add(instance, nullptr, internal::gpu::Sampling::LinearClamp);
    }

    void Canvas::Image(const Texture& texture, const ImageStyle& style) const
    {
        if (!Recorder || !Recorder->Active || !texture)
        {
            return;
        }
        const std::shared_ptr<internal::TextureState>& state = texture.State();
        internal::gpu::Texture* uploaded = state->On(Recorder->Owner.Device);
        if (!uploaded)
        {
            return;
        }
        Recorder->Used.push_back(state);

        easyforge::Rectangle source = style.Source;
        if (source.IsEmpty())
        {
            source = { 0.0f, 0.0f, static_cast<float>(texture.Width()), static_cast<float>(texture.Height()) };
        }
        Vector2 size = style.Size.X > 0.0f && style.Size.Y > 0.0f ? style.Size : source.Size();
        float scale = Recorder->PixelsPerPoint();
        float width = static_cast<float>(texture.Width());
        float height = static_cast<float>(texture.Height());

        if (style.Slice > 0.0f)
        {
            // Nine pieces: the corners keep their size, and the edges and middle
            // stretch. Each edge keeps at most half of the image and of the size.
            float sourceSlice = Min(style.Slice, Min(source.Width, source.Height) * 0.5f);
            float drawnSlice = Min(sourceSlice, Min(size.X, size.Y) * 0.5f);
            float sourceX[4] = { source.Left(), source.Left() + sourceSlice, source.Right() - sourceSlice, source.Right() };
            float sourceY[4] = { source.Top(), source.Top() + sourceSlice, source.Bottom() - sourceSlice, source.Bottom() };
            float drawnX[4] = { 0.0f, drawnSlice, size.X - drawnSlice, size.X };
            float drawnY[4] = { 0.0f, drawnSlice, size.Y - drawnSlice, size.Y };
            for (int row = 0; row < 3; ++row)
            {
                for (int column = 0; column < 3; ++column)
                {
                    Vector2 pieceSize { drawnX[column + 1] - drawnX[column], drawnY[row + 1] - drawnY[row] };
                    if (pieceSize.X <= 0.0f || pieceSize.Y <= 0.0f)
                    {
                        continue;
                    }
                    Vector2 corner = Recorder->ToPixels(style.Position + Vector2 { drawnX[column], drawnY[row] });
                    ShapeInstance piece = internal::Shape(corner, pieceSize * scale, style.Tint, 0.0f, 0.0f,
                        Color::Transparent, ShapeMode::Picture);
                    piece.Coordinates[0] = sourceX[column] / width;
                    piece.Coordinates[1] = sourceY[row] / height;
                    piece.Coordinates[2] = sourceX[column + 1] / width;
                    piece.Coordinates[3] = sourceY[row + 1] / height;
                    Recorder->Add(piece, uploaded, state->Sampling());
                }
            }
            return;
        }

        ShapeInstance instance = internal::Shape(Recorder->ToPixels(style.Position), size * scale, style.Tint,
            style.CornerRadius * scale, 0.0f, Color::Transparent, ShapeMode::Picture);
        instance.Coordinates[0] = source.Left() / width;
        instance.Coordinates[1] = source.Top() / height;
        instance.Coordinates[2] = source.Right() / width;
        instance.Coordinates[3] = source.Bottom() / height;
        Recorder->Add(instance, uploaded, state->Sampling());
    }

    void Canvas::Text(const Font& font, std::string_view text, const TextStyle& style) const
    {
        if (!Recorder || !Recorder->Active || !font || text.empty() || style.Size <= 0.0f)
        {
            return;
        }
        const std::shared_ptr<internal::FontState>& state = font.State();
        Recorder->Used.push_back(state);
        internal::GlyphAtlas& atlas = Recorder->Owner.Resources->Atlas;
        float pixelsPerEm = style.Size * Recorder->PixelsPerPoint();
        Vector2 origin = Recorder->ToPixels(style.Position);

        internal::LayOutText(state->Data, text, pixelsPerEm, [&](int glyph, Vector2 pen) {
            const internal::GlyphPlacement& placement = atlas.Find(*state, glyph, pixelsPerEm);
            if (placement.Width <= 0)
            {
                return;
            }
            // Glyphs sit on whole pixels, so they stay sharp.
            Vector2 corner { std::round(origin.X + pen.X) + static_cast<float>(placement.Left),
                std::round(origin.Y + pen.Y) - static_cast<float>(placement.Top) };
            ShapeInstance instance = internal::Shape(corner,
                { static_cast<float>(placement.Width), static_cast<float>(placement.Height) }, style.Color, 0.0f, 0.0f,
                Color::Transparent, ShapeMode::Glyph);
            float page = static_cast<float>(internal::GlyphAtlas::PageSize);
            instance.Coordinates[0] = static_cast<float>(placement.X) / page;
            instance.Coordinates[1] = static_cast<float>(placement.Y) / page;
            instance.Coordinates[2] = static_cast<float>(placement.X + placement.Width) / page;
            instance.Coordinates[3] = static_cast<float>(placement.Y + placement.Height) / page;
            Recorder->Add(instance, atlas.Page(placement.Page), internal::gpu::Sampling::NearestClamp);
        });
    }

    void Canvas::Draw(const Scene& scene) const
    {
        Draw(scene, { 0.0f, 0.0f, Size().X, Size().Y });
    }

    void Canvas::Draw(const Scene& scene, easyforge::Rectangle area) const
    {
        if (!Recorder || !Recorder->Active || !scene || area.IsEmpty())
        {
            return;
        }
        Vector2 corner = Recorder->ToPixels(area.Position());
        Vector2 size = area.Size() * Recorder->PixelsPerPoint();
        int width = static_cast<int>(std::lround(size.X));
        int height = static_cast<int>(std::lround(size.Y));
        if (width <= 0 || height <= 0)
        {
            return;
        }

        const std::shared_ptr<internal::SceneState>& state = scene.State();
        internal::ScenePass pass;
        pass.Scene = state;
        pass.ViewPoint = state->ViewPoint;
        pass.Light = state->Light;
        pass.Ambient = state->Ambient;
        pass.Background = state->Background;
        pass.Width = width;
        pass.Height = height;
        for (const std::shared_ptr<internal::SceneObjectRecord>& object : state->Objects)
        {
            if (object->Visible && object->Model)
            {
                Matrix4 world = Transform { object->Position, object->Rotation, object->Scale }.ToMatrix();
                pass.Objects.push_back({ object->Model, world });
            }
        }

        internal::gpu::Texture* picture = Recorder->Owner.SceneTarget(Recorder->Scenes.size(), width, height);
        Recorder->Scenes.push_back(std::move(pass));

        ShapeInstance instance = internal::Shape({ std::round(corner.X), std::round(corner.Y) },
            { static_cast<float>(width), static_cast<float>(height) }, Color::White, 0.0f, 0.0f, Color::Transparent,
            ShapeMode::PremultipliedPicture);
        instance.Coordinates[2] = 1.0f;
        instance.Coordinates[3] = 1.0f;
        Recorder->Add(instance, picture, internal::gpu::Sampling::LinearClamp);
    }

    namespace
    {
        // An area in points, after the transforms, as whole frame pixels.
        easyforge::Rectangle PixelArea(const FrameRecorder& recorder, easyforge::Rectangle area)
        {
            Vector2 corner = recorder.ToPixels(area.Position());
            Vector2 size = area.Size() * recorder.PixelsPerPoint();
            float left = std::round(corner.X);
            float top = std::round(corner.Y);
            return { left, top, std::round(corner.X + size.X) - left, std::round(corner.Y + size.Y) - top };
        }
    }

    void Canvas::BlurBehind(easyforge::Rectangle area, float radius, float cornerRadius) const
    {
        if (!Recorder || !Recorder->Active || area.IsEmpty() || radius <= 0.0f)
        {
            return;
        }
        internal::DrawList& list = Recorder->Current();
        float scale = Recorder->PixelsPerPoint();
        float reach = std::ceil(radius * scale);

        // The area in the target's own pixels, grown by the blur's reach so what is
        // just outside still blurs in, and kept inside the target.
        easyforge::Rectangle inner = PixelArea(*Recorder, area);
        inner.X -= list.Origin.X;
        inner.Y -= list.Origin.Y;
        easyforge::Rectangle bounds { 0.0f, 0.0f, list.Size.X, list.Size.Y };
        easyforge::Rectangle grown = Intersection(
            { inner.X - reach, inner.Y - reach, inner.Width + reach * 2.0f, inner.Height + reach * 2.0f }, bounds);
        inner = Intersection(inner, bounds);
        if (inner.IsEmpty() || grown.IsEmpty())
        {
            return;
        }
        int width = static_cast<int>(grown.Width);
        int height = static_cast<int>(grown.Height);
        std::array<internal::gpu::Texture*, 3> targets =
            Recorder->Owner.BlurTargets(Recorder->Owner.BlursRecorded++, width, height);

        ShapeInstance instance = internal::Shape(inner.Position() + list.Origin, inner.Size(), Color::White,
            cornerRadius * scale, 0.0f, Color::Transparent, ShapeMode::Backdrop);
        instance.Coordinates[0] = (inner.X - grown.X) / grown.Width;
        instance.Coordinates[1] = (inner.Y - grown.Y) / grown.Height;
        instance.Coordinates[2] = (inner.Right() - grown.X) / grown.Width;
        instance.Coordinates[3] = (inner.Bottom() - grown.Y) / grown.Height;
        Recorder->Add(instance, targets[0], internal::gpu::Sampling::LinearClamp);

        // The shape just added becomes a step of its own that blurs first.
        internal::DrawStep& step = list.Steps.back();
        if (step.Count > 1)
        {
            --step.Count;
            internal::DrawStep own;
            own.Texture = targets[0];
            own.First = step.First + step.Count;
            own.Count = 1;
            list.Steps.push_back(own);
        }
        internal::DrawStep& blur = list.Steps.back();
        blur.Backdrop = true;
        blur.BlurArea = grown;
        blur.BlurRadius = radius * scale;
        blur.BlurTargets[0] = targets[0];
        blur.BlurTargets[1] = targets[1];
        blur.BlurTargets[2] = targets[2];
    }

    void Canvas::Shaded(const easyforge::Shader& shader, easyforge::Rectangle area, std::vector<ShaderValue> values) const
    {
        if (!Recorder || !Recorder->Active || !shader || area.IsEmpty())
        {
            return;
        }
        Recorder->Used.push_back(shader.State());
        Recorder->AddShader(*shader.State(), PixelArea(*Recorder, area), Recorder->PixelsPerPoint(), nullptr, values);
    }

    void Canvas::BeginLayer(easyforge::Rectangle area) const
    {
        if (!Recorder || !Recorder->Active)
        {
            return;
        }
        easyforge::Rectangle pixels = PixelArea(*Recorder, area);
        int width = Max(static_cast<int>(pixels.Width), 1);
        int height = Max(static_cast<int>(pixels.Height), 1);
        std::size_t index = Recorder->Lists.size();
        internal::DrawList list;
        list.Origin = pixels.Position();
        list.Size = { static_cast<float>(width), static_cast<float>(height) };
        list.Target = Recorder->Owner.LayerTarget(index - 1, width, height);
        Recorder->Lists.push_back(std::move(list));
        Recorder->ListStack.push_back(index);
        Recorder->LayerAreas.push_back(pixels);
    }

    void Canvas::EndLayer(const LayerStyle& style) const
    {
        if (!Recorder || !Recorder->Active)
        {
            return;
        }
        if (Recorder->ListStack.size() <= 1)
        {
            Log(LogLevel::Warning, "Canvas::EndLayer was called more times than BeginLayer");
            return;
        }
        std::size_t index = Recorder->ListStack.back();
        Recorder->ListStack.pop_back();
        Recorder->FinishedLayers.push_back(index);
        easyforge::Rectangle area = Recorder->LayerAreas[index - 1];
        internal::gpu::Texture* picture = Recorder->Lists[index].Target;

        if (style.Shader)
        {
            Recorder->Used.push_back(style.Shader.State());
            Recorder->AddShader(*style.Shader.State(), area, Recorder->PixelsPerPoint(), picture, style.Values);
            return;
        }
        // The picture holds colors multiplied by alpha, so fading multiplies every
        // channel by the opacity.
        float opacity = Clamp(style.Opacity, 0.0f, 1.0f);
        ShapeInstance instance = internal::Shape(area.Position(), area.Size(), Color { 1.0f, 1.0f, 1.0f, opacity }, 0.0f,
            0.0f, Color::Transparent, ShapeMode::PremultipliedPicture);
        instance.Coordinates[2] = 1.0f;
        instance.Coordinates[3] = 1.0f;
        Recorder->Add(instance, picture, internal::gpu::Sampling::LinearClamp);
    }

    void Canvas::PushClip(easyforge::Rectangle area) const
    {
        if (!Recorder || !Recorder->Active)
        {
            return;
        }
        Vector2 corner = Recorder->ToPixels(area.Position());
        Vector2 size = area.Size() * Recorder->PixelsPerPoint();
        easyforge::Rectangle pixels = Intersection(Recorder->Clips.back(), { corner.X, corner.Y, size.X, size.Y });
        Recorder->Clips.push_back(pixels);
    }

    void Canvas::PopClip() const
    {
        if (!Recorder || !Recorder->Active)
        {
            return;
        }
        if (Recorder->Clips.size() > 1)
        {
            Recorder->Clips.pop_back();
        }
        else
        {
            Log(LogLevel::Warning, "Canvas::PopClip was called more times than PushClip");
        }
    }

    void Canvas::PushTransform(Vector2 offset, float scale) const
    {
        if (!Recorder || !Recorder->Active)
        {
            return;
        }
        const FrameRecorder::Placement& parent = Recorder->Transforms.back();
        Recorder->Transforms.push_back({ parent.Offset + offset * parent.Scale, parent.Scale * scale });
    }

    void Canvas::PopTransform() const
    {
        if (!Recorder || !Recorder->Active)
        {
            return;
        }
        if (Recorder->Transforms.size() > 1)
        {
            Recorder->Transforms.pop_back();
        }
        else
        {
            Log(LogLevel::Warning, "Canvas::PopTransform was called more times than PushTransform");
        }
    }

    Vector2 Canvas::Size() const
    {
        if (!Recorder)
        {
            return {};
        }
        return Recorder->PixelSize / Recorder->Scale;
    }

    float Canvas::Scale() const
    {
        return Recorder ? Recorder->Scale : 1.0f;
    }
}
