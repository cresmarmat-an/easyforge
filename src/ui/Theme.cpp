#include <easyforge/ui/Theme.h>

#include <array>
#include <format>
#include <string_view>

#include <easyforge/data/Table.h>

namespace easyforge::ui
{
    namespace
    {
        struct ColorField
        {
            std::string_view Name;
            Color Theme::*Member;
        };

        constexpr std::array<ColorField, 21> ColorFields = { {
            { "Background", &Theme::Background },
            { "Surface", &Theme::Surface },
            { "Control", &Theme::Control },
            { "ControlHovered", &Theme::ControlHovered },
            { "ControlPressed", &Theme::ControlPressed },
            { "Text", &Theme::Text },
            { "MutedText", &Theme::MutedText },
            { "Accent", &Theme::Accent },
            { "AccentHovered", &Theme::AccentHovered },
            { "AccentPressed", &Theme::AccentPressed },
            { "AccentText", &Theme::AccentText },
            { "Border", &Theme::Border },
            { "Focus", &Theme::Focus },
            { "TitleBar", &Theme::TitleBar },
            { "TitleBarText", &Theme::TitleBarText },
            { "TitleButtonHovered", &Theme::TitleButtonHovered },
            { "CloseButtonHovered", &Theme::CloseButtonHovered },
            { "DisabledText", &Theme::DisabledText },
            { "Tooltip", &Theme::Tooltip },
            { "TooltipText", &Theme::TooltipText },
            { "Selection", &Theme::Selection },
        } };

        struct NumberField
        {
            std::string_view Name;
            float Theme::*Member;
        };

        constexpr std::array<NumberField, 4> NumberFields = { {
            { "FontSize", &Theme::FontSize },
            { "CornerRadius", &Theme::CornerRadius },
            { "BorderWidth", &Theme::BorderWidth },
            { "Transition", &Theme::Transition },
        } };
    }

    Theme Theme::Light()
    {
        Theme theme;
        theme.Background = Color::Hex("#F3F3F3");
        theme.Surface = Color::Hex("#FFFFFF");
        theme.Control = Color::Hex("#FDFDFD");
        theme.ControlHovered = Color::Hex("#F4F4F4");
        theme.ControlPressed = Color::Hex("#E9E9E9");
        theme.Text = Color::Hex("#1B1B1B");
        theme.MutedText = Color::Hex("#6B6B6B");
        theme.Accent = Color::Hex("#005FB8");
        theme.AccentHovered = Color::Hex("#1A6FC1");
        theme.AccentPressed = Color::Hex("#004E98");
        theme.AccentText = Color::Hex("#FFFFFF");
        theme.Border = Color::Hex("#D1D1D1");
        theme.Focus = Color::Hex("#1B1B1B");
        theme.TitleBar = Color::Hex("#EBEBEB");
        theme.TitleBarText = Color::Hex("#1B1B1B");
        theme.TitleButtonHovered = Color::Hex("#0000001A");
        theme.CloseButtonHovered = Color::Hex("#C42B1C");
        theme.DisabledText = Color::Hex("#A0A0A0");
        theme.Tooltip = Color::Hex("#2B2B2B");
        theme.TooltipText = Color::Hex("#FFFFFF");
        theme.Selection = Color::Hex("#005FB84D");
        return theme;
    }

    Theme Theme::Dark()
    {
        Theme theme;
        theme.Background = Color::Hex("#202020");
        theme.Surface = Color::Hex("#2B2B2B");
        theme.Control = Color::Hex("#373737");
        theme.ControlHovered = Color::Hex("#3E3E3E");
        theme.ControlPressed = Color::Hex("#313131");
        theme.Text = Color::Hex("#FFFFFF");
        theme.MutedText = Color::Hex("#A0A0A0");
        theme.Accent = Color::Hex("#60CDFF");
        theme.AccentHovered = Color::Hex("#78D5FF");
        theme.AccentPressed = Color::Hex("#4AB4E3");
        theme.AccentText = Color::Hex("#000000");
        theme.Border = Color::Hex("#454545");
        theme.Focus = Color::Hex("#FFFFFF");
        theme.TitleBar = Color::Hex("#1C1C1C");
        theme.TitleBarText = Color::Hex("#FFFFFF");
        theme.TitleButtonHovered = Color::Hex("#FFFFFF1A");
        theme.CloseButtonHovered = Color::Hex("#C42B1C");
        theme.DisabledText = Color::Hex("#6E6E6E");
        theme.Tooltip = Color::Hex("#3A3A3A");
        theme.TooltipText = Color::Hex("#FFFFFF");
        theme.Selection = Color::Hex("#60CDFF55");
        return theme;
    }

    Theme Theme::For(ColorScheme scheme)
    {
        return scheme == ColorScheme::Dark ? Dark() : Light();
    }

    Result<Theme> Theme::Load(std::string_view path)
    {
        Table table = Table::Load(path);
        if (!table)
        {
            return Failure(table.Error());
        }
        Node node = table.Find("Theme");
        if (!node)
        {
            return Failure(std::format("{}: there is no Theme node to read the theme from", path));
        }
        Theme theme = node.Get("Base").AsText() == "Dark" ? Dark() : Light();
        for (const ColorField& field : ColorFields)
        {
            DataValue value = node.Get(field.Name);
            if (value.IsNothing())
            {
                continue;
            }
            if (value.Type() != DataType::Color)
            {
                return Failure(std::format("{}: {} should be a color, such as #E4572E", path, field.Name));
            }
            theme.*field.Member = value.AsColor();
        }
        for (const NumberField& field : NumberFields)
        {
            DataValue value = node.Get(field.Name);
            if (value.IsNothing())
            {
                continue;
            }
            if (value.Type() != DataType::Number && value.Type() != DataType::Integer)
            {
                return Failure(std::format("{}: {} should be a number", path, field.Name));
            }
            theme.*field.Member = value.As<float>();
        }
        DataValue font = node.Get("Font");
        if (!font.IsNothing())
        {
            theme.Font = easyforge::Font::Load(font.AsText());
            if (!theme.Font)
            {
                return Failure(theme.Font.Error());
            }
        }
        return theme;
    }

    Result<> Theme::Save(std::string_view path) const
    {
        Table table = Table::New();
        Node node = table.Add("Theme");
        for (const ColorField& field : ColorFields)
        {
            node.Set(field.Name, this->*field.Member);
        }
        for (const NumberField& field : NumberFields)
        {
            node.Set(field.Name, this->*field.Member);
        }
        return table.Save(path);
    }

    Theme Lerp(const Theme& from, const Theme& to, float amount)
    {
        Theme result = amount < 0.5f ? from : to;
        for (const ColorField& field : ColorFields)
        {
            result.*field.Member = easyforge::Lerp(from.*field.Member, to.*field.Member, amount);
        }
        for (const NumberField& field : NumberFields)
        {
            result.*field.Member = easyforge::Lerp(from.*field.Member, to.*field.Member, amount);
        }
        return result;
    }
}
