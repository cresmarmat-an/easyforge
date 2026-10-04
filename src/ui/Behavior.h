#pragma once

// What makes one kind of element different from another: how it measures and
// arranges, what it draws, and how it answers the pointer and the keyboard.

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include <easyforge/core/Event.h>
#include <easyforge/core/Host.h>
#include <easyforge/graphics/Canvas.h>
#include <easyforge/graphics/Font.h>
#include <easyforge/ui/Theme.h>

#include "ElementState.h"

namespace easyforge::ui::internal
{
    // What layout, drawing, and events share.
    struct Context
    {
        RootState* Root = nullptr;
        const ui::Theme* Theme = nullptr;

        // The theme's font, or the system's when the theme has none.
        easyforge::Font Font;

        // Pixels per point.
        float Scale = 1.0f;

        float DeltaSeconds = 0.0f;

        // Seconds since the root was made.
        float Time = 0.0f;

        // Counts layouts, so a measurement is kept only for the layout it was made in.
        std::uint64_t Pass = 0;
    };

    struct DrawContext : Context
    {
        easyforge::Canvas Canvas;

        // Focus rings are shown after the keyboard moved the focus, not after a click.
        bool FocusVisible = false;
    };

    // The space offered when measuring the inside of an element: exact when the
    // element has a fixed size on that axis, a limit otherwise.
    struct Constraint
    {
        Vector2 Available;
        bool ExactWidth = false;
        bool ExactHeight = false;
    };

    struct Pointer
    {
        // In points from the top left of the root.
        Vector2 Position;
        int ClickCount = 1;
        KeyModifiers Modifiers;
        bool Touch = false;
    };

    // The box behind an element's content.
    struct Box
    {
        std::optional<ui::Background> Background;
        float CornerRadius = 0.0f;
        float BorderWidth = 0.0f;
        Color BorderColor = Color::Transparent;
    };

    class Behavior
    {
    public:
        virtual ~Behavior() = default;

        // The kind's name, which is also the type of its node: "Label".
        virtual std::string_view Name() const = 0;

        // A setting's value when the element has none of its own.
        virtual DataValue Default(std::string_view property) const;

        virtual bool HoldsChildren() const { return false; }

        // ---- Layout -----------------------------------------------------------

        // The size of the content inside the padding.
        virtual Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint);

        // Places the children inside the content box. By default each fills it.
        virtual void Arrange(ElementState& element, Context& context, Rectangle content);

        // The children to draw, in order, and those to give pointer events.
        virtual void VisibleChildren(ElementState& element, std::vector<ElementState*>& into);
        virtual void InteractiveChildren(ElementState& element, std::vector<ElementState*>& into)
        {
            VisibleChildren(element, into);
        }

        virtual bool ClipsChildren(ElementState&) const { return false; }

        // ---- Drawing -----------------------------------------------------------

        // The box behind the content, after the theme's defaults for the kind and
        // its hover and press state.
        virtual Box LookOf(ElementState& element, const Context& context);

        // Draws what the element shows itself, over its box and under its children.
        virtual void Draw(ElementState&, DrawContext&) {}

        // Draws the children in a way of its own, such as a transition between
        // two of them. Returns false to have them drawn in order as usual.
        virtual bool DrawChildren(ElementState&, DrawContext&) { return false; }

        // Draws over its children.
        virtual void DrawOver(ElementState&, DrawContext&) {}

        // Once a frame, before layout, for kinds that change by themselves.
        virtual void Update(ElementState&, Context&) {}

        // ---- Events ------------------------------------------------------------

        // Pointer presses stop at this element instead of passing through.
        virtual bool TakesPointer(ElementState&) const { return false; }

        // Whether it takes the pointer at a point inside it, in the root's points.
        // Most kinds take it everywhere or nowhere.
        virtual bool TakesPointerAt(ElementState& element, Vector2) const { return TakesPointer(element); }

        // Whether a point belongs to the element even where a child covers it,
        // such as a scroll bar drawn over the children.
        virtual bool ClaimsPoint(ElementState&, Vector2) const { return false; }

        // The element can have the keyboard.
        virtual bool TakesKeyboard(ElementState&) const { return false; }

        // A ring shows around the element when the keyboard moves to it. Kinds
        // that show focus themselves, such as text fields, go without.
        virtual bool ShowsFocusRing() const { return true; }

        virtual void PointerPressed(ElementState&, Context&, const Pointer&) {}

        // While pressed, or while the pointer is over the element.
        virtual void PointerMoved(ElementState&, Context&, const Pointer&) {}

        // `inside` is whether the pointer is still over the element. The
        // element was clicked when it is.
        virtual void PointerReleased(ElementState&, Context&, const Pointer&, bool /*inside*/) {}

        virtual bool Wheel(ElementState&, Context&, Vector2) { return false; }
        virtual bool KeyPressed(ElementState&, Context&, const Event&) { return false; }
        virtual bool TextEntered(ElementState&, Context&, const Event&) { return false; }
        virtual void FocusChanged(ElementState&, Context&, bool) {}

        // The pointer's shape over the element when the element sets none.
        virtual std::optional<easyforge::Cursor> PointerCursor(ElementState&) const { return std::nullopt; }

        // For window buttons: which of the window's own buttons the point is on.
        virtual std::optional<HitArea> TitleBarArea(ElementState&, Vector2) const { return std::nullopt; }
    };

    // ---- Layout, shared by every kind -------------------------------------------

    // How big the element wants to be, inside its margin. `fillWidth` and
    // `fillHeight` say whether elements that fill take all of `available` on
    // that axis; when a parent is working out its own size from its children,
    // they fit their content instead.
    Vector2 Measure(ElementState& element, Context& context, Vector2 available, bool fillWidth = true,
        bool fillHeight = true);

    // Puts the element in `box` (inside its margin) and arranges its children.
    void Place(ElementState& element, Context& context, Rectangle box);

    // Rows and columns: measuring and arranging children along one axis.
    Vector2 MeasureLine(ElementState& element, Context& context, const Constraint& constraint, bool horizontal);
    // A scroll arranges its children along a length longer than its view, moved
    // back by `offset`; `sizeRoom`, when given, is the length percent sizes are
    // taken from: the view's.
    void ArrangeLine(ElementState& element, Context& context, Rectangle content, bool horizontal, float offset = 0.0f,
        float sizeRoom = -1.0f);

    // Children placed on top of each other, each by the alignment.
    Vector2 MeasureLayers(ElementState& element, Context& context, const Constraint& constraint);
    void ArrangeLayers(ElementState& element, Context& context, Rectangle content, Alignment alignment);

    // Colors mixed toward another by an amount from 0 to 1.
    Color Mix(Color from, Color to, float amount);

    // Text in the element's font and size.
    easyforge::Font FontOf(ElementState& element, const Context& context);
    float FontSizeOf(ElementState& element, const Context& context);
}
