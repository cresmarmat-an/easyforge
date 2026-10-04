#pragma once

// What text fields and text areas share: the caret and selection, typing,
// copy and paste, undo, and input methods. Each kind decides where text is
// placed and how it is drawn.

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Behavior.h"

namespace easyforge::ui::internal
{
    // Draws a line of text with what an input method is composing put in at a
    // byte, underlined.
    void DrawComposing(const Canvas& canvas, const Font& font, float size, Color color, std::string_view text,
        std::size_t at, std::string_view composing, Vector2 position, float lineHeight);

    class EditingBehavior : public Behavior
    {
    public:
        DataValue Default(std::string_view property) const override;
        Box LookOf(ElementState& element, const Context& context) override;
        void Update(ElementState& element, Context& context) override;

        bool TakesPointer(ElementState&) const override { return true; }
        bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }
        bool ShowsFocusRing() const override { return false; }
        std::optional<easyforge::Cursor> PointerCursor(ElementState&) const override { return easyforge::Cursor::Text; }

        void PointerPressed(ElementState& element, Context& context, const Pointer& pointer) override;
        void PointerMoved(ElementState& element, Context& context, const Pointer& pointer) override;
        bool KeyPressed(ElementState& element, Context& context, const Event& event) override;
        bool TextEntered(ElementState& element, Context& context, const Event& event) override;
        void FocusChanged(ElementState& element, Context& context, bool focused) override;

        // ---- What each kind decides ---------------------------------------------

        // Whether Enter starts a new line, and pasted line breaks are kept.
        virtual bool Multiline() const { return false; }

        // Whether the selection may be copied: not from a password.
        virtual bool CopyAllowed(ElementState&) const { return true; }

        // Where the caret for a byte of the text is, from the top left of the text
        // before scrolling: the left of the character and the top of its line.
        virtual Vector2 PlaceOf(ElementState& element, const Context& context, std::size_t index) = 0;

        // The byte nearest a point, measured the same way.
        virtual std::size_t IndexAt(ElementState& element, const Context& context, Vector2 point) = 0;

        // Where the line holding a byte starts and ends, for Home and End.
        virtual std::size_t LineStartOf(ElementState&, const Context&, std::size_t) { return 0; }
        virtual std::size_t LineEndOf(ElementState& element, const Context&, std::size_t) { return Text(element).size(); }

        // ---- Shared ---------------------------------------------------------------

        static std::string Text(ElementState& element) { return element.Get("Text").AsText(); }

        // The frame inside the padding.
        static Rectangle Content(ElementState& element);

        float LineHeight(ElementState& element, const Context& context) const;

        bool HasSelection() const { return Caret != Anchor; }
        std::size_t SelectionStart() const { return Min(Caret, Anchor); }
        std::size_t SelectionEnd() const { return Max(Caret, Anchor); }

        // Replaces the selection, as typing or pasting does.
        void Replace(ElementState& element, std::string_view insert, bool typing);

        // Writes new text and tells OnChange.
        void Commit(ElementState& element, const std::string& text);

        void Undo(ElementState& element, bool redo);

        // Text the program assigned: the caret goes to its end.
        void Assign(ElementState& element, const std::string& text);

        // Whether the caret blinks on in this frame.
        bool CaretShown(ElementState& element, const Context& context) const;

        std::size_t Caret = 0;
        std::size_t Anchor = 0;

        // How far the text is scrolled, in points.
        Vector2 Scrolled;

        std::string Composition;
        float BlinkStart = 0.0f;

    protected:
        void Remember(ElementState& element, bool typing);
        void MoveCaret(ElementState& element, const Context& context, std::size_t to, bool extend, bool keepColumn = false);

        std::string Known;
        float LastTime = 0.0f;
        bool Writing = false;
        bool LastEditWasTyping = false;
        Rectangle TextInputCaret;

        // Where Up and Down aim across lines, so the caret keeps its column
        // through shorter lines.
        std::optional<float> Column;

        std::vector<std::pair<std::string, std::size_t>> Undone;
        std::vector<std::pair<std::string, std::size_t>> Redone;
    };
}
