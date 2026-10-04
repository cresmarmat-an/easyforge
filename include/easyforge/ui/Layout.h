#pragma once

#include <optional>
#include <string>
#include <vector>

#include <easyforge/ui/Element.h>

namespace easyforge::ui
{
    // Settings for the elements that hold other elements: Row, Column, Stack,
    // Panel, Display, and TitleBar. Every field is optional; write only the ones
    // you need, in this order.
    struct ContainerSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        // Space between children, in points.
        float Gap = 0.0f;

        // Where children sit across a row or column, and inside a stack.
        ui::Alignment Alignment = ui::Alignment::Stretch;

        // How a row or column spreads its children along itself.
        ui::Distribution Distribution = ui::Distribution::Start;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        std::vector<Element> Children;
    };

    // The properties every container has, on top of an Element's.
    class Container : public Element
    {
    public:
        Container(const Container& other);
        Container& operator=(const Container& other);

        Property<float> Gap;
        Property<ui::Alignment> Alignment;
        Property<ui::Distribution> Distribution;

    protected:
        explicit Container(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // Children side by side, from left to right.
    //
    //     ui::Row({ .Gap = 8, .Children = { ui::Button("Cancel"), ui::Button("Save") } })
    class Row : public Container
    {
    public:
        explicit Row(const ContainerSettings& settings = {});

        static constexpr std::string_view KindName = "Row";

    protected:
        explicit Row(std::shared_ptr<internal::ElementState> state) : Container(std::move(state)) {}
        friend class Element;
        friend class Root;
    };

    // Children one under another, from top to bottom.
    class Column : public Container
    {
    public:
        explicit Column(const ContainerSettings& settings = {});

        static constexpr std::string_view KindName = "Column";

    protected:
        explicit Column(std::shared_ptr<internal::ElementState> state) : Container(std::move(state)) {}
        friend class Element;
        friend class Root;
    };

    // Children on top of each other, the last on top: a game with an interface
    // over it, or a badge on a picture. Each child is placed by the stack's
    // Alignment, on both axes.
    class Stack : public Container
    {
    public:
        explicit Stack(const ContainerSettings& settings = {});

        static constexpr std::string_view KindName = "Stack";

    protected:
        explicit Stack(std::shared_ptr<internal::ElementState> state) : Container(std::move(state)) {}
        friend class Element;
        friend class Root;
    };

    // A column in a box: the theme's surface color and rounded corners, unless
    // given its own.
    class Panel : public Container
    {
    public:
        explicit Panel(const ContainerSettings& settings = {});

        static constexpr std::string_view KindName = "Panel";

    protected:
        explicit Panel(std::shared_ptr<internal::ElementState> state) : Container(std::move(state)) {}
        friend class Element;
        friend class Root;
    };

    struct GridSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        // How many children go in each row. The columns share the width equally.
        int Columns = 2;

        // Space between columns, and between rows.
        float ColumnGap = 0.0f;
        float RowGap = 0.0f;

        // Where each child sits in its cell.
        ui::Alignment Alignment = ui::Alignment::Stretch;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        std::vector<Element> Children;
    };

    // Children in rows of equal columns, filled from left to right.
    class Grid : public Element
    {
    public:
        explicit Grid(const GridSettings& settings = {});
        Grid(const Grid& other);
        Grid& operator=(const Grid& other);

        Property<int> Columns;
        Property<float> ColumnGap;
        Property<float> RowGap;
        Property<ui::Alignment> Alignment;

        static constexpr std::string_view KindName = "Grid";

    protected:
        explicit Grid(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    enum class ScrollDirection
    {
        Vertical,
        Horizontal,
    };

    struct ScrollSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;
        float Gap = 0.0f;
        ui::Alignment Alignment = ui::Alignment::Stretch;

        ScrollDirection Direction = ScrollDirection::Vertical;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;
        std::string Tooltip;
        std::optional<easyforge::Cursor> Cursor;

        std::vector<Element> Children;
    };

    // Children in a column (or a row) that can be longer than the scroll's own
    // box. The wheel, dragging the bar, dragging with a finger (which keeps
    // going for a moment after it lets go), and the keyboard move it.
    class Scroll : public Element
    {
    public:
        explicit Scroll(const ScrollSettings& settings = {});
        Scroll(const Scroll& other);
        Scroll& operator=(const Scroll& other);

        Property<float> Gap;
        Property<ui::Alignment> Alignment;
        Property<ScrollDirection> Direction;

        // How far the content is moved, in points from its start.
        Property<float> Position;

        // Scrolls smoothly to a position, or until the element is in view.
        void ScrollTo(float position) const;
        void ScrollIntoView(const Element& element) const;

        static constexpr std::string_view KindName = "Scroll";

    protected:
        explicit Scroll(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // Empty space. Without a size it fills: in a row, it pushes what comes
    // after it to the right.
    class Spacer : public Element
    {
    public:
        Spacer();

        // A fixed amount of space, in points, in both directions.
        explicit Spacer(float size);

        static constexpr std::string_view KindName = "Spacer";

    protected:
        explicit Spacer(std::shared_ptr<internal::ElementState> state) : Element(std::move(state)) {}
        friend class Element;
        friend class Root;
    };
}
