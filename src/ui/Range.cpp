#include <easyforge/ui/Elements.h>

#include <cmath>

#include "Behavior.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        DataValue RangeDefault(std::string_view property)
        {
            if (property == "Width")
            {
                return DataValue(200.0);
            }
            if (property == "Value" || property == "Minimum" || property == "Step")
            {
                return DataValue(0.0);
            }
            if (property == "Maximum")
            {
                return DataValue(1.0);
            }
            return DataValue();
        }

        // How far along the range a value is, from 0 to 1.
        float Fraction(ElementState& element, float value)
        {
            float minimum = element.Get("Minimum").As<float>();
            float maximum = element.Get("Maximum").As<float>();
            return maximum > minimum ? Clamp((value - minimum) / (maximum - minimum), 0.0f, 1.0f) : 0.0f;
        }

        // A value kept inside the range and on a step.
        float Settle(ElementState& element, float value)
        {
            float minimum = element.Get("Minimum").As<float>();
            float maximum = element.Get("Maximum").As<float>();
            float step = element.Get("Step").As<float>();
            if (maximum < minimum)
            {
                std::swap(minimum, maximum);
            }
            if (step > 0.0f)
            {
                value = minimum + std::round((value - minimum) / step) * step;
            }
            return Clamp(value, minimum, maximum);
        }

        void SetValue(ElementState& element, float value)
        {
            value = Settle(element, value);
            if (element.Get("Value").As<float>() == value)
            {
                return;
            }
            std::shared_ptr<ElementState> kept = element.shared_from_this();
            element.Set("Value", DataValue(value));
            std::function<void(float)> callback = element.Object<std::function<void(float)>>("OnChange");
            if (callback)
            {
                callback(value);
            }
        }

        constexpr float ThumbRadius = 10.0f;

        class SliderBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Slider::KindName; }

            DataValue Default(std::string_view property) const override
            {
                DataValue value = RangeDefault(property);
                return value.IsNothing() ? Behavior::Default(property) : value;
            }

            Vector2 MeasureContent(ElementState&, Context&, const Constraint&) override { return { 0.0f, ThumbRadius * 2.0f }; }

            // The track runs between the thumb's centers at either end.
            Rectangle Track(ElementState& element) const
            {
                const Style& style = element.CurrentStyle();
                float left = element.Frame.X + style.Padding.Left + ThumbRadius;
                float right = element.Frame.Right() - style.Padding.Right - ThumbRadius;
                float middle = element.Frame.Y + style.Padding.Top +
                               (element.Frame.Height - style.Padding.Vertical()) * 0.5f;
                return { left, middle, Max(right - left, 0.0f), 0.0f };
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                const Theme& theme = *context.Theme;
                const Canvas& canvas = context.Canvas;
                bool enabled = element.IsEnabled();
                Rectangle track = Track(element);
                float fraction = Fraction(element, element.Get("Value").As<float>());
                std::optional<Color> chosen = FromData<std::optional<Color>>(element.Get("Color"));
                Color accent = enabled ? chosen.value_or(theme.Accent) : theme.DisabledText;

                canvas.Rectangle({ .Position = { track.X - 2.0f, track.Y - 2.0f }, .Size = { track.Width + 4.0f, 4.0f },
                    .Color = theme.Border, .CornerRadius = 2.0f });
                canvas.Rectangle({ .Position = { track.X - 2.0f, track.Y - 2.0f }, .Size = { track.Width * fraction + 4.0f, 4.0f },
                    .Color = accent, .CornerRadius = 2.0f });

                Vector2 center { track.X + track.Width * fraction, track.Y };
                canvas.Circle(center, ThumbRadius, { .Color = theme.Surface, .BorderWidth = 1.0f, .BorderColor = theme.Border });
                float inner = 5.0f + (enabled ? element.HoverAmount * 1.5f - element.PressAmount * 2.0f : 0.0f);
                canvas.Circle(center, Max(inner, 3.0f), { .Color = accent });
            }

            bool TakesPointer(ElementState&) const override { return true; }
            bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }

            void FollowPointer(ElementState& element, Vector2 position)
            {
                Rectangle track = Track(element);
                float fraction = track.Width > 0.0f ? Clamp((position.X - track.X) / track.Width, 0.0f, 1.0f) : 0.0f;
                float minimum = element.Get("Minimum").As<float>();
                float maximum = element.Get("Maximum").As<float>();
                SetValue(element, minimum + (maximum - minimum) * fraction);
            }

            void PointerPressed(ElementState& element, Context&, const Pointer& pointer) override
            {
                FollowPointer(element, pointer.Position);
            }

            void PointerMoved(ElementState& element, Context&, const Pointer& pointer) override
            {
                if (element.Pressed)
                {
                    FollowPointer(element, pointer.Position);
                }
            }

            bool KeyPressed(ElementState& element, Context&, const Event& event) override
            {
                float minimum = element.Get("Minimum").As<float>();
                float maximum = element.Get("Maximum").As<float>();
                float step = element.Get("Step").As<float>();
                if (step <= 0.0f)
                {
                    step = (maximum - minimum) / 100.0f;
                }
                float value = element.Get("Value").As<float>();
                switch (event.Key)
                {
                case Key::Left:
                case Key::Down: SetValue(element, value - step); return true;
                case Key::Right:
                case Key::Up: SetValue(element, value + step); return true;
                case Key::PageDown: SetValue(element, value - step * 10.0f); return true;
                case Key::PageUp: SetValue(element, value + step * 10.0f); return true;
                case Key::Home: SetValue(element, minimum); return true;
                case Key::End: SetValue(element, maximum); return true;
                default: return false;
                }
            }

            bool Wheel(ElementState& element, Context&, Vector2 amount) override
            {
                if (!element.Focused || amount.Y == 0.0f)
                {
                    return false;
                }
                float minimum = element.Get("Minimum").As<float>();
                float maximum = element.Get("Maximum").As<float>();
                float step = element.Get("Step").As<float>();
                step = step > 0.0f ? step : (maximum - minimum) / 100.0f;
                SetValue(element, element.Get("Value").As<float>() + (amount.Y > 0.0f ? step : -step));
                return true;
            }
        };

        class ProgressBarBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return ProgressBar::KindName; }

            DataValue Default(std::string_view property) const override
            {
                DataValue value = RangeDefault(property);
                return value.IsNothing() ? Behavior::Default(property) : value;
            }

            Vector2 MeasureContent(ElementState&, Context&, const Constraint&) override { return { 0.0f, 6.0f }; }

            void Draw(ElementState& element, DrawContext& context) override
            {
                const Theme& theme = *context.Theme;
                const Style& style = element.CurrentStyle();
                Rectangle content { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
                    Max(element.Frame.Width - style.Padding.Horizontal(), 0.0f),
                    Max(element.Frame.Height - style.Padding.Vertical(), 0.0f) };
                float radius = style.CornerRadius.value_or(content.Height * 0.5f);
                float fraction = Fraction(element, element.Get("Value").As<float>());
                std::optional<Color> chosen = FromData<std::optional<Color>>(element.Get("Color"));
                context.Canvas.Rectangle({ .Position = content.Position(), .Size = content.Size(), .Color = theme.Border,
                    .CornerRadius = radius });
                if (fraction > 0.0f)
                {
                    context.Canvas.Rectangle({ .Position = content.Position(),
                        .Size = { Max(content.Width * fraction, content.Height), content.Height },
                        .Color = element.IsEnabled() ? chosen.value_or(theme.Accent) : theme.DisabledText, .CornerRadius = radius });
                }
            }
        };

        Property<float> ValueProperty(std::shared_ptr<ElementState>& state)
        {
            return Property<float>(
                &state,
                [](const void* owner) -> float {
                    ElementState* element = StateOf(owner);
                    return element ? element->Get("Value").As<float>() : 0.0f;
                },
                [](void* owner, const float& value) {
                    if (ElementState* element = StateOf(owner))
                    {
                        SetValue(*element, value);
                    }
                });
        }

        void ApplyRangeSettings(ElementState& element, const RangeSettings& settings)
        {
            const RangeSettings defaults {};
            ApplyCommonSettings(element, settings);
            Choose(element, "Minimum", settings.Minimum, defaults.Minimum);
            Choose(element, "Maximum", settings.Maximum, defaults.Maximum);
            Choose(element, "Step", settings.Step, defaults.Step);
            Choose(element, "Color", settings.Color, defaults.Color);
            Choose(element, "Value", Settle(element, settings.Value), defaults.Value);
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

    Slider::Slider(const RangeSettings& settings) : Slider(MakeElement(std::make_unique<SliderBehavior>()))
    {
        ApplyRangeSettings(*State, settings);
    }

    Slider::Slider(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Value(ValueProperty(State)), Minimum(Stored<float, "Minimum">(State)),
          Maximum(Stored<float, "Maximum">(State)), Step(Stored<float, "Step">(State)),
          Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          OnChange(Kept<std::function<void(float)>, "OnChange">(State))
    {
    }

    Slider::Slider(const Slider& other) : Slider(other.State)
    {
    }

    Slider& Slider::operator=(const Slider& other)
    {
        State = other.State;
        return *this;
    }

    ProgressBar::ProgressBar(const RangeSettings& settings) : ProgressBar(MakeElement(std::make_unique<ProgressBarBehavior>()))
    {
        ApplyRangeSettings(*State, settings);
    }

    ProgressBar::ProgressBar(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Value(StoredAnimated<float, "Value">(State)), Minimum(Stored<float, "Minimum">(State)),
          Maximum(Stored<float, "Maximum">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State))
    {
    }

    ProgressBar::ProgressBar(const ProgressBar& other) : ProgressBar(other.State)
    {
    }

    ProgressBar& ProgressBar::operator=(const ProgressBar& other)
    {
        State = other.State;
        return *this;
    }
}
