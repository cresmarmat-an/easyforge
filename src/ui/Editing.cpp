#include "Editing.h"

#include <cmath>

#include "Properties.h"
#include "RootState.h"
#include "Text.h"

namespace easyforge::ui::internal
{
    namespace
    {
        // How many edits undo remembers.
        constexpr std::size_t UndoLimit = 100;
    }

    void DrawComposing(const Canvas& canvas, const Font& font, float size, Color color, std::string_view text,
        std::size_t at, std::string_view composing, Vector2 position, float lineHeight)
    {
        std::string_view before = text.substr(0, at);
        float beforeWidth = font.Measure(before, size).X;
        float composingWidth = font.Measure(composing, size).X;
        canvas.Text(font, before, { .Position = position, .Size = size, .Color = color });
        canvas.Text(font, composing, { .Position = { position.X + beforeWidth, position.Y }, .Size = size, .Color = color });
        canvas.Text(font, text.substr(at), { .Position = { position.X + beforeWidth + composingWidth, position.Y }, .Size = size,
                                              .Color = color });
        float underline = position.Y + lineHeight - 1.0f;
        canvas.Line({ position.X + beforeWidth, underline }, { position.X + beforeWidth + composingWidth, underline },
            { .Color = color, .Width = 1.0f });
    }

    DataValue EditingBehavior::Default(std::string_view property) const
    {
        if (property == "Padding")
        {
            return DataValue(Vector4 { 8.0f, 6.0f, 8.0f, 6.0f });
        }
        if (property == "Placeholder")
        {
            return DataValue("");
        }
        if (property == "MaximumLength")
        {
            return DataValue(0);
        }
        if (property == "BorderWidth")
        {
            // Without one of its own, the theme's.
            return DataValue();
        }
        return Behavior::Default(property);
    }

    Rectangle EditingBehavior::Content(ElementState& element)
    {
        const Style& style = element.CurrentStyle();
        return { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
            Max(element.Frame.Width - style.Padding.Horizontal(), 0.0f),
            Max(element.Frame.Height - style.Padding.Vertical(), 0.0f) };
    }

    float EditingBehavior::LineHeight(ElementState& element, const Context& context) const
    {
        easyforge::Font font = FontOf(element, context);
        return font ? font.LineHeight(FontSizeOf(element, context)) : 16.0f;
    }

    // ---- Changing the text ----------------------------------------------------------

    void EditingBehavior::Remember(ElementState& element, bool typing)
    {
        if (typing && LastEditWasTyping && !Undone.empty())
        {
            return;
        }
        Undone.push_back({ Text(element), Caret });
        if (Undone.size() > UndoLimit)
        {
            Undone.erase(Undone.begin());
        }
        Redone.clear();
        LastEditWasTyping = typing;
    }

    void EditingBehavior::Replace(ElementState& element, std::string_view insert, bool typing)
    {
        std::string text = Text(element);
        std::size_t start = Min(SelectionStart(), text.size());
        std::size_t end = Min(SelectionEnd(), text.size());
        int limit = element.Get("MaximumLength").As<int>();
        std::string added(insert);
        if (limit > 0)
        {
            std::size_t room = static_cast<std::size_t>(limit) -
                               Min(CountCharacters(text) - CountCharacters(std::string_view(text).substr(start, end - start)),
                                   static_cast<std::size_t>(limit));
            std::size_t cut = 0;
            for (std::size_t kept = 0; kept < room && cut < added.size(); ++kept)
            {
                cut = NextCharacter(added, cut);
            }
            added = added.substr(0, cut);
        }
        if (start == end && added.empty())
        {
            return;
        }
        Remember(element, typing);
        text.replace(start, end - start, added);
        Caret = start + added.size();
        Anchor = Caret;
        Column.reset();
        Commit(element, text);
    }

    void EditingBehavior::Commit(ElementState& element, const std::string& text)
    {
        std::shared_ptr<ElementState> kept = element.shared_from_this();
        Writing = true;
        element.Set("Text", DataValue(text));
        Writing = false;
        BlinkStart = LastTime;
        std::function<void(const std::string&)> changed = element.Object<std::function<void(const std::string&)>>("OnChange");
        if (changed)
        {
            changed(text);
        }
    }

    void EditingBehavior::Undo(ElementState& element, bool redo)
    {
        std::vector<std::pair<std::string, std::size_t>>& from = redo ? Redone : Undone;
        std::vector<std::pair<std::string, std::size_t>>& to = redo ? Undone : Redone;
        if (from.empty())
        {
            return;
        }
        to.push_back({ Text(element), Caret });
        auto [text, caret] = from.back();
        from.pop_back();
        Caret = Min(caret, text.size());
        Anchor = Caret;
        Column.reset();
        LastEditWasTyping = false;
        Commit(element, text);
    }

    void EditingBehavior::Assign(ElementState& element, const std::string& text)
    {
        if (Text(element) == text)
        {
            return;
        }
        Caret = text.size();
        Anchor = Caret;
        Column.reset();
        Commit(element, text);
    }

    bool EditingBehavior::CaretShown(ElementState& element, const Context& context) const
    {
        return element.Focused && element.IsEnabled() && std::fmod(Max(context.Time - BlinkStart, 0.0f), 1.0f) < 0.5f;
    }

    void EditingBehavior::MoveCaret(ElementState&, const Context& context, std::size_t to, bool extend, bool keepColumn)
    {
        Caret = to;
        if (!extend)
        {
            Anchor = to;
        }
        LastEditWasTyping = false;
        BlinkStart = context.Time;
        if (!keepColumn)
        {
            Column.reset();
        }
    }

    // ---- Looks and frames ------------------------------------------------------------

    Box EditingBehavior::LookOf(ElementState& element, const Context& context)
    {
        const Theme& theme = *context.Theme;
        const Style& style = element.CurrentStyle();
        Box box;
        box.CornerRadius = style.CornerRadius.value_or(theme.CornerRadius);
        box.Background = style.Background ? style.Background
                                          : std::optional<ui::Background>(ui::Background(Mix(
                                                theme.Control, theme.ControlHovered, element.IsEnabled() ? element.HoverAmount : 0.0f)));
        std::optional<float> width = FromData<std::optional<float>>(element.Get("BorderWidth"));
        box.BorderWidth = width.value_or(theme.BorderWidth);
        box.BorderColor = element.Focused ? theme.Accent : style.BorderColor.value_or(theme.Border);
        return box;
    }

    void EditingBehavior::Update(ElementState& element, Context& context)
    {
        LastTime = context.Time;
        std::string text = Text(element);
        if (!Writing && text != Known)
        {
            // The program changed the text: keep the caret inside it.
            Caret = Min(Caret, text.size());
            Anchor = Min(Anchor, text.size());
        }
        Known = text;
        if (element.Focused && context.Root && context.Root->TheHost)
        {
            Rectangle content = Content(element);
            Vector2 place = PlaceOf(element, context, Caret);
            Rectangle caret { content.X + place.X - Scrolled.X, content.Y + place.Y - Scrolled.Y, 1.0f,
                LineHeight(element, context) };
            if (!context.Root->TextInputOn || !(caret == TextInputCaret))
            {
                context.Root->TheHost->SetTextInput(true, caret);
                context.Root->TextInputOn = true;
                TextInputCaret = caret;
            }
        }
    }

    // ---- Events -----------------------------------------------------------------------

    void EditingBehavior::PointerPressed(ElementState& element, Context& context, const Pointer& pointer)
    {
        std::string text = Text(element);
        Rectangle content = Content(element);
        std::size_t index = IndexAt(element, context, pointer.Position - content.Position() + Scrolled);
        if (pointer.ClickCount >= 3)
        {
            Anchor = LineStartOf(element, context, index);
            Caret = LineEndOf(element, context, index);
        }
        else if (pointer.ClickCount == 2)
        {
            WordAround(text, index, Anchor, Caret);
        }
        else
        {
            Caret = index;
            if (!pointer.Modifiers.Shift)
            {
                Anchor = index;
            }
        }
        Column.reset();
        BlinkStart = context.Time;
        LastEditWasTyping = false;
    }

    void EditingBehavior::PointerMoved(ElementState& element, Context& context, const Pointer& pointer)
    {
        if (element.Pressed)
        {
            Caret = IndexAt(element, context, pointer.Position - Content(element).Position() + Scrolled);
            BlinkStart = context.Time;
        }
    }

    bool EditingBehavior::KeyPressed(ElementState& element, Context& context, const Event& event)
    {
        // While an input method composes, the keys are its own.
        if (!Composition.empty())
        {
            return true;
        }
        std::string text = Text(element);
        bool shift = event.Modifiers.Shift;

        // Control with Alt is AltGr on many keyboards, which types characters.
        bool control = (event.Modifiers.Control && !event.Modifiers.Alt) || event.Modifiers.Meta;
        auto move = [&](std::size_t to) { MoveCaret(element, context, to, shift); };
        switch (event.Key)
        {
        case Key::Left:
            if (HasSelection() && !shift)
            {
                move(SelectionStart());
            }
            else
            {
                move(control ? PreviousWord(text, Caret) : PreviousCharacter(text, Caret));
            }
            return true;
        case Key::Right:
            if (HasSelection() && !shift)
            {
                move(SelectionEnd());
            }
            else
            {
                move(control ? NextWord(text, Caret) : NextCharacter(text, Caret));
            }
            return true;
        case Key::Up:
        case Key::Down:
        case Key::PageUp:
        case Key::PageDown:
        {
            if (!Multiline())
            {
                return true;
            }
            float line = LineHeight(element, context);
            float page = Max(std::floor(Content(element).Height / line) - 1.0f, 1.0f);
            float lines = event.Key == Key::Up ? -1.0f : event.Key == Key::Down ? 1.0f : event.Key == Key::PageUp ? -page : page;
            Vector2 place = PlaceOf(element, context, Caret);
            float x = Column.value_or(place.X);
            float y = place.Y + line * lines + line * 0.5f;
            float bottom = PlaceOf(element, context, text.size()).Y + line;
            std::size_t to = y < 0.0f ? 0 : y >= bottom ? text.size() : IndexAt(element, context, { x, y });
            MoveCaret(element, context, to, shift, true);
            Column = x;
            return true;
        }
        case Key::Home: move(control ? 0 : LineStartOf(element, context, Caret)); return true;
        case Key::End: move(control ? text.size() : LineEndOf(element, context, Caret)); return true;
        case Key::Backspace:
            if (!HasSelection())
            {
                Anchor = control ? PreviousWord(text, Caret) : PreviousCharacter(text, Caret);
            }
            Replace(element, "", false);
            return true;
        case Key::Delete:
            if (!HasSelection())
            {
                Anchor = control ? NextWord(text, Caret) : NextCharacter(text, Caret);
            }
            Replace(element, "", false);
            return true;
        case Key::Enter:
        case Key::NumberPadEnter:
        {
            if (Multiline())
            {
                Replace(element, "\n", false);
                return true;
            }
            std::shared_ptr<ElementState> kept = element.shared_from_this();
            std::function<void(const std::string&)> submit = element.Object<std::function<void(const std::string&)>>("OnSubmit");
            if (submit)
            {
                submit(text);
            }
            return true;
        }
        case Key::Tab:
        case Key::Escape: return false;
        default: break;
        }
        if (control)
        {
            RootState* root = context.Root;
            switch (event.Key)
            {
            case Key::A:
                Anchor = 0;
                Caret = text.size();
                return true;
            case Key::C:
            case Key::X:
                if (HasSelection() && CopyAllowed(element) && root)
                {
                    root->SetClipboardText(text.substr(SelectionStart(), SelectionEnd() - SelectionStart()));
                    if (event.Key == Key::X)
                    {
                        Replace(element, "", false);
                    }
                }
                return true;
            case Key::V:
                if (root)
                {
                    std::string pasted = root->ClipboardText();
                    if (Multiline())
                    {
                        std::erase(pasted, '\r');
                    }
                    else
                    {
                        std::erase_if(pasted, [](char character) { return character == '\n' || character == '\r'; });
                    }
                    Replace(element, pasted, false);
                }
                return true;
            case Key::Z: Undo(element, shift); return true;
            case Key::Y: Undo(element, true); return true;
            default: break;
            }
        }
        // Every other key belongs to the field while it has the keyboard, so
        // typing does not also move a player.
        return true;
    }

    bool EditingBehavior::TextEntered(ElementState& element, Context& context, const Event& event)
    {
        if (event.Type == EventType::TextComposition)
        {
            Composition = event.Text;
            return true;
        }
        std::string typed;
        for (char character : event.Text)
        {
            unsigned char value = static_cast<unsigned char>(character);
            if (value >= 0x20 && value != 0x7F)
            {
                typed += character;
            }
        }
        Composition.clear();
        BlinkStart = context.Time;
        Replace(element, typed, true);
        return true;
    }

    void EditingBehavior::FocusChanged(ElementState& element, Context& context, bool focused)
    {
        BlinkStart = context.Time;
        Composition.clear();
        LastEditWasTyping = false;
        if (!focused)
        {
            Anchor = Caret;
        }
        if (context.Root && context.Root->TheHost)
        {
            Rectangle caret = Content(element);
            context.Root->TheHost->SetTextInput(focused, { caret.X, caret.Y, 1.0f, LineHeight(element, context) });
            context.Root->TextInputOn = focused;
        }
    }
}
