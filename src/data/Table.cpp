#include <easyforge/data/Table.h>

#include <format>
#include <fstream>
#include <iterator>

#include <easyforge/core/Log.h>

#include "TableState.h"
#include "TreeText.h"

namespace easyforge
{
    using internal::NoSlot;
    using internal::TableState;

    // ---- Cell --------------------------------------------------------------

    const Cell& Cell::operator=(const DataValue& value) const
    {
        if (Owner && Owner->Valid(Slot, Generation))
        {
            Owner->SetProperty(Slot, Property, value.IsNothing() ? std::nullopt : std::optional<DataValue>(value));
        }
        return *this;
    }

    namespace
    {
        DataValue Combine(const DataValue& current, const DataValue& amount, float sign)
        {
            switch (current.Type())
            {
            case DataType::Integer:
                if (amount.Type() == DataType::Integer)
                {
                    return current.AsInteger() + static_cast<std::int64_t>(sign) * amount.AsInteger();
                }
                return current.AsNumber() + sign * amount.AsNumber();
            case DataType::Number: return current.AsNumber() + sign * amount.AsNumber();
            case DataType::Vector2: return current.AsVector2() + amount.AsVector2() * sign;
            case DataType::Vector3: return current.AsVector3() + amount.AsVector3() * sign;
            case DataType::Vector4: return current.AsVector4() + amount.AsVector4() * sign;
            case DataType::Nothing:
                // Adding to nothing starts from zero of the amount's type.
                switch (amount.Type())
                {
                case DataType::Integer: return static_cast<std::int64_t>(sign) * amount.AsInteger();
                case DataType::Number: return sign * amount.AsNumber();
                case DataType::Vector2: return amount.AsVector2() * sign;
                case DataType::Vector3: return amount.AsVector3() * sign;
                case DataType::Vector4: return amount.AsVector4() * sign;
                default: return current;
                }
            default: return current;
            }
        }
    }

    const Cell& Cell::operator+=(const DataValue& amount) const
    {
        return *this = Combine(Get(), amount, 1.0f);
    }

    const Cell& Cell::operator-=(const DataValue& amount) const
    {
        return *this = Combine(Get(), amount, -1.0f);
    }

    DataValue Cell::Get() const
    {
        if (!Owner || !Owner->Valid(Slot, Generation))
        {
            return {};
        }
        return Owner->Read(Slot, Property);
    }

    bool Cell::Exists() const
    {
        return Owner && Owner->Valid(Slot, Generation) &&
               (Owner->OwnValue(Slot, Property) || Owner->TypeValue(Slot, Property));
    }

    void Cell::Clear() const
    {
        if (Owner && Owner->Valid(Slot, Generation))
        {
            Owner->SetProperty(Slot, Property, std::nullopt);
        }
    }

    // ---- Node --------------------------------------------------------------

    bool Node::Valid() const
    {
        return Owner && Owner->Valid(Slot, Generation);
    }

    Node::operator bool() const
    {
        return Valid();
    }

    Cell Node::operator[](std::string_view property) const
    {
        return Cell(Owner, Slot, Generation, std::string(property));
    }

    DataValue Node::Get(std::string_view property) const
    {
        return Valid() ? Owner->Read(Slot, property) : DataValue();
    }

    void Node::Set(std::string_view property, const DataValue& value) const
    {
        if (Valid())
        {
            Owner->SetProperty(Slot, std::string(property), value.IsNothing() ? std::nullopt : std::optional(value));
        }
    }

    Node Node::Add(std::string_view name, std::string_view type) const
    {
        if (!Valid())
        {
            return {};
        }
        return Owner->Handle(Owner->AddNode(Slot, name, type));
    }

    Node Node::Add(std::string_view name, const PropertyValues& values) const
    {
        return Add(name, {}, values);
    }

    Node Node::Add(std::string_view name, std::string_view type, const PropertyValues& values) const
    {
        Node child = Add(name, type);
        for (const auto& [property, value] : values)
        {
            child[property] = value;
        }
        return child;
    }

    void Node::Remove() const
    {
        if (Valid())
        {
            Owner->RemoveNode(Slot);
        }
    }

    std::string Node::Name() const
    {
        return Valid() ? Owner->Rows[Slot].Name : std::string();
    }

    void Node::Rename(std::string_view name) const
    {
        if (Valid())
        {
            Owner->RenameNode(Slot, name);
        }
    }

    std::string Node::TypeName() const
    {
        return Valid() ? Owner->Rows[Slot].Type : std::string();
    }

    Node Node::Parent() const
    {
        if (!Valid() || Owner->Rows[Slot].Parent == NoSlot)
        {
            return {};
        }
        return Owner->Handle(Owner->Rows[Slot].Parent);
    }

    std::vector<Node> Node::Children() const
    {
        std::vector<Node> children;
        if (Valid())
        {
            for (std::uint32_t child : Owner->ChildrenOf(Slot))
            {
                children.push_back(Owner->Handle(child));
            }
        }
        return children;
    }

    std::size_t Node::ChildCount() const
    {
        return Valid() ? Owner->ChildrenOf(Slot).size() : 0;
    }

    Node Node::Child(std::string_view name) const
    {
        if (!Valid())
        {
            return {};
        }
        std::uint32_t child = Owner->FindChild(Slot, name);
        return child == NoSlot ? Node() : Owner->Handle(child);
    }

    void Node::MoveTo(const Node& parent) const
    {
        if (!Valid() || (parent.Owner && parent.Owner != Owner))
        {
            return;
        }
        if (parent.Owner && !parent.Valid())
        {
            return;
        }
        Owner->MoveNode(Slot, parent.Owner ? parent.Slot : NoSlot);
    }

    bool Node::Has(std::string_view property) const
    {
        return (*this)[property].Exists();
    }

    std::vector<std::string> Node::Properties() const
    {
        return Valid() ? Owner->Rows[Slot].PropertyOrder : std::vector<std::string>();
    }

    std::uint64_t Node::Identifier() const
    {
        return Valid() ? Owner->Rows[Slot].Identifier : 0;
    }

    std::string Node::Path() const
    {
        if (!Valid())
        {
            return {};
        }
        std::string path;
        for (std::uint32_t slot = Slot; slot != NoSlot; slot = Owner->Rows[slot].Parent)
        {
            path = path.empty() ? Owner->Rows[slot].Name : Owner->Rows[slot].Name + "/" + path;
        }
        return path;
    }

    // ---- Table -------------------------------------------------------------

    Table Table::New(const TableSettings& settings)
    {
        auto state = std::make_shared<TableState>(settings);
        state->Made = true;
        return Table(state);
    }

    Table Table::Load(std::string_view path)
    {
        std::ifstream file { std::string(path), std::ios::binary };
        if (!file)
        {
            auto state = std::make_shared<TableState>(TableSettings {});
            state->ErrorText = std::format("{}: the file could not be opened", path);
            return Table(state);
        }
        std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        return FromText(text, path);
    }

    Table Table::FromText(std::string_view text, std::string_view name)
    {
        auto state = std::make_shared<TableState>(TableSettings {});
        Result<> read = internal::ReadTree(*state, text, name);
        if (!read)
        {
            auto failed = std::make_shared<TableState>(TableSettings {});
            failed->ErrorText = read.Error();
            return Table(failed);
        }
        // What was read is where the table starts, not changes to it.
        state->Changes.clear();
        state->CurrentVersion = 0;
        state->Made = true;
        return Table(state);
    }

    Table::Table() : State(std::make_shared<TableState>(TableSettings {}))
    {
    }

    Table::Table(std::shared_ptr<internal::TableState> state) : State(std::move(state))
    {
    }

    Table::operator bool() const
    {
        return State->Made;
    }

    const std::string& Table::Error() const
    {
        return State->ErrorText;
    }

    Node Table::Add(std::string_view name, std::string_view type) const
    {
        if (!State->Made)
        {
            return {};
        }
        return State->Handle(State->AddNode(NoSlot, name, type));
    }

    Node Table::Add(std::string_view name, const PropertyValues& values) const
    {
        return Add(name, {}, values);
    }

    Node Table::Add(std::string_view name, std::string_view type, const PropertyValues& values) const
    {
        Node node = Add(name, type);
        for (const auto& [property, value] : values)
        {
            node[property] = value;
        }
        return node;
    }

    std::vector<Node> Table::TopNodes() const
    {
        std::vector<Node> nodes;
        for (std::uint32_t slot : State->ChildrenOf(NoSlot))
        {
            nodes.push_back(State->Handle(slot));
        }
        return nodes;
    }

    Node Table::Find(std::string_view path) const
    {
        std::uint32_t current = NoSlot;
        std::size_t start = 0;
        while (start <= path.size())
        {
            std::size_t slash = path.find('/', start);
            std::string_view name = path.substr(start, slash == std::string_view::npos ? std::string_view::npos : slash - start);
            current = State->FindChild(current, name);
            if (current == NoSlot)
            {
                return {};
            }
            if (slash == std::string_view::npos)
            {
                break;
            }
            start = slash + 1;
        }
        return State->Handle(current);
    }

    Node Table::FindByIdentifier(std::uint64_t identifier) const
    {
        for (std::uint32_t slot = 0; slot < State->Rows.size(); ++slot)
        {
            if (State->Rows[slot].Alive && State->Rows[slot].Identifier == identifier)
            {
                return State->Handle(slot);
            }
        }
        return {};
    }

    std::vector<Node> Table::Nodes() const
    {
        std::vector<std::uint32_t> slots;
        State->CollectTree(State->FirstTop, slots);
        std::vector<Node> nodes;
        nodes.reserve(slots.size());
        for (std::uint32_t slot : slots)
        {
            nodes.push_back(State->Handle(slot));
        }
        return nodes;
    }

    std::size_t Table::NodeCount() const
    {
        std::size_t count = 0;
        for (const internal::Row& row : State->Rows)
        {
            count += row.Alive ? 1 : 0;
        }
        return count;
    }

    std::vector<Node> Table::NodesWith(std::string_view property) const
    {
        std::vector<Node> nodes;
        std::vector<std::uint8_t> taken(State->Rows.size(), 0);
        auto column = State->Columns.find(property);
        if (column != State->Columns.end())
        {
            const std::vector<std::uint8_t>& present = column->second.Present;
            for (std::uint32_t slot = 0; slot < present.size(); ++slot)
            {
                if (present[slot] && State->Rows[slot].Alive)
                {
                    taken[slot] = 1;
                    nodes.push_back(State->Handle(slot));
                }
            }
        }
        // Nodes whose type gives the property.
        for (const auto& [type, values] : State->Types)
        {
            bool gives = false;
            for (const auto& [name, value] : values)
            {
                gives = gives || name == property;
            }
            if (!gives)
            {
                continue;
            }
            for (std::uint32_t slot = 0; slot < State->Rows.size(); ++slot)
            {
                const internal::Row& row = State->Rows[slot];
                if (row.Alive && !taken[slot] && row.Type == type)
                {
                    taken[slot] = 1;
                    nodes.push_back(State->Handle(slot));
                }
            }
        }
        return nodes;
    }

    std::vector<Node> Table::NodesOfType(std::string_view type) const
    {
        std::vector<Node> nodes;
        for (std::uint32_t slot = 0; slot < State->Rows.size(); ++slot)
        {
            if (State->Rows[slot].Alive && State->Rows[slot].Type == type)
            {
                nodes.push_back(State->Handle(slot));
            }
        }
        return nodes;
    }

    void Table::DefineType(std::string_view name, const PropertyValues& values) const
    {
        if (State->Made)
        {
            State->DefineType(std::string(name), values);
        }
    }

    PropertyValues Table::TypeValues(std::string_view name) const
    {
        auto found = State->Types.find(name);
        return found == State->Types.end() ? PropertyValues() : found->second;
    }

    std::vector<std::string> Table::TypeNames() const
    {
        std::vector<std::string> names;
        for (const auto& [name, values] : State->Types)
        {
            names.push_back(name);
        }
        return names;
    }

    std::uint64_t Table::Version() const
    {
        return State->CurrentVersion;
    }

    std::vector<Change> Table::ChangesSince(std::uint64_t version) const
    {
        std::vector<Change> changes;
        for (const internal::ChangeRecord& record : State->Changes)
        {
            if (record.Version <= version)
            {
                continue;
            }
            Change change;
            change.Version = record.Version;
            change.Kind = record.Kind;
            change.Target = State->Handle(record.Target, record.TargetGeneration);
            change.Parent = State->Handle(record.Parent, record.ParentGeneration);
            change.OldParent = State->Handle(record.OldParent, record.OldParentGeneration);
            change.Property = record.Property;
            change.Old = record.Old;
            change.New = record.New;
            changes.push_back(std::move(change));
        }
        return changes;
    }

    std::uint64_t Table::OldestVersion() const
    {
        return State->Changes.empty() ? State->CurrentVersion + 1 : State->Changes.front().Version;
    }

    void Table::BeginEdit(std::string_view name) const
    {
        State->BeginEdit(name);
    }

    void Table::EndEdit() const
    {
        State->EndEdit();
    }

    bool Table::Undo() const
    {
        return State->Undo();
    }

    bool Table::Redo() const
    {
        return State->Redo();
    }

    bool Table::CanUndo() const
    {
        return !State->UndoStack.empty() && State->EditDepth == 0;
    }

    bool Table::CanRedo() const
    {
        return !State->RedoStack.empty() && State->EditDepth == 0;
    }

    std::string Table::UndoName() const
    {
        return State->UndoStack.empty() ? std::string() : State->UndoStack.back().Name;
    }

    std::string Table::RedoName() const
    {
        return State->RedoStack.empty() ? std::string() : State->RedoStack.back().Name;
    }

    std::string Table::ToText() const
    {
        return internal::WriteTree(*State);
    }

    Result<> Table::Save(std::string_view path) const
    {
        std::string text = ToText();
        std::ofstream file { std::string(path), std::ios::binary };
        file << text;
        if (!file)
        {
            return Failure(std::format("{}: the table could not be written", path));
        }
        return {};
    }
}
