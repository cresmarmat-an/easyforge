#include <easyforge/ui/Elements.h>

#include "Behavior.h"
#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        constexpr float IndicatorGap = 8.0f;

        // Checkbox and Toggle: an indicator, and text beside it.
        class CheckBehavior final : public Behavior
        {
        public:
            explicit CheckBehavior(bool toggle) : IsToggle(toggle) {}

            std::string_view Name() const override { return IsToggle ? Toggle::KindName : Checkbox::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Checked")
                {
                    return DataValue(false);
                }
                return Behavior::Default(property);
            }

            Vector2 Indicator() const { return IsToggle ? Vector2 { 40.0f, 20.0f } : Vector2 { 20.0f, 20.0f }; }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint&) override
            {
                Vector2 size = Indicator();
                std::string text = element.Get("Text").AsText();
                easyforge::Font font = FontOf(element, context);
                if (!text.empty() && font)
                {
                    Vector2 measured = font.Measure(text, FontSizeOf(element, context));
                    size.X += IndicatorGap + measured.X;
                    size.Y = Max(size.Y, measured.Y);
                }
                return size;
            }

            void Update(ElementState& element, Context& context) override
            {
                float target = element.Get("Checked").AsBoolean() ? 1.0f : 0.0f;
                float step = context.DeltaSeconds / Max(context.Theme->Transition, 0.001f);
                Amount = Amount < target ? Min(Amount + step, target) : Max(Amount - step, target);
                if (!Started)
                {
                    Amount = target;
                    Started = true;
                }
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                const Theme& theme = *context.Theme;
                const Canvas& canvas = context.Canvas;
                const Style& style = element.CurrentStyle();
                bool enabled = element.IsEnabled();
                float hover = enabled ? element.HoverAmount : 0.0f;
                Rectangle content { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
                    element.Frame.Width - style.Padding.Horizontal(), element.Frame.Height - style.Padding.Vertical() };
                Vector2 indicator = Indicator();
                Vector2 corner { content.X, content.Y + (content.Height - indicator.Y) * 0.5f };
                Color accent = enabled ? Mix(theme.Accent, theme.AccentHovered, hover) : theme.DisabledText;
                Color idle = enabled ? Mix(theme.Control, theme.ControlHovered, hover) : theme.Control;

                if (IsToggle)
                {
                    // A track that fills with the accent, and a knob that slides across.
                    Color track = Mix(idle, accent, Amount);
                    Color edge = Mix(enabled ? theme.MutedText : theme.DisabledText, accent, Amount);
                    canvas.Rectangle({ .Position = corner, .Size = indicator, .Color = track, .CornerRadius = indicator.Y * 0.5f,
                        .BorderWidth = 1.0f, .BorderColor = edge });
                    float radius = easyforge::Lerp(5.0f, 6.0f, Amount) + (element.Pressed ? 1.0f : 0.0f);
                    float travel = indicator.X - indicator.Y;
                    Vector2 center { corner.X + indicator.Y * 0.5f + travel * Amount, corner.Y + indicator.Y * 0.5f };
                    Color knob = Mix(enabled ? theme.MutedText : theme.DisabledText, theme.AccentText, Amount);
                    canvas.Circle(center, radius, { .Color = knob });
                }
                else
                {
                    Color fill = Mix(idle, accent, Amount);
                    Color edge = Mix(theme.Border, accent, Amount);
                    canvas.Rectangle({ .Position = corner, .Size = indicator, .Color = fill, .CornerRadius = 4.0f,
                        .BorderWidth = 1.0f, .BorderColor = edge });
                    if (Amount > 0.01f)
                    {
                        Color mark = theme.AccentText.WithAlpha(Amount);
                        LineStyle line { .Color = mark, .Width = 1.8f };
                        Vector2 first = corner + Vector2 { 5.0f, 10.5f };
                        Vector2 middle = corner + Vector2 { 8.5f, 14.0f };
                        Vector2 last = corner + Vector2 { 15.0f, 6.5f };
                        canvas.Line(first, middle, line);
                        canvas.Line(middle, last, line);
                    }
                }

                std::string text = element.Get("Text").AsText();
                easyforge::Font font = FontOf(element, context);
                if (!text.empty() && font)
                {
                    std::optional<Color> color = FromData<std::optional<Color>>(element.Get("Color"));
                    Color shown = enabled ? color.value_or(theme.Text) : theme.DisabledText;
                    float left = indicator.X + IndicatorGap;
                    DrawTextIn(canvas, font, text, FontSizeOf(element, context), shown,
                        { content.X + left, content.Y, content.Width - left, content.Height }, TextAlignment::Start, true);
                }
            }

            bool TakesPointer(ElementState&) const override { return true; }
            bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }

            void PointerReleased(ElementState& element, Context&, const Pointer&, bool inside) override
            {
                if (inside && element.IsEnabled())
                {
                    Flip(element);
                }
            }

            bool KeyPressed(ElementState& element, Context&, const Event& event) override
            {
                if (event.Key == Key::Space || event.Key == Key::Enter || event.Key == Key::NumberPadEnter)
                {
                    if (!event.Repeat)
                    {
                        Flip(element);
                    }
                    return true;
                }
                return false;
            }

            static void Flip(ElementState& element)
            {
                SetChecked(element, !element.Get("Checked").AsBoolean());
            }

            static void SetChecked(ElementState& element, bool checked)
            {
                if (element.Get("Checked").AsBoolean() == checked)
                {
                    return;
                }
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                element.Set("Checked", DataValue(checked));
                std::function<void(bool)> callback = element.Object<std::function<void(bool)>>("OnChange");
                if (callback)
                {
                    callback(checked);
                }
            }

            bool IsToggle;
            float Amount = 0.0f;
            bool Started = false;
        };

        Property<bool> CheckedProperty(std::shared_ptr<ElementState>& state)
        {
            return Property<bool>(
                &state,
                [](const void* owner) -> bool {
                    ElementState* element = StateOf(owner);
                    return element && element->Get("Checked").AsBoolean();
                },
                [](void* owner, const bool& value) {
                    if (ElementState* element = StateOf(owner))
                    {
                        CheckBehavior::SetChecked(*element, value);
                    }
                });
        }

        void ApplyCheckSettings(ElementState& element, std::string text, const CheckSettings& settings)
        {
            const CheckSettings defaults {};
            ApplyCommonSettings(element, settings);
            Choose(element, "Checked", settings.Checked, defaults.Checked);
            Choose(element, "FontSize", settings.FontSize, defaults.FontSize);
            Choose(element, "Color", settings.Color, defaults.Color);
            if (!text.empty())
            {
                element.Set("Text", DataValue(std::move(text)));
            }
            if (settings.OnChange)
            {
                element.SetObject("OnChange", settings.OnChange);
            }
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Checkbox::Checkbox(std::string text, const CheckSettings& settings)
        : Checkbox(MakeElement(std::make_unique<CheckBehavior>(false)))
    {
        ApplyCheckSettings(*State, std::move(text), settings);
    }

    Checkbox::Checkbox(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Text(Stored<std::string, "Text">(State)), Checked(CheckedProperty(State)),
          FontSize(Stored<float, "FontSize">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          OnChange(Kept<std::function<void(bool)>, "OnChange">(State))
    {
    }

    Checkbox::Checkbox(const Checkbox& other) : Checkbox(other.State)
    {
    }

    Checkbox& Checkbox::operator=(const Checkbox& other)
    {
        State = other.State;
        return *this;
    }

    Toggle::Toggle(std::string text, const CheckSettings& settings) : Toggle(MakeElement(std::make_unique<CheckBehavior>(true)))
    {
        ApplyCheckSettings(*State, std::move(text), settings);
    }

    Toggle::Toggle(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Text(Stored<std::string, "Text">(State)), Checked(CheckedProperty(State)),
          FontSize(Stored<float, "FontSize">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          OnChange(Kept<std::function<void(bool)>, "OnChange">(State))
    {
    }

    Toggle::Toggle(const Toggle& other) : Toggle(other.State)
    {
    }

    Toggle& Toggle::operator=(const Toggle& other)
    {
        State = other.State;
        return *this;
    }
}
