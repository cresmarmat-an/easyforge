#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <easyforge/data/Table.h>

namespace easyforge::ui
{
    // A property in a data table that an element follows by itself: when the
    // value changes, anywhere, the element shows the new one in the next frame.
    //
    //     ui::Label(ui::Bind(player["Health"], "Health: {}"))
    class Binding
    {
    public:
        // Follows nothing.
        Binding() = default;

        // `format` is a std::format string with one {} for the value.
        explicit Binding(const Cell& cell, std::string_view format = "{}");

        explicit operator bool() const { return Source.has_value(); }

        // The text for the value the cell has now.
        std::string Text() const;

        // The value the cell has now.
        DataValue Value() const;

        const std::string& Format() const { return Pattern; }

    private:
        std::optional<Cell> Source;
        std::string Pattern;
    };

    inline Binding Bind(const Cell& cell, std::string_view format = "{}")
    {
        return Binding(cell, format);
    }
}
