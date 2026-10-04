#include <easyforge/ui/Elements.h>

#include <cmath>
#include <format>

#include <easyforge/core/Log.h>

#include "Behavior.h"
#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"
#include "Text.h"

namespace easyforge::ui
{
    // ---- Binding ------------------------------------------------------------------

    Binding::Binding(const Cell& cell, std::string_view format) : Source(cell), Pattern(format)
    {
    }

    DataValue Binding::Value() const
    {
        return Source ? Source->Get() : DataValue();
    }

    std::string Binding::Text() const
    {
        if (!Source)
        {
            return {};
        }
        DataValue value = Source->Get();
        try
        {
            return std::vformat(Pattern, std::make_format_args(value));
        }
        catch (const std::format_error&)
        {
            return Pattern;
        }
    }
}

namespace easyforge::ui::internal
{
    namespace
    {
        class LabelBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Label::KindName; }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint) override
            {
                easyforge::Font font = FontOf(element, context);
                if (!font)
                {
                    return {};
                }
                float size = FontSizeOf(element, context);
                std::string text = element.Get("Text").AsText();
                if (element.Get("Wrap").AsBoolean() && std::isfinite(constraint.Available.X))
                {
                    text = WrapText(font, text, size, constraint.Available.X);
                }
                Vector2 measured = font.Measure(text, size);
                if (text.empty())
                {
                    measured.Y = font.LineHeight(size);
                }
                return measured;
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                easyforge::Font font = FontOf(element, context);
                std::string text = element.Get("Text").AsText();
                if (!font || text.empty())
                {
                    return;
                }
                const Style& style = element.CurrentStyle();
                Rectangle content { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
                    element.Frame.Width - style.Padding.Horizontal(), element.Frame.Height - style.Padding.Vertical() };
                float size = FontSizeOf(element, context);
                if (element.Get("Wrap").AsBoolean())
                {
                    text = WrapText(font, text, size, content.Width);
                }
                std::optional<Color> color = FromData<std::optional<Color>>(element.Get("Color"));
                Color shown = element.IsEnabled() ? color.value_or(context.Theme->Text) : context.Theme->DisabledText;
                DrawTextIn(context.Canvas, font, text, size, shown, content,
                    FromData<TextAlignment>(element.Get("TextAlignment")), true);
            }

            void Update(ElementState& element, Context&) override
            {
                auto found = element.Objects.find("Binding");
                if (found == element.Objects.end())
                {
                    return;
                }
                const ui::Binding* binding = std::any_cast<ui::Binding>(&found->second);
                if (!binding || !*binding)
                {
                    return;
                }
                std::string text = binding->Text();
                if (text != element.Get("Text").AsText())
                {
                    element.Set("Text", DataValue(text));
                }
            }
        };

        Property<std::string> LabelText(std::shared_ptr<ElementState>& state)
        {
            return Property<std::string>(
                &state,
                [](const void* owner) -> std::string {
                    ElementState* element = StateOf(owner);
                    return element ? element->Get("Text").AsText() : std::string();
                },
                [](void* owner, const std::string& value) {
                    if (ElementState* element = StateOf(owner))
                    {
                        element->ClearObject("Binding");
                        element->Set("Text", DataValue(value));
                    }
                });
        }

        Property<ui::Binding> LabelBinding(std::shared_ptr<ElementState>& state)
        {
            return Property<ui::Binding>(
                &state,
                [](const void* owner) -> ui::Binding {
                    ElementState* element = StateOf(owner);
                    return element ? element->Object<ui::Binding>("Binding") : ui::Binding();
                },
                [](void* owner, const ui::Binding& value) {
                    if (ElementState* element = StateOf(owner))
                    {
                        element->SetObject("Binding", value);
                        element->Set("Text", DataValue(value.Text()));
                    }
                });
        }

        void ApplyLabelSettings(ElementState& element, const LabelSettings& settings)
        {
            const LabelSettings defaults {};
            ApplyCommonSettings(element, settings);
            Choose(element, "FontSize", settings.FontSize, defaults.FontSize);
            Choose(element, "Color", settings.Color, defaults.Color);
            Choose(element, "TextAlignment", settings.TextAlignment, defaults.TextAlignment);
            Choose(element, "Wrap", settings.Wrap, defaults.Wrap);
            if (settings.Font)
            {
                element.SetObject("Font", settings.Font);
            }
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Label::Label(std::string text, const LabelSettings& settings) : Label(MakeElement(std::make_unique<LabelBehavior>()))
    {
        ApplyLabelSettings(*State, settings);
        if (!text.empty())
        {
            State->Set("Text", DataValue(std::move(text)));
        }
    }

    Label::Label(const ui::Binding& binding, const LabelSettings& settings)
        : Label(MakeElement(std::make_unique<LabelBehavior>()))
    {
        ApplyLabelSettings(*State, settings);
        State->SetObject("Binding", binding);
        State->Set("Text", DataValue(binding.Text()));
    }

    Label::Label(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Text(LabelText(State)), FontSize(Stored<float, "FontSize">(State)),
          Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          TextAlignment(Stored<ui::TextAlignment, "TextAlignment">(State)), Wrap(Stored<bool, "Wrap">(State)),
          Font(Kept<easyforge::Font, "Font", true>(State)), Binding(LabelBinding(State))
    {
    }

    Label::Label(const Label& other) : Label(other.State)
    {
    }

    Label& Label::operator=(const Label& other)
    {
        State = other.State;
        return *this;
    }
}
