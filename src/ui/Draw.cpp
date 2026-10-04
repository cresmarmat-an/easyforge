#include <cmath>
#include <string>

#include <easyforge/core/Log.h>
#include <easyforge/graphics/Font.h>
#include <easyforge/graphics/Shader.h>
#include <easyforge/ui/Effects.h>

#include "Drawing.h"
#include "RootState.h"

namespace easyforge::ui
{
    Background Background::Gradient(easyforge::Color from, easyforge::Color to, float angle)
    {
        ui::Background background;
        background.Type = Kind::Gradient;
        background.First = from;
        background.Last = to;
        background.Angle = angle;
        return background;
    }

    Background Background::Image(std::string_view path, const ImageBackgroundSettings& settings)
    {
        return Image(easyforge::Texture::Load(path), settings);
    }

    Background Background::Image(const easyforge::Texture& texture, const ImageBackgroundSettings& settings)
    {
        ui::Background background;
        background.Type = Kind::Image;
        background.Picture = texture;
        background.PictureSettings = settings;
        if (!texture && !texture.Error().empty())
        {
            Log(LogLevel::Warning, "the background image could not be loaded: {}", texture.Error());
        }
        return background;
    }
}

namespace easyforge::ui::internal
{
    namespace
    {
        constexpr const char* MaskSource = R"(
value BoxOrigin: vector2 = vector2(0, 0)
value BoxSize: vector2 = vector2(0, 0)
value CornerRadius: number = 0
value Feather: number = 0

function Pixel(input: PixelInput) returns color then
    variable result = Sample(input.Content, input.Coordinates)
    constant half = BoxSize * 0.5
    constant radius = Min(CornerRadius, Min(half.X, half.Y))
    constant inside = Absolute(input.Position - BoxOrigin - half) - half + vector2(radius, radius)
    constant distance = Length(Max(inside, vector2(0, 0))) + Min(Max(inside.X, inside.Y), 0) - radius
    constant softness = Max(Feather, 0.75)
    result.Alpha = result.Alpha * Clamp(0.5 - distance / softness, 0, 1)
    return result
end
)";

        constexpr const char* ColorAdjustSource = R"(
value Brightness: number = 0
value Contrast: number = 1
value Saturation: number = 1
value Hue: number = 0

function Pixel(input: PixelInput) returns color then
    constant original = Sample(input.Content, input.Coordinates)
    variable shade = vector3(original.Red, original.Green, original.Blue)

    -- Turning around the gray axis moves every color around the color wheel.
    constant angle = Radians(Hue)
    constant cosine = Cosine(angle)
    constant sine = Sine(angle)
    constant axis = vector3(0.57735, 0.57735, 0.57735)
    shade = shade * cosine + Cross(axis, shade) * sine + axis * Dot(axis, shade) * (1 - cosine)

    constant gray = Dot(shade, vector3(0.2126, 0.7152, 0.0722))
    shade = Lerp(vector3(gray), shade, Saturation)
    shade = (shade - vector3(0.5)) * Contrast + vector3(0.5)
    shade = shade + vector3(Brightness)
    return color(Clamp(shade, vector3(0), vector3(1)), original.Alpha)
end
)";

        easyforge::Shader MakeEffectShader(const char* source, std::string_view name)
        {
            easyforge::Shader shader = easyforge::Shader::FromText(source, name);
            if (!shader)
            {
                Log(LogLevel::Error, "ui's {} shader did not compile: {}", name, shader.Error());
            }
            return shader;
        }

        // The root's effect shaders, made on first use.
        RootState& WithEffectShaders(const Context& context)
        {
            RootState& root = *context.Root;
            if (!root.EffectShadersMade)
            {
                root.EffectShadersMade = true;
                root.MaskEffect = MakeEffectShader(MaskSource, "mask.shader");
                root.ColorAdjustEffect = MakeEffectShader(ColorAdjustSource, "color-adjust.shader");
            }
            return root;
        }

        // How far past the box its effects and focus ring reach.
        float Reach(const Style& style)
        {
            float reach = 4.0f;
            for (const Effect& effect : style.Effects)
            {
                if (const Shadow* shadow = std::get_if<Shadow>(&effect))
                {
                    reach = Max(reach, shadow->Blur + shadow->Spread + Max(std::abs(shadow->Offset.X), std::abs(shadow->Offset.Y)));
                }
                else if (const Glow* glow = std::get_if<Glow>(&effect))
                {
                    reach = Max(reach, glow->Blur + glow->Spread);
                }
                else if (const Outline* outline = std::get_if<Outline>(&effect))
                {
                    reach = Max(reach, outline->Gap + outline->Width);
                }
            }
            return std::ceil(reach);
        }

        Rectangle Grown(Rectangle area, float amount)
        {
            return { area.X - amount, area.Y - amount, area.Width + amount * 2.0f, area.Height + amount * 2.0f };
        }

        // How far the element and what is drawn with it reach: its effects, and
        // children that overflow it, unless it clips them.
        Rectangle Extent(ElementState& element)
        {
            Rectangle extent = Grown(element.Frame, Reach(element.CurrentStyle()));
            if (element.Kind->ClipsChildren(element))
            {
                return extent;
            }
            std::vector<ElementState*> children;
            element.Kind->VisibleChildren(element, children);
            for (ElementState* child : children)
            {
                Rectangle inner = Extent(*child);
                const Style& style = child->CurrentStyle();
                if (style.Offset != Vector2 {} || style.Scale != 1.0f)
                {
                    Vector2 shift = style.Offset + child->Frame.Center() * (1.0f - style.Scale);
                    inner = { inner.X * style.Scale + shift.X, inner.Y * style.Scale + shift.Y, inner.Width * style.Scale,
                        inner.Height * style.Scale };
                }
                extent = Union(extent, inner);
            }
            return extent;
        }
    }

    FittedPicture FitPicture(Vector2 pictureSize, Rectangle box, ImageFit fit)
    {
        FittedPicture result { box, { 0.0f, 0.0f, pictureSize.X, pictureSize.Y } };
        if (pictureSize.X <= 0.0f || pictureSize.Y <= 0.0f || box.IsEmpty())
        {
            return result;
        }
        switch (fit)
        {
        case ImageFit::Stretch: break;
        case ImageFit::Contain:
        {
            float scale = Min(box.Width / pictureSize.X, box.Height / pictureSize.Y);
            Vector2 size = pictureSize * scale;
            result.Destination = { box.X + (box.Width - size.X) * 0.5f, box.Y + (box.Height - size.Y) * 0.5f, size.X, size.Y };
            break;
        }
        case ImageFit::Cover:
        {
            float scale = Max(box.Width / pictureSize.X, box.Height / pictureSize.Y);
            Vector2 shown = box.Size() / scale;
            result.Source = { (pictureSize.X - shown.X) * 0.5f, (pictureSize.Y - shown.Y) * 0.5f, shown.X, shown.Y };
            break;
        }
        case ImageFit::Center:
        {
            Vector2 size { Min(pictureSize.X, box.Width), Min(pictureSize.Y, box.Height) };
            result.Destination = { box.X + (box.Width - size.X) * 0.5f, box.Y + (box.Height - size.Y) * 0.5f, size.X, size.Y };
            result.Source = { (pictureSize.X - size.X) * 0.5f, (pictureSize.Y - size.Y) * 0.5f, size.X, size.Y };
            break;
        }
        }
        return result;
    }

    void DrawPicture(const Canvas& canvas, const Texture& texture, Rectangle box, ImageFit fit, float slice, Color tint,
        float cornerRadius)
    {
        if (!texture || box.IsEmpty())
        {
            return;
        }
        Vector2 pictureSize { static_cast<float>(texture.Width()), static_cast<float>(texture.Height()) };
        if (slice > 0.0f && fit == ImageFit::Stretch)
        {
            canvas.Image(texture, { .Position = box.Position(), .Size = box.Size(), .Tint = tint, .Slice = slice });
            return;
        }
        FittedPicture placed = FitPicture(pictureSize, box, fit);
        canvas.Image(texture, { .Position = placed.Destination.Position(), .Size = placed.Destination.Size(),
                                  .Source = placed.Source, .Tint = tint, .CornerRadius = cornerRadius });
    }

    void DrawBox(const Canvas& canvas, const Box& box, Rectangle area)
    {
        bool border = box.BorderWidth > 0.0f && box.BorderColor.Alpha > 0.0f;
        const std::optional<ui::Background>& background = box.Background;
        Background::Kind kind = background ? background->Type : Background::Kind::None;
        switch (kind)
        {
        case Background::Kind::None:
        case Background::Kind::Color:
        {
            Color fill = kind == Background::Kind::Color ? background->First : Color::Transparent;
            if (fill.Alpha <= 0.0f && !border)
            {
                return;
            }
            canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(), .Color = fill,
                .CornerRadius = box.CornerRadius, .BorderWidth = border ? box.BorderWidth : 0.0f,
                .BorderColor = box.BorderColor });
            return;
        }
        case Background::Kind::Gradient:
            canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(), .CornerRadius = box.CornerRadius,
                .BorderWidth = border ? box.BorderWidth : 0.0f, .BorderColor = box.BorderColor,
                .Gradient = LinearGradient { .From = background->First, .To = background->Last, .Angle = background->Angle } });
            return;
        case Background::Kind::Image:
        {
            const ImageBackgroundSettings& settings = background->PictureSettings;
            DrawPicture(canvas, background->Picture, area, settings.Fit, settings.Slice, settings.Tint, box.CornerRadius);
            if (border)
            {
                canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(), .Color = Color::Transparent,
                    .CornerRadius = box.CornerRadius, .BorderWidth = box.BorderWidth, .BorderColor = box.BorderColor });
            }
            return;
        }
        }
    }

    void DrawRing(const Canvas& canvas, Rectangle area, float cornerRadius, float gap, float width, Color color)
    {
        float grow = gap + width;
        canvas.Rectangle({ .Position = area.Position() - Vector2 { grow, grow },
            .Size = area.Size() + Vector2 { grow * 2.0f, grow * 2.0f }, .Color = Color::Transparent,
            .CornerRadius = cornerRadius > 0.0f ? cornerRadius + grow : 0.0f, .BorderWidth = width, .BorderColor = color });
    }

    void DrawTextIn(const Canvas& canvas, const Font& font, std::string_view text, float size, Color color, Rectangle box,
        TextAlignment alignment, bool centerVertically)
    {
        if (!font || text.empty())
        {
            return;
        }
        float lineHeight = font.LineHeight(size);
        std::size_t lines = 1;
        for (char character : text)
        {
            lines += character == '\n' ? 1 : 0;
        }
        float top = box.Y;
        if (centerVertically)
        {
            top += (box.Height - lineHeight * static_cast<float>(lines)) * 0.5f;
        }
        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t end = text.find('\n', start);
            std::string_view line = text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
            float left = box.X;
            if (alignment != TextAlignment::Start)
            {
                float width = font.Measure(line, size).X;
                left += alignment == TextAlignment::Center ? (box.Width - width) * 0.5f : box.Width - width;
            }
            canvas.Text(font, line, { .Position = { left, top }, .Size = size, .Color = color });
            top += lineHeight;
            if (end == std::string_view::npos)
            {
                break;
            }
            start = end + 1;
        }
    }

    void DrawElement(ElementState& element, DrawContext& context)
    {
        const Style& style = element.CurrentStyle();
        if (!style.Visible || style.Opacity <= 0.001f)
        {
            return;
        }
        const Canvas& canvas = context.Canvas;
        Rectangle area = element.Frame;

        bool moved = style.Offset != Vector2 {} || style.Scale != 1.0f;
        if (moved)
        {
            Vector2 center = area.Center();
            canvas.PushTransform(style.Offset + center * (1.0f - style.Scale), style.Scale);
        }

        const ColorAdjust* adjust = nullptr;
        const Mask* mask = nullptr;
        for (const Effect& effect : style.Effects)
        {
            adjust = adjust ? adjust : std::get_if<ColorAdjust>(&effect);
            mask = mask ? mask : std::get_if<Mask>(&effect);
        }
        Box look = element.Kind->LookOf(element, context);

        // The blur of what is behind comes first, on the picture under the
        // element rather than inside its own layers, and fades with it.
        for (const Effect& effect : style.Effects)
        {
            if (const BackgroundBlur* blur = std::get_if<BackgroundBlur>(&effect))
            {
                canvas.BlurBehind(area, blur->Radius, mask ? mask->CornerRadius : look.CornerRadius, style.Opacity);
            }
        }

        // Fading and adjusting cover everything drawn with the element, cut down
        // to what can show. A shader's layer keeps the element's own area, which
        // its input.Position is measured from.
        bool fade = style.Opacity < 0.999f;
        bool shaded = static_cast<bool>(style.Shader);
        Rectangle shown = (fade || adjust) ? Intersection(Extent(element), canvas.ClipArea()) : Rectangle {};
        Rectangle shaderBounds = Grown(area, Reach(style));
        if (fade)
        {
            canvas.BeginLayer(shown);
        }
        if (shaded)
        {
            canvas.BeginLayer(shaderBounds);
        }
        if (adjust)
        {
            canvas.BeginLayer(shown);
        }

        // Under the box: shadows and glows, outside it only, so a see-through
        // box does not show them through itself.
        for (const Effect& effect : style.Effects)
        {
            if (const Shadow* shadow = std::get_if<Shadow>(&effect))
            {
                Vector2 grow { shadow->Spread, shadow->Spread };
                canvas.Rectangle({ .Position = area.Position() + shadow->Offset - grow, .Size = area.Size() + grow * 2.0f,
                    .Color = shadow->Color, .CornerRadius = look.CornerRadius + shadow->Spread, .Blur = shadow->Blur,
                    .Hole = area, .HoleCornerRadius = look.CornerRadius });
            }
            else if (const Glow* glow = std::get_if<Glow>(&effect))
            {
                Vector2 grow { glow->Spread, glow->Spread };
                canvas.Rectangle({ .Position = area.Position() - grow, .Size = area.Size() + grow * 2.0f,
                    .Color = glow->Color.value_or(context.Theme->Accent), .CornerRadius = look.CornerRadius + glow->Spread,
                    .Blur = glow->Blur, .Hole = area, .HoleCornerRadius = look.CornerRadius });
            }
        }

        // The mask cuts the box, the content, and the children; what belongs
        // outside the box, such as outlines and the focus ring, is drawn after.
        Rectangle maskBounds = Grown(area, mask ? std::ceil(mask->Feather) + 2.0f : 0.0f);
        if (mask)
        {
            canvas.BeginLayer(maskBounds);
        }

        DrawBox(canvas, look, area);
        for (const Effect& effect : style.Effects)
        {
            if (const Gradient* gradient = std::get_if<Gradient>(&effect))
            {
                canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(), .CornerRadius = look.CornerRadius,
                    .Gradient = LinearGradient { .From = gradient->From, .To = gradient->To, .Angle = gradient->Angle } });
            }
        }

        element.Kind->Draw(element, context);

        std::vector<ElementState*> children;
        element.Kind->VisibleChildren(element, children);
        if (!children.empty())
        {
            bool clip = element.Kind->ClipsChildren(element);
            if (clip)
            {
                canvas.PushClip(area);
            }
            if (!element.Kind->DrawChildren(element, context))
            {
                // Held while drawing, since drawing code may take children out.
                std::vector<std::shared_ptr<ElementState>> kept;
                kept.reserve(children.size());
                for (ElementState* child : children)
                {
                    kept.push_back(child->shared_from_this());
                }
                for (const std::shared_ptr<ElementState>& child : kept)
                {
                    DrawElement(*child, context);
                }
            }
            if (clip)
            {
                canvas.PopClip();
            }
        }

        element.Kind->DrawOver(element, context);

        if (mask)
        {
            canvas.EndLayer({ .Shader = WithEffectShaders(context).MaskEffect,
                .Values = { { "BoxOrigin", area.Position() - maskBounds.Position() }, { "BoxSize", area.Size() },
                    { "CornerRadius", mask->CornerRadius }, { "Feather", mask->Feather } } });
        }

        for (const Effect& effect : style.Effects)
        {
            if (const Outline* outline = std::get_if<Outline>(&effect))
            {
                DrawRing(canvas, area, look.CornerRadius, outline->Gap, outline->Width,
                    outline->Color.value_or(context.Theme->Focus));
            }
        }
        if (element.Focused && context.FocusVisible && element.Kind->ShowsFocusRing())
        {
            DrawRing(canvas, area, look.CornerRadius, 2.0f, 2.0f, context.Theme->Focus);
        }

        if (adjust)
        {
            canvas.EndLayer({ .Shader = WithEffectShaders(context).ColorAdjustEffect,
                .Values = { { "Brightness", adjust->Brightness }, { "Contrast", adjust->Contrast },
                    { "Saturation", adjust->Saturation }, { "Hue", adjust->Hue } } });
        }
        if (shaded)
        {
            canvas.EndLayer({ .Shader = style.Shader, .Values = style.ShaderValues });
        }
        if (fade)
        {
            canvas.EndLayer({ .Opacity = style.Opacity });
        }
        if (moved)
        {
            canvas.PopTransform();
        }
    }
}
