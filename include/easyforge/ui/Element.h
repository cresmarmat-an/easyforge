#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Host.h>
#include <easyforge/core/Property.h>
#include <easyforge/core/Rectangle.h>
#include <easyforge/graphics/Shader.h>
#include <easyforge/ui/Effects.h>
#include <easyforge/ui/Values.h>

namespace easyforge::ui
{
    class Element;
    class Root;

    namespace internal
    {
        class ElementState;

        std::string_view KindOf(const ElementState* state);
        std::shared_ptr<ElementState> FindInside(const ElementState* state, std::string_view name, std::string_view kind);
        const std::shared_ptr<ElementState>& StateOfHandle(const Element& element);
    }

    // Anything that can be part of an interface. Every element (Label, Button,
    // Column, and the rest) is an Element with properties of its own, so a list
    // of children holds any of them, and a function can return any of them:
    //
    //     ui::Element Card(std::string title)
    //     {
    //         return ui::Panel({ .Padding = 16, .Children = { ui::Label(title) } });
    //     }
    //
    // Elements are handles: copying one is cheap and every copy refers to the
    // same element, which lives as long as a copy of it does or it is in an
    // interface. Changes work before and after the element is on screen.
    class Element
    {
    public:
        // No element. Tests as false.
        Element();

        Element(const Element& other);
        Element& operator=(const Element& other);
        ~Element();

        explicit operator bool() const;

        // What kind of element it is: "Label", "Button", "Column", and so on.
        std::string_view Kind() const;

        // True when the element is of that kind: `element.Is<ui::Button>()`.
        template <typename Type>
        bool Is() const
        {
            return State && internal::KindOf(State.get()) == Type::KindName;
        }

        // The element as its own kind, or no element when it is another kind.
        template <typename Type>
        Type As() const
        {
            return Is<Type>() ? Type(State) : Type(std::shared_ptr<internal::ElementState>());
        }

        // The first element with the name inside this one, of the kind asked for.
        template <typename Type = Element>
        Type Find(std::string_view name) const
        {
            return Type(internal::FindInside(State.get(), name, KindFilter<Type>()));
        }

        // The element this one is in, or no element.
        Element Parent() const;
        std::vector<Element> Children() const;

        // Adds a child at the end, or at an index. The child leaves the element
        // it was in before. Only elements that hold children accept them, such
        // as Row, Column, Panel, and Stack.
        void Add(const Element& child) const;
        void Insert(std::size_t index, const Element& child) const;

        // Takes the element out of its parent. It keeps its settings, and can be
        // added somewhere else.
        void Remove() const;

        // Takes out every child.
        void Clear() const;

        // Where the element was last placed, in points from the top left of the
        // window or of the root's canvas. Empty until it has been on screen.
        Rectangle Frame() const;

        bool IsHovered() const;
        bool IsPressed() const;
        bool IsFocused() const;

        // Gives the element the keyboard, when it takes the keyboard.
        void Focus() const;

        // The element as something a window can show: `window.Content = ui::Column(...)`.
        operator std::shared_ptr<View>() const;

        // True when both refer to the same element.
        bool operator==(const Element& other) const { return State == other.State; }

        // The name `Find` finds the element by.
        Property<std::string> Name;

        Property<Size> Width;
        Property<Size> Height;
        Property<float> MinimumWidth;
        Property<float> MinimumHeight;
        Property<float> MaximumWidth;
        Property<float> MaximumHeight;

        // Space kept free around the element, and inside it around its content.
        Property<Insets> Margin;
        Property<Insets> Padding;

        // The box behind the content. Elements without these draw no box, except
        // those the theme gives one, such as buttons and panels.
        Property<std::optional<float>> CornerRadius;
        Property<std::optional<ui::Background>> Background;
        Property<float> BorderWidth;
        Property<std::optional<Color>> BorderColor;

        // How the element is drawn, without changing where it is laid out: faded,
        // moved by an offset in points, and scaled around its center.
        Animated<float> Opacity;
        Animated<Vector2> Offset;
        Animated<float> Scale;

        Property<std::vector<Effect>> Effects;

        // Draws the element and everything in it through a shader, which reads
        // what would have been drawn as input.Content.
        Property<easyforge::Shader> Shader;
        Property<std::vector<ShaderValue>> ShaderValues;

        // A hidden element takes no space and gets no events.
        Property<bool> Visible;

        // A disabled element is drawn dimmed and does not respond; neither do the
        // elements in it.
        Property<bool> Enabled;

        // Text shown next to the pointer while it rests on the element.
        Property<std::string> Tooltip;

        // The pointer's shape over the element. Without one, the element's own
        // choice: a text cursor over text fields, the arrow elsewhere.
        Property<std::optional<easyforge::Cursor>> Cursor;

        // Dragging. An element with drag text can be picked up by pressing on it
        // and moving the pointer a few points; a faded copy of it follows the
        // pointer, and an element with OnDrop under the pointer is outlined.
        // Letting go there calls its OnDrop with the text. Escape cancels.
        Property<std::string> DragText;
        Property<std::function<void(const std::string&)>> OnDrop;

        // Called with the paths of files dragged from the system and dropped on
        // the element, when its window reports them.
        Property<std::function<void(const std::vector<std::string>&)>> OnFilesDropped;

        static constexpr std::string_view KindName = "";

    protected:
        explicit Element(std::shared_ptr<internal::ElementState> state);

        template <typename Type>
        static constexpr std::string_view KindFilter()
        {
            return Type::KindName;
        }

        std::shared_ptr<internal::ElementState> State;

        friend class Root;
        friend class internal::ElementState;
        friend const std::shared_ptr<internal::ElementState>& internal::StateOfHandle(const Element& element);
    };
}
