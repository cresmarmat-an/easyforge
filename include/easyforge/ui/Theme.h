#pragma once

#include <string_view>

#include <easyforge/core/Color.h>
#include <easyforge/core/Host.h>
#include <easyforge/core/Result.h>
#include <easyforge/graphics/Font.h>

namespace easyforge::ui
{
    // The colors, font, and shapes an interface uses wherever an element has no
    // value of its own.
    //
    //     ui::Root::Of(window).Theme = ui::Theme::Dark();
    //
    //     ui::Theme theme = ui::Theme::Light();
    //     theme.Accent = Color::Hex("#E4572E");
    //     ui::Root::Of(window).Theme = theme;
    struct Theme
    {
        // Behind everything: the window's own color.
        Color Background;

        // Panels, cards, menus, and dialogs.
        Color Surface;

        // Buttons, fields, and other controls.
        Color Control;
        Color ControlHovered;
        Color ControlPressed;

        Color Text;

        // Text that matters less: hints, placeholders, captions.
        Color MutedText;

        // Highlighted buttons, checked boxes, sliders, and selections.
        Color Accent;
        Color AccentHovered;
        Color AccentPressed;

        // Text on the accent color.
        Color AccentText;

        // Lines around fields and panels, and between sections.
        Color Border;

        // The ring around whatever has the keyboard.
        Color Focus;

        // Behind selected text.
        Color Selection;

        // Title bars and their buttons.
        Color TitleBar;
        Color TitleBarText;
        Color TitleButtonHovered;
        Color CloseButtonHovered;

        // Text in controls that cannot be used right now.
        Color DisabledText;

        // Tooltips.
        Color Tooltip;
        Color TooltipText;

        // Without a font, the system's interface font is used: Segoe UI on
        // Windows.
        easyforge::Font Font;

        // In points.
        float FontSize = 15.0f;

        float CornerRadius = 6.0f;
        float BorderWidth = 1.0f;

        // How long hover and press changes take, in seconds.
        float Transition = 0.12f;

        static Theme Light();
        static Theme Dark();

        // Light or Dark, whichever the system uses.
        static Theme For(ColorScheme scheme);

        // Reads a theme from a .tree file with a node named Theme and a property
        // for each value it changes:
        //
        //     Theme
        //         Base = "Dark"
        //         Accent = #E4572E
        //         CornerRadius = 10
        //         Font = "Inter.ttf"
        //
        // Values the file leaves out come from Light, or from Dark with
        // `Base = "Dark"`. Save writes every color and number; it does not write
        // the font, which a theme holds as a font rather than a file name.
        static Result<Theme> Load(std::string_view path);
        Result<> Save(std::string_view path) const;
    };

    // A theme part way between two others, for switching themes gradually.
    Theme Lerp(const Theme& from, const Theme& to, float amount);
}
