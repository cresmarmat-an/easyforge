#include <easyforge/ui/TitleBar.h>

#include "Behavior.h"
#include "Containers.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        class TitleBarBehavior final : public LineBehavior
        {
        public:
            TitleBarBehavior() : LineBehavior(TitleBar::KindName, true) {}

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width")
                {
                    return DataValue("fill");
                }
                if (property == "Height")
                {
                    return DataValue(36.0);
                }
                if (property == "Alignment")
                {
                    return DataValue("center");
                }
                if (property == "Gap")
                {
                    return DataValue(8.0);
                }
                if (property == "Padding")
                {
                    return DataValue(Vector4 { 12.0f, 0.0f, 0.0f, 0.0f });
                }
                return LineBehavior::Default(property);
            }

            Box LookOf(ElementState& element, const Context& context) override
            {
                Box box = Behavior::LookOf(element, context);
                if (!element.CurrentStyle().Background)
                {
                    box.Background = ui::Background(context.Theme->TitleBar);
                }
                return box;
            }
        };

        enum class Part
        {
            None,
            Minimize,
            Maximize,
            Close,
        };

        class WindowButtonsBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return WindowButtons::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Height")
                {
                    return DataValue("fill");
                }
                if (property == "ButtonWidth")
                {
                    return DataValue(46.0);
                }
                if (property == "Minimize" || property == "Maximize" || property == "Close")
                {
                    return DataValue(true);
                }
                return Behavior::Default(property);
            }

            std::vector<Part> Parts(ElementState& element) const
            {
                std::vector<Part> parts;
                if (element.Get("Minimize").AsBoolean())
                {
                    parts.push_back(Part::Minimize);
                }
                if (element.Get("Maximize").AsBoolean())
                {
                    parts.push_back(Part::Maximize);
                }
                if (element.Get("Close").AsBoolean())
                {
                    parts.push_back(Part::Close);
                }
                return parts;
            }

            Part PartAt(ElementState& element, Vector2 point) const
            {
                if (!element.Frame.Contains(point))
                {
                    return Part::None;
                }
                std::vector<Part> parts = Parts(element);
                float width = element.Get("ButtonWidth").As<float>();
                float fromRight = element.Frame.Right() - point.X;
                std::size_t index = static_cast<std::size_t>(fromRight / Max(width, 1.0f));
                if (index >= parts.size())
                {
                    return Part::None;
                }
                return parts[parts.size() - 1 - index];
            }

            Vector2 MeasureContent(ElementState& element, Context&, const Constraint&) override
            {
                return { element.Get("ButtonWidth").As<float>() * static_cast<float>(Parts(element).size()), 32.0f };
            }

            bool TakesPointer(ElementState&) const override { return true; }

            void PointerMoved(ElementState& element, Context&, const Pointer& pointer) override
            {
                Hovered = PartAt(element, pointer.Position);
            }

            void PointerPressed(ElementState& element, Context&, const Pointer& pointer) override
            {
                Pressed = PartAt(element, pointer.Position);
            }

            void PointerReleased(ElementState&, Context&, const Pointer&, bool) override
            {
                // The window acts on its buttons itself.
                Pressed = Part::None;
            }

            std::optional<HitArea> TitleBarArea(ElementState& element, Vector2 point) const override
            {
                switch (PartAt(element, point))
                {
                case Part::Minimize: return HitArea::MinimizeButton;
                case Part::Maximize: return HitArea::MaximizeButton;
                case Part::Close: return HitArea::CloseButton;
                case Part::None: return std::nullopt;
                }
                return std::nullopt;
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                const Theme& theme = *context.Theme;
                const Canvas& canvas = context.Canvas;
                std::vector<Part> parts = Parts(element);
                float width = element.Get("ButtonWidth").As<float>();
                bool maximized = context.Root && context.Root->TheHost &&
                                 context.Root->TheHost->Mode() == WindowMode::Maximized;
                float left = element.Frame.Right() - width * static_cast<float>(parts.size());
                for (Part part : parts)
                {
                    Rectangle area { left, element.Frame.Y, width, element.Frame.Height };
                    left += width;
                    bool hovered = element.Hovered && Hovered == part;
                    bool pressed = element.Pressed && Pressed == part;
                    Color glyph = theme.TitleBarText;
                    if (hovered || pressed)
                    {
                        Color fill = part == Part::Close ? theme.CloseButtonHovered : theme.TitleButtonHovered;
                        if (pressed)
                        {
                            fill = part == Part::Close ? Mix(fill, Color::Black, 0.15f) : fill.WithAlpha(fill.Alpha * 1.6f);
                        }
                        canvas.Rectangle({ .Position = area.Position(), .Size = area.Size(), .Color = fill });
                        if (part == Part::Close)
                        {
                            glyph = Color::White;
                        }
                    }
                    Vector2 center = area.Center();
                    LineStyle line { .Color = glyph, .Width = 1.0f };
                    switch (part)
                    {
                    case Part::Minimize:
                        canvas.Line(center + Vector2 { -5.0f, 0.5f }, center + Vector2 { 5.0f, 0.5f }, line);
                        break;
                    case Part::Maximize:
                        if (maximized)
                        {
                            canvas.Rectangle({ .Position = center + Vector2 { -5.0f, -3.0f }, .Size = { 8.0f, 8.0f },
                                .Color = Color::Transparent, .CornerRadius = 1.0f, .BorderWidth = 1.0f, .BorderColor = glyph });
                            canvas.Line(center + Vector2 { -3.0f, -5.0f }, center + Vector2 { 5.0f, -5.0f }, line);
                            canvas.Line(center + Vector2 { 5.0f, -5.5f }, center + Vector2 { 5.0f, 3.0f }, line);
                        }
                        else
                        {
                            canvas.Rectangle({ .Position = center - Vector2 { 5.0f, 5.0f }, .Size = { 10.0f, 10.0f },
                                .Color = Color::Transparent, .CornerRadius = 1.0f, .BorderWidth = 1.0f, .BorderColor = glyph });
                        }
                        break;
                    case Part::Close:
                        canvas.Line(center + Vector2 { -5.0f, -5.0f }, center + Vector2 { 5.0f, 5.0f }, line);
                        canvas.Line(center + Vector2 { 5.0f, -5.0f }, center + Vector2 { -5.0f, 5.0f }, line);
                        break;
                    case Part::None: break;
                    }
                }
            }

            Part Hovered = Part::None;
            Part Pressed = Part::None;
        };
    }
}

namespace easyforge::ui
{
    using namespace internal;

    TitleBar::TitleBar(const TitleBarSettings& settings) : Container(MakeElement(std::make_unique<TitleBarBehavior>()))
    {
        const TitleBarSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Gap", settings.Gap, defaults.Gap);
        Choose(*State, "Alignment", settings.Alignment, defaults.Alignment);
        Choose(*State, "Distribution", settings.Distribution, defaults.Distribution);
        AddChildren(*State, settings.Children);
    }

    WindowButtons::WindowButtons(const WindowButtonsSettings& settings)
        : Element(MakeElement(std::make_unique<WindowButtonsBehavior>()))
    {
        const WindowButtonsSettings defaults {};
        if (!settings.Name.empty())
        {
            State->Row.Rename(settings.Name);
        }
        Choose(*State, "ButtonWidth", settings.ButtonWidth, defaults.ButtonWidth);
        Choose(*State, "Minimize", settings.Minimize, defaults.Minimize);
        Choose(*State, "Maximize", settings.Maximize, defaults.Maximize);
        Choose(*State, "Close", settings.Close, defaults.Close);
        Choose(*State, "Visible", settings.Visible, defaults.Visible);
    }
}
