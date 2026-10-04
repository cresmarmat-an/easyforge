#include "TableState.h"

#include <algorithm>

#include <easyforge/core/Log.h>

namespace easyforge::internal
{
    Node TableState::Handle(std::uint32_t slot)
    {
        if (slot == NoSlot || slot >= Rows.size())
        {
            return {};
        }
        return Node(shared_from_this(), slot, Rows[slot].Generation);
    }

    Node TableState::Handle(std::uint32_t slot, std::uint32_t generation)
    {
        if (slot == NoSlot)
        {
            return {};
        }
        return Node(shared_from_this(), slot, generation);
    }

    std::uint32_t TableState::NewRow()
    {
        if (!FreeRows.empty())
        {
            std::uint32_t slot = FreeRows.back();
            FreeRows.pop_back();
            return slot;
        }
        Rows.emplace_back();
        return static_cast<std::uint32_t>(Rows.size() - 1);
    }

    void TableState::Link(std::uint32_t slot, std::uint32_t parent, std::uint32_t before)
    {
        std::uint32_t& first = parent == NoSlot ? FirstTop : Rows[parent].FirstChild;
        std::uint32_t& last = parent == NoSlot ? LastTop : Rows[parent].LastChild;
        // A sibling to go before has to still be one; otherwise the node goes last.
        if (before != NoSlot && (before >= Rows.size() || !Rows[before].Alive || Rows[before].Parent != parent))
        {
            before = NoSlot;
        }
        Row& row = Rows[slot];
        row.Parent = parent;
        if (before == NoSlot)
        {
            row.Previous = last;
            row.Next = NoSlot;
            if (last != NoSlot)
            {
                Rows[last].Next = slot;
            }
            else
            {
                first = slot;
            }
            last = slot;
            return;
        }
        Row& after = Rows[before];
        row.Next = before;
        row.Previous = after.Previous;
        if (after.Previous != NoSlot)
        {
            Rows[after.Previous].Next = slot;
        }
        else
        {
            first = slot;
        }
        after.Previous = slot;
    }

    void TableState::Unlink(std::uint32_t slot)
    {
        Row& row = Rows[slot];
        std::uint32_t& first = row.Parent == NoSlot ? FirstTop : Rows[row.Parent].FirstChild;
        std::uint32_t& last = row.Parent == NoSlot ? LastTop : Rows[row.Parent].LastChild;
        if (row.Previous != NoSlot)
        {
            Rows[row.Previous].Next = row.Next;
        }
        else
        {
            first = row.Next;
        }
        if (row.Next != NoSlot)
        {
            Rows[row.Next].Previous = row.Previous;
        }
        else
        {
            last = row.Previous;
        }
        row.Next = NoSlot;
        row.Previous = NoSlot;
    }

    void TableState::SetAlive(std::uint32_t slot, bool alive)
    {
        std::vector<std::uint32_t> tree { slot };
        CollectTree(Rows[slot].FirstChild, tree);
        for (std::uint32_t member : tree)
        {
            Rows[member].Alive = alive;
        }
    }

    void TableState::CollectTree(std::uint32_t first, std::vector<std::uint32_t>& into) const
    {
        for (std::uint32_t child = first; child != NoSlot; child = Rows[child].Next)
        {
            into.push_back(child);
            CollectTree(Rows[child].FirstChild, into);
        }
    }

    std::vector<std::uint32_t> TableState::ChildrenOf(std::uint32_t parent) const
    {
        std::vector<std::uint32_t> children;
        std::uint32_t first = parent == NoSlot ? FirstTop : Rows[parent].FirstChild;
        for (std::uint32_t child = first; child != NoSlot; child = Rows[child].Next)
        {
            children.push_back(child);
        }
        return children;
    }

    std::uint32_t TableState::FindChild(std::uint32_t parent, std::string_view name) const
    {
        std::uint32_t first = parent == NoSlot ? FirstTop : Rows[parent].FirstChild;
        for (std::uint32_t child = first; child != NoSlot; child = Rows[child].Next)
        {
            if (Rows[child].Name == name)
            {
                return child;
            }
        }
        return NoSlot;
    }

    void TableState::Record(ChangeRecord record)
    {
        record.Version = ++CurrentVersion;
        if (Settings.ChangeLimit == 0)
        {
            return;
        }
        Changes.push_back(std::move(record));
        while (Changes.size() > Settings.ChangeLimit)
        {
            Changes.pop_front();
        }
    }

    void TableState::Remember(Operation operation)
    {
        if (OpenEdit && !Replaying)
        {
            OpenEdit->Operations.push_back(std::move(operation));
        }
    }

    std::uint32_t TableState::AddNode(std::uint32_t parent, std::string_view name, std::string_view type)
    {
        std::uint32_t slot = NewRow();
        Row& row = Rows[slot];
        row.Name = std::string(name);
        row.Type = std::string(type);
        row.Identifier = NextIdentifier++;
        row.Alive = true;
        Link(slot, parent, NoSlot);

        ChangeRecord record;
        record.Kind = ChangeKind::Added;
        record.Target = slot;
        record.TargetGeneration = row.Generation;
        record.Parent = parent;
        record.ParentGeneration = parent == NoSlot ? 0 : Rows[parent].Generation;
        Record(std::move(record));

        Operation operation;
        operation.What = Operation::Kind::Add;
        operation.Target = slot;
        operation.TargetGeneration = row.Generation;
        operation.NewParent = parent;
        Remember(std::move(operation));
        return slot;
    }

    void TableState::RemoveNode(std::uint32_t slot)
    {
        Row& row = Rows[slot];
        std::uint32_t parent = row.Parent;
        std::uint32_t next = row.Next;
        Unlink(slot);
        SetAlive(slot, false);

        ChangeRecord record;
        record.Kind = ChangeKind::Removed;
        record.Target = slot;
        record.TargetGeneration = row.Generation;
        record.Parent = parent;
        record.ParentGeneration = parent == NoSlot ? 0 : Rows[parent].Generation;
        Record(std::move(record));

        if (OpenEdit && !Replaying)
        {
            Operation operation;
            operation.What = Operation::Kind::Remove;
            operation.Target = slot;
            operation.TargetGeneration = Rows[slot].Generation;
            operation.OldParent = parent;
            operation.OldNext = next;
            Remember(std::move(operation));
            // Held until the edit is kept or dropped, so undo can bring it back.
            ++Rows[slot].HeldByHistory;
            PendingHolds.push_back(slot);
            return;
        }
        if (!Replaying)
        {
            RecycleIfFree(slot);
        }
    }

    std::optional<DataValue> TableState::OwnValue(std::uint32_t slot, std::string_view property) const
    {
        auto column = Columns.find(property);
        if (column == Columns.end() || slot >= column->second.Present.size() || !column->second.Present[slot])
        {
            return std::nullopt;
        }
        return column->second.Values[slot];
    }

    std::optional<DataValue> TableState::TypeValue(std::uint32_t slot, std::string_view property) const
    {
        const std::string& type = Rows[slot].Type;
        if (type.empty())
        {
            return std::nullopt;
        }
        auto found = Types.find(type);
        if (found == Types.end())
        {
            return std::nullopt;
        }
        for (const auto& [name, value] : found->second)
        {
            if (name == property)
            {
                return value;
            }
        }
        return std::nullopt;
    }

    DataValue TableState::Read(std::uint32_t slot, std::string_view property) const
    {
        if (std::optional<DataValue> own = OwnValue(slot, property))
        {
            return *own;
        }
        return TypeValue(slot, property).value_or(DataValue());
    }

    void TableState::SetProperty(std::uint32_t slot, const std::string& property, std::optional<DataValue> value)
    {
        std::optional<DataValue> old = OwnValue(slot, property);
        if (old == value)
        {
            return;
        }
        Column& column = Columns[property];
        if (column.Present.size() <= slot)
        {
            column.Present.resize(Rows.size(), 0);
            column.Values.resize(Rows.size());
        }
        Row& row = Rows[slot];
        if (value)
        {
            column.Values[slot] = *value;
            if (!column.Present[slot])
            {
                column.Present[slot] = 1;
                row.PropertyOrder.push_back(property);
            }
        }
        else
        {
            column.Values[slot] = DataValue();
            column.Present[slot] = 0;
            std::erase(row.PropertyOrder, property);
        }

        ChangeRecord record;
        record.Kind = ChangeKind::PropertySet;
        record.Target = slot;
        record.TargetGeneration = row.Generation;
        record.Property = property;
        record.Old = old.value_or(DataValue());
        record.New = value.value_or(DataValue());
        Record(std::move(record));

        Operation operation;
        operation.What = Operation::Kind::SetProperty;
        operation.Target = slot;
        operation.TargetGeneration = row.Generation;
        operation.Property = property;
        operation.Old = std::move(old);
        operation.New = std::move(value);
        Remember(std::move(operation));
    }

    void TableState::RenameNode(std::uint32_t slot, std::string_view name)
    {
        Row& row = Rows[slot];
        if (row.Name == name)
        {
            return;
        }
        std::string old = row.Name;
        row.Name = std::string(name);

        ChangeRecord record;
        record.Kind = ChangeKind::Renamed;
        record.Target = slot;
        record.TargetGeneration = row.Generation;
        record.Old = old;
        record.New = row.Name;
        Record(std::move(record));

        Operation operation;
        operation.What = Operation::Kind::Rename;
        operation.Target = slot;
        operation.TargetGeneration = row.Generation;
        operation.Old = DataValue(old);
        operation.New = DataValue(row.Name);
        Remember(std::move(operation));
    }

    void TableState::MoveNode(std::uint32_t slot, std::uint32_t parent)
    {
        // A node cannot go inside itself.
        for (std::uint32_t above = parent; above != NoSlot; above = Rows[above].Parent)
        {
            if (above == slot)
            {
                Log(LogLevel::Warning, "Node::MoveTo: {} cannot be moved inside itself", Rows[slot].Name);
                return;
            }
        }
        Row& row = Rows[slot];
        std::uint32_t oldParent = row.Parent;
        std::uint32_t oldNext = row.Next;
        Unlink(slot);
        Link(slot, parent, NoSlot);

        ChangeRecord record;
        record.Kind = ChangeKind::Moved;
        record.Target = slot;
        record.TargetGeneration = row.Generation;
        record.Parent = parent;
        record.ParentGeneration = parent == NoSlot ? 0 : Rows[parent].Generation;
        record.OldParent = oldParent;
        record.OldParentGeneration = oldParent == NoSlot ? 0 : Rows[oldParent].Generation;
        Record(std::move(record));

        Operation operation;
        operation.What = Operation::Kind::Move;
        operation.Target = slot;
        operation.TargetGeneration = row.Generation;
        operation.OldParent = oldParent;
        operation.OldNext = oldNext;
        operation.NewParent = parent;
        Remember(std::move(operation));
    }

    void TableState::DefineType(const std::string& name, std::vector<std::pair<std::string, DataValue>> values)
    {
        std::optional<std::vector<std::pair<std::string, DataValue>>> old;
        if (auto found = Types.find(name); found != Types.end())
        {
            old = found->second;
        }
        Types[name] = values;

        ChangeRecord record;
        record.Kind = ChangeKind::TypeDefined;
        record.Property = name;
        Record(std::move(record));

        Operation operation;
        operation.What = Operation::Kind::DefineType;
        operation.Property = name;
        operation.OldType = std::move(old);
        operation.NewType = std::move(values);
        Remember(std::move(operation));
    }

    void TableState::BeginEdit(std::string_view name)
    {
        if (EditDepth++ == 0)
        {
            OpenEdit = Edit { std::string(name), {} };
        }
    }

    void TableState::EndEdit()
    {
        if (EditDepth == 0)
        {
            Log(LogLevel::Warning, "Table::EndEdit was called more times than BeginEdit");
            return;
        }
        if (--EditDepth > 0)
        {
            return;
        }
        Edit edit = std::move(*OpenEdit);
        OpenEdit.reset();
        // Rows removed during the edit were held while it was open; the edit holds
        // them now, or lets them go if it has nothing to undo.
        for (std::uint32_t slot : PendingHolds)
        {
            --Rows[slot].HeldByHistory;
        }
        PendingHolds.clear();
        if (edit.Operations.empty())
        {
            return;
        }
        for (Edit& dropped : RedoStack)
        {
            DropEdit(dropped);
        }
        RedoStack.clear();
        Hold(edit, 1);
        UndoStack.push_back(std::move(edit));
        while (UndoStack.size() > Settings.UndoLimit)
        {
            DropEdit(UndoStack.front());
            UndoStack.erase(UndoStack.begin());
        }
    }

    void TableState::Hold(const Edit& edit, int amount)
    {
        for (const Operation& operation : edit.Operations)
        {
            if ((operation.What == Operation::Kind::Add || operation.What == Operation::Kind::Remove) &&
                operation.Target < Rows.size() && Rows[operation.Target].Generation == operation.TargetGeneration)
            {
                Rows[operation.Target].HeldByHistory =
                    static_cast<std::uint32_t>(static_cast<int>(Rows[operation.Target].HeldByHistory) + amount);
            }
        }
    }

    void TableState::DropEdit(Edit& edit)
    {
        Hold(edit, -1);
        for (const Operation& operation : edit.Operations)
        {
            if ((operation.What == Operation::Kind::Add || operation.What == Operation::Kind::Remove) &&
                operation.Target < Rows.size() && Rows[operation.Target].Generation == operation.TargetGeneration)
            {
                RecycleIfFree(operation.Target);
            }
        }
    }

    void TableState::RecycleIfFree(std::uint32_t slot)
    {
        Row& row = Rows[slot];
        if (row.Alive || row.HeldByHistory > 0)
        {
            return;
        }
        std::vector<std::uint32_t> tree { slot };
        CollectTree(row.FirstChild, tree);
        for (std::uint32_t member : tree)
        {
            if (Rows[member].HeldByHistory > 0)
            {
                return;
            }
        }
        for (std::uint32_t member : tree)
        {
            Recycle(member);
        }
    }

    void TableState::Recycle(std::uint32_t slot)
    {
        for (auto& [name, column] : Columns)
        {
            if (slot < column.Present.size())
            {
                column.Present[slot] = 0;
                column.Values[slot] = DataValue();
            }
        }
        Row& row = Rows[slot];
        std::uint32_t generation = row.Generation + 1;
        row = Row {};
        row.Generation = generation;
        FreeRows.push_back(slot);
    }

    void TableState::Apply(const Operation& operation, bool reverse)
    {
        if (operation.What != Operation::Kind::DefineType &&
            (operation.Target >= Rows.size() || Rows[operation.Target].Generation != operation.TargetGeneration))
        {
            return;
        }
        std::uint32_t slot = operation.Target;
        switch (operation.What)
        {
        case Operation::Kind::Add:
        case Operation::Kind::Remove:
        {
            // Undoing an add, or repeating a remove, takes the node out; the other
            // two put it back where it was.
            bool takeOut = (operation.What == Operation::Kind::Add) == reverse;
            Row& row = Rows[slot];
            if (takeOut && row.Alive)
            {
                std::uint32_t parent = row.Parent;
                Unlink(slot);
                SetAlive(slot, false);
                ChangeRecord record;
                record.Kind = ChangeKind::Removed;
                record.Target = slot;
                record.TargetGeneration = row.Generation;
                record.Parent = parent;
                record.ParentGeneration = parent == NoSlot ? 0 : Rows[parent].Generation;
                Record(std::move(record));
            }
            else if (!takeOut && !row.Alive)
            {
                std::uint32_t parent = operation.What == Operation::Kind::Add ? operation.NewParent : operation.OldParent;
                std::uint32_t next = operation.What == Operation::Kind::Add ? operation.NewNext : operation.OldNext;
                if (parent != NoSlot && !Rows[parent].Alive)
                {
                    return;
                }
                Link(slot, parent, next);
                SetAlive(slot, true);
                ChangeRecord record;
                record.Kind = ChangeKind::Added;
                record.Target = slot;
                record.TargetGeneration = row.Generation;
                record.Parent = parent;
                record.ParentGeneration = parent == NoSlot ? 0 : Rows[parent].Generation;
                Record(std::move(record));
            }
            return;
        }
        case Operation::Kind::SetProperty:
            if (Rows[slot].Alive)
            {
                SetProperty(slot, operation.Property, reverse ? operation.Old : operation.New);
            }
            return;
        case Operation::Kind::Rename:
            if (Rows[slot].Alive)
            {
                RenameNode(slot, (reverse ? operation.Old : operation.New)->AsText());
            }
            return;
        case Operation::Kind::Move:
        {
            if (!Rows[slot].Alive)
            {
                return;
            }
            std::uint32_t parent = reverse ? operation.OldParent : operation.NewParent;
            std::uint32_t next = reverse ? operation.OldNext : operation.NewNext;
            if (parent != NoSlot && !Rows[parent].Alive)
            {
                return;
            }
            std::uint32_t oldParent = Rows[slot].Parent;
            Unlink(slot);
            Link(slot, parent, next);
            ChangeRecord record;
            record.Kind = ChangeKind::Moved;
            record.Target = slot;
            record.TargetGeneration = Rows[slot].Generation;
            record.Parent = parent;
            record.ParentGeneration = parent == NoSlot ? 0 : Rows[parent].Generation;
            record.OldParent = oldParent;
            record.OldParentGeneration = oldParent == NoSlot ? 0 : Rows[oldParent].Generation;
            Record(std::move(record));
            return;
        }
        case Operation::Kind::DefineType:
        {
            if (reverse && !operation.OldType)
            {
                Types.erase(operation.Property);
                ChangeRecord record;
                record.Kind = ChangeKind::TypeDefined;
                record.Property = operation.Property;
                Record(std::move(record));
                return;
            }
            DefineType(operation.Property, reverse ? *operation.OldType : operation.NewType);
            return;
        }
        }
    }

    bool TableState::Undo()
    {
        if (UndoStack.empty() || EditDepth > 0)
        {
            return false;
        }
        Edit edit = std::move(UndoStack.back());
        UndoStack.pop_back();
        Replaying = true;
        for (auto operation = edit.Operations.rbegin(); operation != edit.Operations.rend(); ++operation)
        {
            Apply(*operation, true);
        }
        Replaying = false;
        RedoStack.push_back(std::move(edit));
        return true;
    }

    bool TableState::Redo()
    {
        if (RedoStack.empty() || EditDepth > 0)
        {
            return false;
        }
        Edit edit = std::move(RedoStack.back());
        RedoStack.pop_back();
        Replaying = true;
        for (const Operation& operation : edit.Operations)
        {
            Apply(operation, false);
        }
        Replaying = false;
        UndoStack.push_back(std::move(edit));
        return true;
    }
}
