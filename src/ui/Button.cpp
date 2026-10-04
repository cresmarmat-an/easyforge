#include <easyforge/ui/Elements.h>

#include "Behavior.h"
#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        class ButtonBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Button::KindName; }
            bool HoldsChildren() const override { return true; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Padding")
                {
                    return DataValue(Vector4 { 14.0f, 6.0f, 14.0f, 6.0f });
                }
                if (property == "Style")
                {
                    return DataValue("normal");
                }
                if (property == "Gap")
                {
                    return DataValue(8.0);
                }
                if (property == "Alignment" || property == "Distribution" || property == "TextAlignment")
                {
                    return DataValue("center");
                }
                return Behavior::Default(property);
            }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint) override
            {
                if (!element.Children.empty())
                {
                    return MeasureLine(element, context, constraint, true);
                }
                easyforge::Font font = FontOf(element, context);
                float size = FontSizeOf(element, context);
                if (!font)
                {
                    return {};
                }
                std::string text = element.Get("Text").AsText();
                Vector2 measured = font.Measure(text, size);
                measured.Y = Max(measured.Y, font.LineHeight(size));
                return measured;
            }

            void Arrange(ElementState& element, Context& context, Rectangle content) override
            {
                ArrangeLine(element, context, content, true);
            }

            Box LookOf(ElementState& element, const Context& context) override
            {
                const Style& style = element.CurrentStyle();
                const Theme& theme = *context.Theme;
                ButtonStyle kind = FromData<ButtonStyle>(element.Get("Style"));
                bool enabled = element.IsEnabled();
                float hover = enabled ? element.HoverAmount : 0.0f;
                float press = enabled ? element.PressAmount : 0.0f;

                Box box;
                box.CornerRadius = style.CornerRadius.value_or(theme.CornerRadius);
                if (style.Background && style.Background->Type == Background::Kind::Color)
                {
                    Color base = style.Background->First;
                    Color shown = Mix(Mix(base, theme.Text, hover * 0.08f), theme.Text, press * 0.12f);
                    box.Background = ui::Background(shown);
                }
                else if (style.Background)
                {
                    box.Background = style.Background;
                }
                else
                {
                    Color base = theme.Control;
                    Color hovered = theme.ControlHovered;
                    Color pressed = theme.ControlPressed;
                    if (kind == ButtonStyle::Accent)
                    {
                        base = theme.Accent;
                        hovered = theme.AccentHovered;
                        pressed = theme.AccentPressed;
                    }
                    else if (kind == ButtonStyle::Subtle)
                    {
                        base = theme.ControlHovered.WithAlpha(0.0f);
                    }
                    if (!enabled && kind == ButtonStyle::Accent)
                    {
                        base = theme.Control;
                    }
                    box.Background = ui::Background(Mix(Mix(base, hovered, hover), pressed, press));
                }
                if (style.BorderWidth > 0.0f)
                {
                    box.BorderWidth = style.BorderWidth;
                    box.BorderColor = style.BorderColor.value_or(theme.Border);
                }
                else if (kind == ButtonStyle::Normal && !style.Background)
                {
                    box.BorderWidth = theme.BorderWidth;
                    box.BorderColor = style.BorderColor.value_or(theme.Border);
                }
                return box;
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                if (!element.Children.empty())
                {
                    return;
                }
                std::string text = element.Get("Text").AsText();
                easyforge::Font font = FontOf(element, context);
                if (text.empty() || !font)
                {
                    return;
                }
                const Theme& theme = *context.Theme;
                const Style& style = element.CurrentStyle();
                ButtonStyle kind = FromData<ButtonStyle>(element.Get("Style"));
                std::optional<Color> chosen = FromData<std::optional<Color>>(element.Get("Color"));
                Color color = chosen.value_or(kind == ButtonStyle::Accent && !style.Background ? theme.AccentText : theme.Text);
                if (!element.IsEnabled())
                {
                    color = theme.DisabledText;
                }
                Rectangle content { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
                    element.Frame.Width - style.Padding.Horizontal(), element.Frame.Height - style.Padding.Vertical() };
                DrawTextIn(context.Canvas, font, text, FontSizeOf(element, context), color, content,
                    FromData<ui::TextAlignment>(element.Get("TextAlignment")), true);
            }

            bool TakesPointer(ElementState&) const override { return true; }
            bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }

            void PointerReleased(ElementState& element, Context&, const Pointer&, bool inside) override
            {
                if (inside && element.IsEnabled())
                {
                    Click(element);
                }
            }

            bool KeyPressed(ElementState& element, Context&, const Event& event) override
            {
                if (event.Key == Key::Enter || event.Key == Key::NumberPadEnter || event.Key == Key::Space)
                {
                    if (!event.Repeat)
                    {
                        Click(element);
                    }
                    return true;
                }
                return false;
            }

            static void Click(ElementState& element)
            {
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                std::function<void()> callback = element.Object<std::function<void()>>("OnClick");
                if (callback)
                {
                    callback();
                }
            }
        };
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Button::Button(std::string text, const ButtonSettings& settings) : Button(MakeElement(std::make_unique<ButtonBehavior>()))
    {
        const ButtonSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Style", settings.Style, defaults.Style);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
        Choose(*State, "TextAlignment", settings.TextAlignment, defaults.TextAlignment);
        if (!text.empty())
        {
            State->Set("Text", DataValue(std::move(text)));
        }
        if (settings.OnClick)
        {
            State->SetObject("OnClick", settings.OnClick);
        }
        AddChildren(*State, settings.Children);
    }

    Button::Button(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Text(Stored<std::string, "Text">(State)), Style(Stored<ButtonStyle, "Style">(State)),
          FontSize(Stored<float, "FontSize">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          TextAlignment(Stored<ui::TextAlignment, "TextAlignment">(State)),
          OnClick(Kept<std::function<void()>, "OnClick">(State))
    {
    }

    Button::Button(const Button& other) : Button(other.State)
    {
    }

    Button& Button::operator=(const Button& other)
    {
        State = other.State;
        return *this;
    }

    void Button::Click() const
    {
        if (State && State->IsEnabled())
        {
            ButtonBehavior::Click(*State);
        }
    }
}
