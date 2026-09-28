#include "TreeText.h"

#include <format>
#include <optional>
#include <vector>

#include "TableState.h"

namespace easyforge::internal
{
    namespace
    {
        bool NeedsQuotes(std::string_view name)
        {
            return name.empty() || name.find_first_of("=:\"\n\t\r") != std::string_view::npos || name.front() == ' ' ||
                   name.back() == ' ' || name.find("--") != std::string_view::npos || name == "type" ||
                   name.starts_with("type ");
        }

        std::string Quote(std::string_view name)
        {
            if (!NeedsQuotes(name))
            {
                return std::string(name);
            }
            return DataValue(name).ToText();
        }

        std::string Indent(int depth)
        {
            return std::string(static_cast<std::size_t>(depth) * 4, ' ');
        }

        void WriteNodes(const TableState& table, std::uint32_t first, int depth, std::string& text)
        {
            for (std::uint32_t slot = first; slot != NoSlot; slot = table.Rows[slot].Next)
            {
                const Row& row = table.Rows[slot];
                text += Indent(depth) + Quote(row.Name);
                if (!row.Type.empty())
                {
                    text += " : " + Quote(row.Type);
                }
                text += '\n';
                for (const std::string& property : row.PropertyOrder)
                {
                    text += Indent(depth + 1) + Quote(property) + " = " +
                            table.OwnValue(slot, property).value_or(DataValue()).ToText() + '\n';
                }
                WriteNodes(table, row.FirstChild, depth + 1, text);
            }
        }

        std::string_view Trim(std::string_view text)
        {
            std::size_t first = text.find_first_not_of(" \t\r");
            if (first == std::string_view::npos)
            {
                return {};
            }
            return text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
        }

        // Where `symbol` first appears outside quotes, or npos.
        std::size_t FindOutsideQuotes(std::string_view text, std::string_view symbol)
        {
            bool quoted = false;
            for (std::size_t index = 0; index < text.size(); ++index)
            {
                if (text[index] == '\\' && quoted)
                {
                    ++index;
                    continue;
                }
                if (text[index] == '"')
                {
                    quoted = !quoted;
                    continue;
                }
                if (!quoted && text.substr(index, symbol.size()) == symbol)
                {
                    return index;
                }
            }
            return std::string_view::npos;
        }

        std::optional<std::string> Unquote(std::string_view text)
        {
            text = Trim(text);
            if (!text.empty() && text.front() == '"')
            {
                bool valid = false;
                DataValue value = DataValue::FromText(text, &valid);
                if (!valid || value.Type() != DataType::Text)
                {
                    return std::nullopt;
                }
                return value.AsText();
            }
            return std::string(text);
        }
    }

    std::string WriteTree(const TableState& table)
    {
        std::string text;
        for (const auto& [name, values] : table.Types)
        {
            text += "type " + Quote(name) + '\n';
            for (const auto& [property, value] : values)
            {
                text += Indent(1) + Quote(property) + " = " + value.ToText() + '\n';
            }
            text += '\n';
        }
        WriteNodes(table, table.FirstTop, 0, text);
        return text;
    }

    Result<> ReadTree(TableState& table, std::string_view text, std::string_view name)
    {
        std::vector<std::uint32_t> path;
        std::optional<std::string> currentType;
        int lineNumber = 0;
        std::size_t position = 0;
        auto fail = [&](std::string message) { return Failure(std::format("{}:{}: {}", name, lineNumber, message)); };

        while (position <= text.size())
        {
            std::size_t end = text.find('\n', position);
            std::string_view line = text.substr(position, end == std::string_view::npos ? std::string_view::npos : end - position);
            position = end == std::string_view::npos ? text.size() + 1 : end + 1;
            ++lineNumber;

            if (std::size_t comment = FindOutsideQuotes(line, "--"); comment != std::string_view::npos)
            {
                line = line.substr(0, comment);
            }
            if (Trim(line).empty())
            {
                continue;
            }

            int indent = 0;
            std::size_t start = 0;
            for (; start < line.size() && (line[start] == ' ' || line[start] == '\t'); ++start)
            {
                indent += line[start] == '\t' ? 4 : 1;
            }
            if (indent % 4 != 0)
            {
                return fail("each level is four spaces deeper, but this line is indented by " + std::to_string(indent));
            }
            std::size_t depth = static_cast<std::size_t>(indent / 4);
            std::string_view content = Trim(line.substr(start));

            std::size_t equals = FindOutsideQuotes(content, "=");
            if (equals != std::string_view::npos)
            {
                std::optional<std::string> property = Unquote(content.substr(0, equals));
                if (!property || property->empty())
                {
                    return fail("a property needs a name before the =");
                }
                bool valid = false;
                std::string_view written = Trim(content.substr(equals + 1));
                DataValue value = DataValue::FromText(written, &valid);
                if (!valid)
                {
                    return fail(std::format("'{}' is not a value: write a number, text in quotes, true, false, a color "
                                            "such as #FF8000, or numbers separated by commas",
                        written));
                }
                if (currentType && depth == 1)
                {
                    table.Types[*currentType].emplace_back(*property, value);
                    continue;
                }
                if (depth == 0 || depth > path.size())
                {
                    return fail(std::format("the property {} is not under a node", *property));
                }
                table.SetProperty(path[depth - 1], *property, value.IsNothing() ? std::nullopt : std::optional(value));
                continue;
            }

            if (depth == 0 && (content == "type" || content.starts_with("type ")))
            {
                std::optional<std::string> type = Unquote(content.substr(4));
                if (!type || type->empty())
                {
                    return fail("a type needs a name: type Enemy");
                }
                currentType = *type;
                table.Types[*type];
                path.clear();
                continue;
            }

            currentType.reset();
            if (depth > path.size())
            {
                return fail("this node is indented deeper than the node above it allows");
            }
            std::size_t colon = FindOutsideQuotes(content, ":");
            std::optional<std::string> nodeName =
                Unquote(colon == std::string_view::npos ? content : content.substr(0, colon));
            std::optional<std::string> type = colon == std::string_view::npos ? std::optional<std::string>(std::string())
                                                                              : Unquote(content.substr(colon + 1));
            if (!nodeName || !type)
            {
                return fail(std::format("'{}' is not a node: write its name, and a colon and its type if it has one",
                    content));
            }
            std::uint32_t parent = depth == 0 ? NoSlot : path[depth - 1];
            path.resize(depth);
            path.push_back(table.AddNode(parent, *nodeName, *type));
        }
        return {};
    }
}
