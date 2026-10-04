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
        // A line as drawn: a range of bytes in the text, without its line break.
        struct Line
        {
            std::size_t Start = 0;
            std::size_t End = 0;
        };

        class TextAreaBehavior final : public EditingBehavior
        {
        public:
            std::string_view Name() const override { return TextArea::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width")
                {
                    return DataValue(300.0);
                }
                if (property == "Height")
                {
                    return DataValue(120.0);
                }
                if (property == "Wrap")
                {
                    return DataValue(true);
                }
                return EditingBehavior::Default(property);
            }

            bool Multiline() const override { return true; }

            // ---- Lines ------------------------------------------------------------

            float WrapWidth(ElementState& element, float width) const
            {
                return element.Get("Wrap").AsBoolean() && std::isfinite(width) ? Max(width, 1.0f) : Unlimited;
            }

            // The lines for a width, kept until the text, width, or font size changes.
            const std::vector<Line>& LinesFor(ElementState& element, const Context& context, float width)
            {
                std::string text = Text(element);
                easyforge::Font font = FontOf(element, context);
                float size = FontSizeOf(element, context);
                if (Built && text == LinesText && width == LinesWidth && size == LinesSize)
                {
                    return Lines;
                }
                Built = true;
                LinesText = text;
                LinesWidth = width;
                LinesSize = size;
                Lines.clear();
                std::size_t start = 0;
                while (true)
                {
                    std::size_t found = text.find('\n', start);
                    std::size_t end = found == std::string::npos ? text.size() : found;
                    Break(font, text, start, end, size, width);
                    if (found == std::string::npos)
                    {
                        break;
                    }
                    start = found + 1;
                }
                return Lines;
            }

            // Breaks one paragraph into lines no wider than `width`, between words
            // where it can, and between characters in a word wider than a line.
            void Break(const easyforge::Font& font, const std::string& text, std::size_t from, std::size_t to, float size,
                float width)
            {
                std::size_t lineStart = from;
                while (true)
                {
                    std::string_view rest = std::string_view(text).substr(lineStart, to - lineStart);
                    if (!font || !std::isfinite(width) || rest.empty() || font.Measure(rest, size).X <= width)
                    {
                        Lines.push_back({ lineStart, to });
                        return;
                    }
                    std::size_t index = lineStart;
                    std::size_t fits = lineStart;
                    std::size_t afterSpace = std::string::npos;
                    while (index < to)
                    {
                        std::size_t next = NextCharacter(text, index);
                        if (font.Measure(std::string_view(text).substr(lineStart, next - lineStart), size).X > width)
                        {
                            break;
                        }
                        fits = next;
                        if (text[index] == ' ')
                        {
                            afterSpace = next;
                        }
                        index = next;
                    }
                    std::size_t cut = 0;
                    if (index < to && text[index] == ' ')
                    {
                        // A space may hang past the edge at the end of a line.
                        cut = NextCharacter(text, index);
                    }
                    else if (afterSpace != std::string::npos && afterSpace > lineStart)
                    {
                        cut = afterSpace;
                    }
                    else
                    {
                        cut = Max(fits, NextCharacter(text, lineStart));
                    }
                    Lines.push_back({ lineStart, cut });
                    if (cut >= to)
                    {
                        return;
                    }
                    lineStart = cut;
                }
            }

            const std::vector<Line>& CurrentLines(ElementState& element, const Context& context)
            {
                return LinesFor(element, context, WrapWidth(element, Content(element).Width));
            }

            // Whether a line ends because it was too wide, rather than at a line break.
            static bool Wrapped(const std::vector<Line>& lines, std::size_t line)
            {
                return line + 1 < lines.size() && lines[line + 1].Start == lines[line].End;
            }

            // The line a byte is on. At the end of a wrapped line, the next line.
            static std::size_t LineOf(const std::vector<Line>& lines, std::size_t index)
            {
                std::size_t found = 0;
                for (std::size_t line = 0; line < lines.size() && lines[line].Start <= index; ++line)
                {
                    found = line;
                }
                return found;
            }

            float XOf(ElementState& element, const Context& context, const Line& line, std::size_t index) const
            {
                easyforge::Font font = FontOf(element, context);
                std::string text = Text(element);
                std::size_t end = Clamp(index, line.Start, line.End);
                return font ? font.Measure(std::string_view(text).substr(line.Start, end - line.Start), FontSizeOf(element, context)).X
                            : 0.0f;
            }

            Vector2 PlaceOf(ElementState& element, const Context& context, std::size_t index) override
            {
                const std::vector<Line>& lines = CurrentLines(element, context);
                std::size_t line = LineOf(lines, index);
                return { XOf(element, context, lines[line], index), static_cast<float>(line) * LineHeight(element, context) };
            }

            std::size_t IndexAt(ElementState& element, const Context& context, Vector2 point) override
            {
                const std::vector<Line>& lines = CurrentLines(element, context);
                float height = LineHeight(element, context);
                std::size_t line = static_cast<std::size_t>(
                    Clamp(std::floor(point.Y / height), 0.0f, static_cast<float>(lines.size() - 1)));
                std::string text = Text(element);
                std::size_t last = Wrapped(lines, line) ? PreviousCharacter(text, lines[line].End) : lines[line].End;
                std::size_t best = lines[line].Start;
                float bestDistance = std::abs(point.X);
                for (std::size_t index = lines[line].Start; index < last;)
                {
                    index = NextCharacter(text, index);
                    float distance = std::abs(XOf(element, context, lines[line], index) - point.X);
                    if (distance < bestDistance)
                    {
                        best = index;
                        bestDistance = distance;
                    }
                }
                return best;
            }

            std::size_t LineStartOf(ElementState& element, const Context& context, std::size_t index) override
            {
                const std::vector<Line>& lines = CurrentLines(element, context);
                return lines[LineOf(lines, index)].Start;
            }

            std::size_t LineEndOf(ElementState& element, const Context& context, std::size_t index) override
            {
                const std::vector<Line>& lines = CurrentLines(element, context);
                std::size_t line = LineOf(lines, index);
                // The end of a wrapped line is the start of the next, so stop
                // before its last character.
                return Wrapped(lines, line) ? PreviousCharacter(Text(element), lines[line].End) : lines[line].End;
            }

            // ---- Layout and drawing --------------------------------------------------

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint) override
            {
                const std::vector<Line>& lines = LinesFor(element, context, WrapWidth(element, constraint.Available.X));
                return { 0.0f, static_cast<float>(lines.size()) * LineHeight(element, context) };
            }

            float Widest(ElementState& element, const Context& context)
            {
                float widest = 0.0f;
                for (const Line& line : CurrentLines(element, context))
                {
                    widest = Max(widest, XOf(element, context, line, line.End));
                }
                return widest;
            }

            // How far the text can scroll on each axis.
            Vector2 Farthest(ElementState& element, const Context& context)
            {
                Rectangle content = Content(element);
                float height = static_cast<float>(CurrentLines(element, context).size()) * LineHeight(element, context);
                float width = std::isfinite(WrapWidth(element, content.Width)) ? 0.0f : Widest(element, context) + 1.0f;
                return { Max(width - content.Width, 0.0f), Max(height - content.Height, 0.0f) };
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
                float lineHeight = LineHeight(element, context);
                Rectangle content = Content(element);
                std::string text = Text(element);
                const std::vector<Line> lines = CurrentLines(element, context);
                bool enabled = element.IsEnabled();

                // Scroll the caret into view when it moved or the text changed, so
                // the wheel can scroll away from it.
                Vector2 caret = PlaceOf(element, context, Caret);
                if (Caret != RevealedCaret || text.size() != RevealedLength)
                {
                    RevealedCaret = Caret;
                    RevealedLength = text.size();
                    Scrolled.Y = Clamp(Scrolled.Y, caret.Y + lineHeight - content.Height, caret.Y);
                    Scrolled.X = Clamp(Scrolled.X, caret.X - content.Width + 1.0f, caret.X);
                }
                Vector2 farthest = Farthest(element, context);
                Scrolled = { Clamp(Scrolled.X, 0.0f, farthest.X), Clamp(Scrolled.Y, 0.0f, farthest.Y) };

                canvas.PushClip({ content.X - 1.0f, content.Y, content.Width + 2.0f, content.Height });
                float left = content.X - Scrolled.X;
                std::optional<Color> chosen = FromData<std::optional<Color>>(element.Get("Color"));
                Color color = enabled ? chosen.value_or(theme.Text) : theme.DisabledText;

                if (text.empty() && Composition.empty())
                {
                    canvas.Text(font, element.Get("Placeholder").AsText(),
                        { .Position = { left, content.Y - Scrolled.Y }, .Size = size, .Color = theme.MutedText });
                }

                std::size_t caretLine = LineOf(lines, Caret);
                std::size_t first = static_cast<std::size_t>(Max(std::floor(Scrolled.Y / lineHeight), 0.0f));
                for (std::size_t index = first; index < lines.size(); ++index)
                {
                    const Line& line = lines[index];
                    float top = content.Y + static_cast<float>(index) * lineHeight - Scrolled.Y;
                    if (top > content.Bottom())
                    {
                        break;
                    }
                    if (element.Focused && HasSelection() && SelectionStart() <= line.End && SelectionEnd() >= line.Start)
                    {
                        float from = XOf(element, context, line, Max(SelectionStart(), line.Start));
                        float to = XOf(element, context, line, Min(SelectionEnd(), line.End));
                        // A selected line break shows as a little space at the end.
                        if (SelectionEnd() > line.End && !Wrapped(lines, index))
                        {
                            to += size * 0.3f;
                        }
                        if (to > from)
                        {
                            canvas.Rectangle({ .Position = { left + from, top }, .Size = { to - from, lineHeight },
                                .Color = theme.Selection });
                        }
                    }
                    std::string_view shown = std::string_view(text).substr(line.Start, line.End - line.Start);
                    if (index == caretLine && !Composition.empty())
                    {
                        DrawComposing(canvas, font, size, color, shown, Caret - line.Start, Composition, { left, top }, lineHeight);
                    }
                    else
                    {
                        canvas.Text(font, shown, { .Position = { left, top }, .Size = size, .Color = color });
                    }
                }

                if (CaretShown(element, context))
                {
                    canvas.Rectangle({ .Position = { std::round(left + caret.X), content.Y + caret.Y - Scrolled.Y },
                        .Size = { 1.0f, lineHeight }, .Color = color });
                }
                canvas.PopClip();

                // A thin bar while there is more text than fits.
                if (farthest.Y > 0.0f)
                {
                    float total = farthest.Y + content.Height;
                    float length = Max(content.Height * content.Height / total, 24.0f);
                    float start = (content.Height - length) * Scrolled.Y / farthest.Y;
                    canvas.Rectangle({ .Position = { element.Frame.Right() - 5.0f, content.Y + start }, .Size = { 3.0f, length },
                        .Color = theme.MutedText.WithAlpha(0.5f), .CornerRadius = 1.5f });
                }
            }

            bool Wheel(ElementState& element, Context& context, Vector2 amount) override
            {
                Vector2 farthest = Farthest(element, context);
                Vector2 before = Scrolled;
                Scrolled.Y = Clamp(Scrolled.Y - amount.Y * 48.0f, 0.0f, farthest.Y);
                Scrolled.X = Clamp(Scrolled.X - amount.X * 48.0f, 0.0f, farthest.X);
                return !(Scrolled == before);
            }

            std::vector<Line> Lines;
            std::string LinesText;
            float LinesWidth = 0.0f;
            float LinesSize = 0.0f;
            bool Built = false;
            std::size_t RevealedCaret = static_cast<std::size_t>(-1);
            std::size_t RevealedLength = 0;
        };

        TextAreaBehavior* AreaOf(const std::shared_ptr<ElementState>& state)
        {
            return state ? dynamic_cast<TextAreaBehavior*>(state->Kind.get()) : nullptr;
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    TextArea::TextArea(const TextAreaSettings& settings) : TextArea(MakeElement(std::make_unique<TextAreaBehavior>()))
    {
        const TextAreaSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Placeholder", settings.Placeholder, defaults.Placeholder);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
        Choose(*State, "Wrap", settings.Wrap, defaults.Wrap);
        Choose(*State, "MaximumLength", settings.MaximumLength, defaults.MaximumLength);
        if (!settings.Text.empty())
        {
            State->Set("Text", DataValue(settings.Text));
            AreaOf(State)->Caret = settings.Text.size();
            AreaOf(State)->Anchor = settings.Text.size();
        }
        if (settings.OnChange)
        {
            State->SetObject("OnChange", settings.OnChange);
        }
    }

    TextArea::TextArea(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)),
          Text(
              &State,
              [](const void* owner) -> std::string {
                  ElementState* element = StateOf(owner);
                  return element ? element->Get("Text").AsText() : std::string();
              },
              [](void* owner, const std::string& value) {
                  std::shared_ptr<ElementState>& element = SharedStateOf(owner);
                  if (TextAreaBehavior* area = AreaOf(element))
                  {
                      area->Assign(*element, value);
                  }
              }),
          Placeholder(Stored<std::string, "Placeholder">(State)), FontSize(Stored<float, "FontSize">(State)),
          Color(Stored<std::optional<easyforge::Color>, "Color">(State)), Wrap(Stored<bool, "Wrap">(State)),
          MaximumLength(Stored<int, "MaximumLength">(State)),
          OnChange(Kept<std::function<void(const std::string&)>, "OnChange">(State))
    {
    }

    TextArea::TextArea(const TextArea& other) : TextArea(other.State)
    {
    }

    TextArea& TextArea::operator=(const TextArea& other)
    {
        State = other.State;
        return *this;
    }

    void TextArea::SelectAll() const
    {
        if (TextAreaBehavior* area = AreaOf(State))
        {
            area->Anchor = 0;
            area->Caret = State->Get("Text").AsText().size();
        }
    }
}
