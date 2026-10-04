#pragma once

// Lets scripts read and change a data table: find nodes, add and remove them,
// and read and set their properties. Header-only, so it compiles only in
// programs that use both libraries.
//
//     #include <easyforge/bridges/data_script.h>
//
//     scripts.Define("game", ScriptObjectFor(game));
//
// In a script:
//
//     constant player = game.Find("Player")
//     player.Health -= 10
//     constant sword = player.Add("Sword")
//     sword.Damage = 12
//     for child in player.Children() then
//         print(child.Name)
//     end
//
// A node's properties are read and set with a dot. The names Name, Type, Path,
// Parent, Children, Child, Add, Remove, Rename, Has, Get, and Set belong to the
// node itself; a property with one of those names is read with
// node.Get("Name") and set with node.Set("Name", value). Vectors become
// tables with X, Y, Z, and W, and colors tables with Red, Green, Blue, and
// Alpha.

#include <cmath>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/data.h>
#include <easyforge/script.h>

namespace easyforge
{
    namespace internal::bridges
    {
        inline ScriptValue DataToScript(const DataValue& value)
        {
            switch (value.Type())
            {
            case DataType::Nothing: return {};
            case DataType::Boolean: return value.AsBoolean();
            case DataType::Integer:
            case DataType::Number: return value.AsNumber();
            case DataType::Text: return value.AsText();
            case DataType::Vector2:
            {
                Vector2 vector = value.AsVector2();
                return ScriptValue::Table({ { "X", vector.X }, { "Y", vector.Y } });
            }
            case DataType::Vector3:
            {
                Vector3 vector = value.AsVector3();
                return ScriptValue::Table({ { "X", vector.X }, { "Y", vector.Y }, { "Z", vector.Z } });
            }
            case DataType::Vector4:
            {
                Vector4 vector = value.AsVector4();
                return ScriptValue::Table({ { "X", vector.X }, { "Y", vector.Y }, { "Z", vector.Z }, { "W", vector.W } });
            }
            case DataType::Color:
            {
                Color color = value.AsColor();
                return ScriptValue::Table(
                    { { "Red", color.Red }, { "Green", color.Green }, { "Blue", color.Blue }, { "Alpha", color.Alpha } });
            }
            }
            return {};
        }

        // A script's value as a table value. A whole number stays whole when the
        // property is new or was whole already.
        inline DataValue ScriptToData(const ScriptValue& value, const DataValue& current)
        {
            switch (value.Kind())
            {
            case ScriptValueKind::Nothing: return {};
            case ScriptValueKind::Boolean: return value.AsBoolean();
            case ScriptValueKind::Number:
            {
                double number = value.AsNumber();
                bool keepsWhole = current.Type() == DataType::Integer || current.Type() == DataType::Nothing;
                if (keepsWhole && std::abs(number) < 9e15 && number == static_cast<double>(static_cast<long long>(number)))
                {
                    return static_cast<long long>(number);
                }
                return number;
            }
            case ScriptValueKind::Text: return value.AsText();
            case ScriptValueKind::Table:
            {
                auto number = [&](std::string_view name) { return value.Field(name).As<float>(); };
                if (!value.Field("Red").IsNothing())
                {
                    ScriptValue alpha = value.Field("Alpha");
                    return Color { number("Red"), number("Green"), number("Blue"), alpha.IsNothing() ? 1.0f : alpha.As<float>() };
                }
                if (!value.Field("W").IsNothing())
                {
                    return Vector4 { number("X"), number("Y"), number("Z"), number("W") };
                }
                if (!value.Field("Z").IsNothing())
                {
                    return Vector3 { number("X"), number("Y"), number("Z") };
                }
                if (!value.Field("X").IsNothing())
                {
                    return Vector2 { number("X"), number("Y") };
                }
                return {};
            }
            default: return {};
            }
        }

        inline ScriptValue NodeValue(const Node& node);

        inline ScriptValue NodeList(const std::vector<Node>& nodes)
        {
            std::vector<ScriptValue> items;
            items.reserve(nodes.size());
            for (const Node& node : nodes)
            {
                items.push_back(NodeValue(node));
            }
            return ScriptValue::List(std::move(items));
        }

        // `Add(name)` or `Add(name, type)`, for a table or a node.
        template <typename Owner>
        ScriptValue AdderFor(Owner owner)
        {
            return ScriptValue::Function("Add", [owner](const std::vector<ScriptValue>& arguments) -> Result<ScriptValue> {
                if (arguments.empty() || arguments.size() > 2 || arguments[0].Kind() != ScriptValueKind::Text ||
                    (arguments.size() == 2 && arguments[1].Kind() != ScriptValueKind::Text))
                {
                    return Failure("Add takes a name, and optionally a type, as strings");
                }
                std::string type = arguments.size() == 2 ? arguments[1].AsText() : std::string();
                return NodeValue(owner.Add(arguments[0].AsText(), type));
            });
        }

        class NodeObject final : public ScriptObject
        {
        public:
            explicit NodeObject(Node node) : Target(std::move(node)) {}

            std::string TypeName() const override { return "Node"; }

            static bool OwnMember(std::string_view name)
            {
                return name == "Name" || name == "Type" || name == "Path" || name == "Parent" || name == "Children" ||
                       name == "Child" || name == "Add" || name == "Remove" || name == "Rename" || name == "Has" ||
                       name == "Get" || name == "Set";
            }

            ScriptValue Get(std::string_view name) override
            {
                if (!Target)
                {
                    return {};
                }
                Node node = Target;
                if (name == "Name")
                {
                    return node.Name();
                }
                if (name == "Type")
                {
                    return node.TypeName();
                }
                if (name == "Path")
                {
                    return node.Path();
                }
                if (name == "Parent")
                {
                    return NodeValue(node.Parent());
                }
                if (name == "Children")
                {
                    return ScriptValue::Function("Children", [node]() { return NodeList(node.Children()); });
                }
                if (name == "Child")
                {
                    return ScriptValue::Function("Child", [node](std::string child) { return NodeValue(node.Child(child)); });
                }
                if (name == "Add")
                {
                    return AdderFor(node);
                }
                if (name == "Remove")
                {
                    return ScriptValue::Function("Remove", [node]() { node.Remove(); });
                }
                if (name == "Rename")
                {
                    return ScriptValue::Function("Rename", [node](std::string newName) { node.Rename(newName); });
                }
                if (name == "Has")
                {
                    return ScriptValue::Function("Has", [node](std::string property) { return node.Has(property); });
                }
                if (name == "Get")
                {
                    return ScriptValue::Function("Get", [node](std::string property) { return DataToScript(node.Get(property)); });
                }
                if (name == "Set")
                {
                    return ScriptValue::Function("Set", [node](std::string property, ScriptValue value) {
                        node.Set(property, ScriptToData(value, node.Get(property)));
                    });
                }
                return DataToScript(node.Get(name));
            }

            bool Set(std::string_view name, const ScriptValue& value) override
            {
                if (!Target || OwnMember(name))
                {
                    return false;
                }
                Target.Set(name, ScriptToData(value, Target.Get(name)));
                return true;
            }

            std::vector<std::string> MemberNames() const override
            {
                return Target ? Target.Properties() : std::vector<std::string>();
            }

            Node Target;
        };

        class DataTableObject final : public ScriptObject
        {
        public:
            explicit DataTableObject(Table table) : Target(std::move(table)) {}

            std::string TypeName() const override { return "Table"; }

            ScriptValue Get(std::string_view name) override
            {
                Table table = Target;
                if (!table)
                {
                    return {};
                }
                if (name == "Find")
                {
                    return ScriptValue::Function("Find", [table](std::string path) { return NodeValue(table.Find(path)); });
                }
                if (name == "Add")
                {
                    return AdderFor(table);
                }
                if (name == "TopNodes")
                {
                    return ScriptValue::Function("TopNodes", [table]() { return NodeList(table.TopNodes()); });
                }
                if (name == "Nodes")
                {
                    return ScriptValue::Function("Nodes", [table]() { return NodeList(table.Nodes()); });
                }
                if (name == "NodesOfType")
                {
                    return ScriptValue::Function("NodesOfType", [table](std::string type) { return NodeList(table.NodesOfType(type)); });
                }
                if (name == "NodesWith")
                {
                    return ScriptValue::Function("NodesWith", [table](std::string property) { return NodeList(table.NodesWith(property)); });
                }
                if (name == "Version")
                {
                    return static_cast<double>(table.Version());
                }
                if (name == "BeginEdit")
                {
                    return ScriptValue::Function("BeginEdit", [table](std::string edit) { table.BeginEdit(edit); });
                }
                if (name == "EndEdit")
                {
                    return ScriptValue::Function("EndEdit", [table]() { table.EndEdit(); });
                }
                if (name == "Undo")
                {
                    return ScriptValue::Function("Undo", [table]() { return table.Undo(); });
                }
                if (name == "Redo")
                {
                    return ScriptValue::Function("Redo", [table]() { return table.Redo(); });
                }
                return {};
            }

            Table Target;
        };

        inline ScriptValue NodeValue(const Node& node)
        {
            return node ? ScriptValue(std::make_shared<NodeObject>(node)) : ScriptValue();
        }
    }

    // An object scripts use to read and change a table: Find, Add, TopNodes,
    // Nodes, NodesOfType, NodesWith, Version, BeginEdit, EndEdit, Undo, Redo.
    inline std::shared_ptr<ScriptObject> ScriptObjectFor(const Table& table)
    {
        return std::make_shared<internal::bridges::DataTableObject>(table);
    }

    // One node, as Find and Add give them to scripts.
    inline std::shared_ptr<ScriptObject> ScriptObjectFor(const Node& node)
    {
        return std::make_shared<internal::bridges::NodeObject>(node);
    }
}
