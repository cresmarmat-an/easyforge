#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/data/Table.h>
#include <easyforge/ui/Element.h>

namespace easyforge::ui
{
    struct ListSettings
    {
        std::string Name;

        // Without a size, 240 by 200 points.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        std::optional<Insets> Padding;

        std::vector<std::string> Items;

        // The item chosen, counting from 0, or -1 for none.
        int Selected = -1;

        float FontSize = 0.0f;
        std::optional<easyforge::Color> Color;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        std::optional<float> BorderWidth;
        std::optional<easyforge::Color> BorderColor;

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

        // Called with the item chosen, when a person chooses one or the program
        // assigns Selected.
        std::function<void(int)> OnSelect;

        // Called when an item is double-clicked, or Enter is pressed on it.
        std::function<void(int)> OnActivate;
    };

    // Rows of text, one of which can be chosen, in a box that scrolls when there
    // are more than fit. Clicking chooses a row; the arrow keys, Page Up, Page
    // Down, Home, and End move the choice while the list has the keyboard.
    class List : public Element
    {
    public:
        explicit List(const ListSettings& settings = {});

        List(const List& other);
        List& operator=(const List& other);

        // The chosen item's text, or empty.
        std::string SelectedText() const;

        Property<std::vector<std::string>> Items;
        Property<int> Selected;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<std::function<void(int)>> OnSelect;
        Property<std::function<void(int)>> OnActivate;

        static constexpr std::string_view KindName = "List";

    protected:
        explicit List(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct TreeSettings
    {
        std::string Name;

        // Without a size, 240 by 300 points.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        std::optional<Insets> Padding;

        // The table whose nodes the tree shows.
        Table Source;

        // Show the nodes at the top of the table open.
        bool OpenAtStart = true;

        float FontSize = 0.0f;
        std::optional<easyforge::Color> Color;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        std::optional<float> BorderWidth;
        std::optional<easyforge::Color> BorderColor;

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

        // Called with the node chosen.
        std::function<void(Node)> OnSelect;

        // Called when a node is double-clicked, or Enter is pressed on it.
        std::function<void(Node)> OnActivate;
    };

    // The nodes of a data table as rows that open and close, following the table
    // as it changes: nodes added, removed, renamed, or moved show in the next
    // frame. Clicking the arrow beside a node, or pressing Left and Right, closes
    // and opens it; Up and Down move the choice.
    class Tree : public Element
    {
    public:
        explicit Tree(const TreeSettings& settings = {});

        Tree(const Tree& other);
        Tree& operator=(const Tree& other);

        void OpenNode(const Node& node) const;
        void CloseNode(const Node& node) const;
        bool IsOpen(const Node& node) const;

        Property<Table> Source;
        Property<Node> Selected;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<std::function<void(Node)>> OnSelect;
        Property<std::function<void(Node)>> OnActivate;

        static constexpr std::string_view KindName = "Tree";

    protected:
        explicit Tree(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };
}
