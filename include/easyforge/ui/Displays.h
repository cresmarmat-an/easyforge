#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/graphics/Shader.h>
#include <easyforge/ui/Layout.h>

namespace easyforge::ui
{
    // Which side the new display comes in from when sliding. Going back slides
    // the other way.
    enum class SlideFrom
    {
        Right,
        Left,
        Bottom,
        Top,
    };

    // How Displays changes from one display to the next.
    //
    //     pages.Show("Settings", ui::Transition::Slide(0.25f));
    class Transition
    {
    public:
        enum class Kind
        {
            Cut,
            Fade,
            Slide,
            Scale,
            Shader,
        };

        // At once. The default.
        static Transition Cut();

        // The new display fades in over the old one.
        static Transition Fade(float seconds = 0.25f);

        // The new display pushes the old one out.
        static Transition Slide(float seconds = 0.25f, SlideFrom from = SlideFrom::Right);

        // The old display shrinks a little and fades while the new one grows into
        // place.
        static Transition Scale(float seconds = 0.25f);

        // The new display is drawn through a shader over the old one. The shader
        // gets `value Progress: number`, going from 0 to 1, and reads the new
        // display as input.Content: at 0 nothing of it should show, at 1 all of it.
        static Transition Shader(const easyforge::Shader& shader, float seconds = 0.4f);

        // The transition that undoes this one: a slide from the other side.
        Transition Reversed() const;

        Kind Type = Kind::Cut;
        float Seconds = 0.0f;
        ui::Easing Easing = ui::Easing::InOut;
        SlideFrom From = SlideFrom::Right;
        easyforge::Shader Effect;
    };

    // One screen of a Displays: a column with a name.
    //
    //     ui::Display("Settings", { .Padding = 24, .Children = { ... } })
    class Display : public Container
    {
    public:
        explicit Display(std::string name = {}, const ContainerSettings& settings = {});

        Display(const Display& other);
        Display& operator=(const Display& other);

        // Called when the display starts to be shown, and when it has gone.
        Property<std::function<void()>> OnShown;
        Property<std::function<void()>> OnHidden;

        static constexpr std::string_view KindName = "Display";

    protected:
        explicit Display(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct DisplaysSettings
    {
        std::string Name;

        // Without a size, it fills.
        Size Width = ui::Fill;
        Size Height = ui::Fill;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;
        Insets Padding;

        // The display shown first. Without one, the first child.
        std::string Start;

        // Used by Show and Back when they are not given one.
        ui::Transition Transition = ui::Transition::Cut();

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
        std::optional<easyforge::Cursor> Cursor;

        // Called with the name of the display being shown.
        std::function<void(const std::string&)> OnChange;

        // Its displays.
        std::vector<Element> Children;
    };

    // Several displays in one place, one shown at a time: the menu, the settings,
    // and the game, or the pages on the right of a settings screen.
    //
    //     ui::Displays pages({
    //         .Start = "Menu",
    //         .Children = {
    //             ui::Display("Menu", { .Children = { ... } }),
    //             ui::Display("Settings", { .Children = { ... } }),
    //         },
    //     });
    //     pages.Show("Settings", ui::Transition::Slide(0.25f));
    //     pages.Back();
    //
    // Both displays are drawn during a transition. A hidden display keeps its
    // state, such as where it is scrolled to and what was typed, until it is
    // shown again. Only the display coming in gets events.
    class Displays : public Element
    {
    public:
        explicit Displays(const DisplaysSettings& settings = {});

        Displays(const Displays& other);
        Displays& operator=(const Displays& other);

        // Shows the display with the name, remembering the one shown before for
        // Back. Returns false when there is no such display.
        bool Show(std::string_view name) const;
        bool Show(std::string_view name, const ui::Transition& transition) const;

        // Goes back to the display shown before, with the transition that led here
        // reversed. Returns false when there is nothing to go back to.
        bool Back() const;
        bool CanGoBack() const;

        // The name of the display shown, or being shown. Assigning shows it with
        // the default transition.
        Property<std::string> Current;

        Property<ui::Transition> Transition;
        Property<std::function<void(const std::string&)>> OnChange;

        static constexpr std::string_view KindName = "Displays";

    protected:
        explicit Displays(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };

    struct TabsSettings
    {
        std::string Name;

        // Without a size, it fills.
        Size Width = ui::Fill;
        Size Height = ui::Fill;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;

        // Around the page, below the tabs.
        Insets Padding;

        // The tab shown first. Without one, the first child.
        std::string Start;

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

        // Called with the name of the tab chosen.
        std::function<void(const std::string&)> OnChange;

        // Its pages, usually Display elements. Each page's name is its tab's text.
        std::vector<Element> Children;
    };

    // A row of tabs over pages, one page shown at a time.
    //
    //     ui::Tabs({
    //         .Children = {
    //             ui::Display("General", { .Padding = 16, .Children = { ... } }),
    //             ui::Display("Advanced", { .Padding = 16, .Children = { ... } }),
    //         },
    //     })
    //
    // Clicking a tab shows its page. With the keyboard on the tabs, Left and Right
    // move to the tab before or after, and Home and End to the first and last.
    // Pages that are not shown keep their state. Tabs is as large as its largest
    // page, so it does not change size between tabs.
    class Tabs : public Element
    {
    public:
        explicit Tabs(const TabsSettings& settings = {});

        Tabs(const Tabs& other);
        Tabs& operator=(const Tabs& other);

        // The name of the tab shown. Assigning shows it.
        Property<std::string> Current;

        Property<float> FontSize;
        Property<std::optional<easyforge::Color>> Color;
        Property<std::function<void(const std::string&)>> OnChange;

        static constexpr std::string_view KindName = "Tabs";

    protected:
        explicit Tabs(std::shared_ptr<internal::ElementState> state);
        friend class Element;
        friend class Root;
    };
}
