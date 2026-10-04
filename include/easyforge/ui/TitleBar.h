#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/ui/Layout.h>

namespace easyforge::ui
{
    struct TitleBarSettings
    {
        std::string Name;

        // As wide as the window and 36 points tall, unless given others.
        Size Width = ui::Fill;
        Size Height = 36.0f;
        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;
        float MaximumWidth = Unlimited;
        float MaximumHeight = Unlimited;

        Insets Margin;

        // 12 points before the first child.
        Insets Padding { 12.0f, 0.0f, 0.0f, 0.0f };

        float Gap = 8.0f;
        ui::Alignment Alignment = ui::Alignment::Center;
        ui::Distribution Distribution = ui::Distribution::Start;

        // Without a background, the theme's TitleBar color.
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

    // A title bar to draw in place of the system's:
    //
    //     window.TitleBar = ui::TitleBar({
    //         .Height = 40,
    //         .Background = Color::Hex("#15151A"),
    //         .Children = {
    //             ui::Image("icon.png", { .Width = 20, .Height = 20 }),
    //             ui::Label("Notes"),
    //             ui::Spacer(),
    //             ui::WindowButtons(),
    //         },
    //     });
    //
    // A row, 36 points tall and as wide as the window unless told otherwise,
    // with its children centered from top to bottom and 8 points apart. Dragging
    // anywhere that is not a control moves the window, and double-clicking there
    // maximizes it.
    class TitleBar : public Container
    {
    public:
        explicit TitleBar(const TitleBarSettings& settings = {});

        static constexpr std::string_view KindName = "TitleBar";

    protected:
        explicit TitleBar(std::shared_ptr<internal::ElementState> state) : Container(std::move(state)) {}
        friend class Element;
        friend class Root;
    };

    struct WindowButtonsSettings
    {
        std::string Name;

        // Each button's width. Their height is the title bar's.
        float ButtonWidth = 46.0f;

        bool Minimize = true;
        bool Maximize = true;
        bool Close = true;

        bool Visible = true;
    };

    // The minimize, maximize, and close buttons, drawn in the theme's colors.
    // The window acts on them as on its own buttons; on Windows 11, resting on
    // maximize shows the snap layouts. Put them last in a TitleBar.
    class WindowButtons : public Element
    {
    public:
        explicit WindowButtons(const WindowButtonsSettings& settings = {});

        static constexpr std::string_view KindName = "WindowButtons";

    protected:
        explicit WindowButtons(std::shared_ptr<internal::ElementState> state) : Element(std::move(state)) {}
        friend class Element;
        friend class Root;
    };
}
