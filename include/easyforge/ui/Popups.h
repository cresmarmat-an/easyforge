#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/ui/Elements.h>
#include <easyforge/ui/Layout.h>

namespace easyforge::ui
{
    class Root;

    struct DropdownSettings
    {
        std::string Name;

        // Without a width, 200 points.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        std::optional<Insets> Padding;

        std::vector<std::string> Options;

        // The option chosen, counting from 0, or -1 for none.
        int Selected = -1;

        // Shown while nothing is chosen.
        std::string Placeholder;

        float FontSize = 0.0f;
        std::optional<easyforge::Color> Color;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
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

        // Called with the option chosen, when a person chooses one or the program
        // assigns Selected.
        std::function<void(int)> OnChange;
    };

    // A button that shows the option chosen, and opens a list of the others
    // below it.
    //
    //     ui::Dropdown size({ .Options = { "Small", "Medium", "Large" }, .Selected = 1 })
    //
    // Clicking or pressing Enter or Space opens the list; the arrow keys move
    // through it, Enter chooses, and Escape or a click elsewhere closes it. While
    // closed, Up and Down choose the option before or after.
    class Dropdown : public Element
    {
    public:
        explicit Dropdown(const DropdownSettings& settings = {});

        Dropdown(const Dropdown& other);
        Dropdown& operator=(const Dropdown& other);

        // The chosen option's text, or empty.
        std::string SelectedText() const;

        Property<std::vector<std::string>> Options;
        Property<int> Selected;
        Property<std::string> Placeholder;
        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<std::function<void(int)>> OnChange;

        static constexpr std::string_view KindName = "Dropdown";

    protected:
        explicit Dropdown(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    // One entry of a menu.
    struct MenuItem
    {
        std::string Text;
        std::function<void()> OnClick;

        // Text shown at the right, such as "Ctrl+S". The menu does not act on it;
        // the program handles its own shortcuts.
        std::string Shortcut;

        bool Enabled = true;

        // A line between groups of entries; Text and OnClick are not used.
        bool Separator = false;
    };

    struct MenuSettings
    {
        std::string Name;

        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        std::optional<Insets> Padding;

        std::vector<MenuItem> Items;

        // The button's look. Menus in title bars and toolbars suit Subtle, the default.
        ButtonStyle Style = ButtonStyle::Subtle;

        float FontSize = 0.0f;
        std::optional<easyforge::Color> Color;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
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
    };

    // A button that opens a list of actions below it, for menu bars such as
    // File and Edit.
    //
    //     ui::Menu("File", {
    //         .Items = {
    //             { .Text = "Open", .OnClick = [] { Open(); }, .Shortcut = "Ctrl+O" },
    //             { .Separator = true },
    //             { .Text = "Quit", .OnClick = [window] { window.Close(); } },
    //         },
    //     })
    class Menu : public Element
    {
    public:
        explicit Menu(std::string text = {}, const MenuSettings& settings = {});

        Menu(const Menu& other);
        Menu& operator=(const Menu& other);

        // Opens or closes the list, as a click would.
        void Open() const;
        void Close() const;
        bool IsOpen() const;

        Property<std::string> Text;
        Property<std::vector<MenuItem>> Items;
        Property<ButtonStyle> Style;

        static constexpr std::string_view KindName = "Menu";

    protected:
        explicit Menu(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct DialogSettings
    {
        std::string Name;

        // Without a width, 440 points.
        Size Width;
        Size Height;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;

        // Without padding, 24 points.
        std::optional<Insets> Padding;

        // Space between the title, the children, and the buttons. Without one, 16.
        std::optional<float> Gap;

        // Shown at the top in a larger size.
        std::string Title;

        std::optional<float> CornerRadius;
        std::optional<ui::Background> Background;
        float BorderWidth = 0.0f;
        std::optional<easyforge::Color> BorderColor;

        float Opacity = 1.0f;
        Vector2 Offset;
        float Scale = 1.0f;
        std::vector<Effect> Effects;
        easyforge::Shader Shader;
        std::vector<ShaderValue> ShaderValues;

        bool Visible = true;
        bool Enabled = true;

        // Whether clicking outside the dialog closes it. Escape always does.
        bool ClosesOnOutsideClick = false;

        // Called when the dialog has closed, however it was closed.
        std::function<void()> OnClosed;

        std::vector<Element> Children;

        // Laid out in a row at the bottom right, such as Cancel and OK.
        std::vector<Element> Buttons;
    };

    // A box over everything else, which takes the keyboard and the pointer until
    // it is closed. The rest of the interface is dimmed behind it.
    //
    //     ui::Dialog confirm({
    //         .Title = "Delete the note?",
    //         .Children = { ui::Label("This cannot be undone.") },
    //         .Buttons = { ui::Button("Cancel", { ... }), ui::Button("Delete", { ... }) },
    //     });
    //     confirm.Open(ui::Root::Of(window));
    //     ...
    //     confirm.Close();
    class Dialog : public Container
    {
    public:
        explicit Dialog(const DialogSettings& settings = {});

        Dialog(const Dialog& other);
        Dialog& operator=(const Dialog& other);

        // Shows the dialog over the root's interface, centered, and gives the
        // keyboard to the first element in it that takes the keyboard.
        void Open(const Root& root) const;
        void Close() const;
        bool IsOpen() const;

        Property<std::string> Title;
        Property<bool> ClosesOnOutsideClick;
        Property<std::function<void()>> OnClosed;

        static constexpr std::string_view KindName = "Dialog";

    protected:
        explicit Dialog(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };
}
