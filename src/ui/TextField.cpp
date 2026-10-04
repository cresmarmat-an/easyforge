#include <easyforge/ui/Elements.h>

#include <cmath>

#include "Drawing.h"
#include "Editing.h"
#include "Properties.h"
#include "RootState.h"
#include "Text.h"

namespace easyforge::ui::internal
{
    namespace
    {
        constexpr std::string_view Bullet = "\xE2\x80\xA2";

        class TextFieldBehavior final : public EditingBehavior
        {
        public:
            std::string_view Name() const override { return TextField::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width")
                {
                    return DataValue(200.0);
                }
                if (property == "Password")
                {
                    return DataValue(false);
                }
                return EditingBehavior::Default(property);
            }

            bool CopyAllowed(ElementState& element) const override { return !element.Get("Password").AsBoolean(); }

            // What is drawn: the text, or a dot for each character of a password.
            static std::string Shown(ElementState& element, const std::string& text)
            {
                if (!element.Get("Password").AsBoolean())
                {
                    return text;
                }
                std::string dots;
                for (std::size_t count = CountCharacters(text); count > 0; --count)
                {
                    dots += Bullet;
                }
                return dots;
            }

            // A byte in the text as a byte in what is drawn.
            static std::size_t ShownIndex(ElementState& element, const std::string& text, std::size_t index)
            {
                if (!element.Get("Password").AsBoolean())
                {
                    return index;
                }
                return CountCharacters(std::string_view(text).substr(0, index)) * Bullet.size();
            }

            static float XOf(ElementState& element, const Context& context, const std::string& text, std::size_t index)
            {
                easyforge::Font font = FontOf(element, context);
                if (!font)
                {
                    return 0.0f;
                }
                std::string shown = Shown(element, text);
                return font.Measure(std::string_view(shown).substr(0, ShownIndex(element, text, index)),
                    FontSizeOf(element, context)).X;
            }

            // The one line is centered in the field.
            Vector2 PlaceOf(ElementState& element, const Context& context, std::size_t index) override
            {
                float top = (Content(element).Height - LineHeight(element, context)) * 0.5f;
                return { XOf(element, context, Text(element), index), top };
            }

            std::size_t IndexAt(ElementState& element, const Context& context, Vector2 point) override
            {
                std::string text = Text(element);
                std::size_t best = 0;
                float bestDistance = std::abs(point.X);
                for (std::size_t index = 0; index < text.size();)
                {
                    index = NextCharacter(text, index);
                    float distance = std::abs(XOf(element, context, text, index) - point.X);
                    if (distance < bestDistance)
                    {
                        best = index;
                        bestDistance = distance;
                    }
                }
                return best;
            }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint&) override
            {
                return { 0.0f, LineHeight(element, context) };
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                const Theme& theme = *context.Theme;
                const Canvas& canvas = context.Canvas;
                easyforge::Font font = FontOf(element, context);
                if (!font)
                {
                    return;
                }
                float size = FontSizeOf(element, context);
                Rectangle content = Content(element);
                std::string text = Text(element);
                std::string shown = Shown(element, text);
                bool enabled = element.IsEnabled();

                // Keep the caret in view.
                float caretX = XOf(element, context, text, Caret);
                if (caretX - Scrolled.X > content.Width - 1.0f)
                {
                    Scrolled.X = caretX - content.Width + 1.0f;
                }
                if (caretX - Scrolled.X < 0.0f)
                {
                    Scrolled.X = caretX;
                }
                Scrolled.X = Max(Min(Scrolled.X, font.Measure(shown, size).X - content.Width + 1.0f), 0.0f);

                canvas.PushClip({ content.X - 1.0f, element.Frame.Y, content.Width + 2.0f, element.Frame.Height });
                float lineHeight = LineHeight(element, context);
                float top = content.Y + (content.Height - lineHeight) * 0.5f;
                float left = content.X - Scrolled.X;

                if (element.Focused && HasSelection())
                {
                    float from = XOf(element, context, text, SelectionStart());
                    float to = XOf(element, context, text, SelectionEnd());
                    canvas.Rectangle({ .Position = { left + from, top }, .Size = { to - from, lineHeight }, .Color = theme.Selection });
                }

                std::optional<Color> chosen = FromData<std::optional<Color>>(element.Get("Color"));
                Color color = enabled ? chosen.value_or(theme.Text) : theme.DisabledText;
                if (!Composition.empty())
                {
                    // What an input method is composing, shown at the caret and underlined.
                    std::size_t at = ShownIndex(element, text, Caret);
                    DrawComposing(canvas, font, size, color, shown, at, Composition, { left, top }, lineHeight);
                }
                else if (text.empty())
                {
                    std::string placeholder = element.Get("Placeholder").AsText();
                    canvas.Text(font, placeholder, { .Position = { left, top }, .Size = size, .Color = theme.MutedText });
                }
                else
                {
                    canvas.Text(font, shown, { .Position = { left, top }, .Size = size, .Color = color });
                }

                if (CaretShown(element, context))
                {
                    float x = std::round(left + caretX);
                    canvas.Rectangle({ .Position = { x, top }, .Size = { 1.0f, lineHeight }, .Color = color });
                }
                canvas.PopClip();
            }
        };

        TextFieldBehavior* FieldOf(const std::shared_ptr<ElementState>& state)
        {
            return state ? dynamic_cast<TextFieldBehavior*>(state->Kind.get()) : nullptr;
        }

        Property<std::string> FieldText(std::shared_ptr<ElementState>& state)
        {
            return Property<std::string>(
                &state,
                [](const void* owner) -> std::string {
                    ElementState* element = StateOf(owner);
                    return element ? element->Get("Text").AsText() : std::string();
                },
                [](void* owner, const std::string& value) {
                    std::shared_ptr<ElementState>& element = SharedStateOf(owner);
                    if (TextFieldBehavior* field = FieldOf(element))
                    {
                        field->Assign(*element, value);
                    }
                });
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    TextField::TextField(const TextFieldSettings& settings) : TextField(MakeElement(std::make_unique<TextFieldBehavior>()))
    {
        const TextFieldSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Placeholder", settings.Placeholder, defaults.Placeholder);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
        Choose(*State, "Password", settings.Password, defaults.Password);
        Choose(*State, "MaximumLength", settings.MaximumLength, defaults.MaximumLength);
        if (!settings.Text.empty())
        {
            State->Set("Text", DataValue(settings.Text));
            FieldOf(State)->Caret = settings.Text.size();
            FieldOf(State)->Anchor = settings.Text.size();
        }
        if (settings.OnChange)
        {
            State->SetObject("OnChange", settings.OnChange);
        }
        if (settings.OnSubmit)
        {
            State->SetObject("OnSubmit", settings.OnSubmit);
        }
    }

    TextField::TextField(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Text(FieldText(State)), Placeholder(Stored<std::string, "Placeholder">(State)),
          FontSize(Stored<float, "FontSize">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          Password(Stored<bool, "Password">(State)), MaximumLength(Stored<int, "MaximumLength">(State)),
          OnChange(Kept<std::function<void(const std::string&)>, "OnChange">(State)),
          OnSubmit(Kept<std::function<void(const std::string&)>, "OnSubmit">(State))
    {
    }

    TextField::TextField(const TextField& other) : TextField(other.State)
    {
    }

    TextField& TextField::operator=(const TextField& other)
    {
        State = other.State;
        return *this;
    }

    void TextField::SelectAll() const
    {
        if (TextFieldBehavior* field = FieldOf(State))
        {
            field->Anchor = 0;
            field->Caret = State->Get("Text").AsText().size();
        }
    }
}
