#pragma once

#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <easyforge/data/Table.h>

namespace easyforge::internal
{
    inline constexpr std::uint32_t NoSlot = 0xFFFFFFFF;

    // One node: its name and type, where it sits in the tree, and which of the
    // columns it has a value in. Tree links are row numbers, not pointers.
    struct Row
    {
        std::string Name;
        std::string Type;
        std::uint32_t Parent = NoSlot;
        std::uint32_t FirstChild = NoSlot;
        std::uint32_t LastChild = NoSlot;
        std::uint32_t Next = NoSlot;
        std::uint32_t Previous = NoSlot;
        std::uint32_t Generation = 0;
        std::uint64_t Identifier = 0;
        bool Alive = false;

        // Edits in the undo history that can bring this row back. A removed row is
        // only reused once none can.
        std::uint32_t HeldByHistory = 0;

        // The node's own properties, in the order they were first set.
        std::vector<std::string> PropertyOrder;
    };

    // One property of every node: a value per row, and whether the row has one.
    struct Column
    {
        std::vector<DataValue> Values;
        std::vector<std::uint8_t> Present;
    };

    // A change as the table keeps it: rows and generations, not handles, so the
    // table does not keep itself alive.
    struct ChangeRecord
    {
        std::uint64_t Version = 0;
        ChangeKind Kind = ChangeKind::PropertySet;
        std::uint32_t Target = NoSlot;
        std::uint32_t TargetGeneration = 0;
        std::uint32_t Parent = NoSlot;
        std::uint32_t ParentGeneration = 0;
        std::uint32_t OldParent = NoSlot;
        std::uint32_t OldParentGeneration = 0;
        std::string Property;
        DataValue Old;
        DataValue New;
    };

    // One step of an edit, with what is needed to reverse and repeat it.
    struct Operation
    {
        enum class Kind
        {
            Add,
            Remove,
            SetProperty,
            Rename,
            Move,
            DefineType,
        };

        Kind What = Kind::SetProperty;
        std::uint32_t Target = NoSlot;

        // The target's generation when the step was made; a step whose row has
        // been reused since is skipped.
        std::uint32_t TargetGeneration = 0;

        // Add, Remove, Move: where the node was, and where it went. `Next` is the
        // sibling it sat before, or NoSlot for the end.
        std::uint32_t OldParent = NoSlot;
        std::uint32_t OldNext = NoSlot;
        std::uint32_t NewParent = NoSlot;
        std::uint32_t NewNext = NoSlot;

        // SetProperty and Rename: the property or type name, and the values.
        std::string Property;
        std::optional<DataValue> Old;
        std::optional<DataValue> New;

        // DefineType: the type's values before and after.
        std::optional<std::vector<std::pair<std::string, DataValue>>> OldType;
        std::vector<std::pair<std::string, DataValue>> NewType;
    };

    struct Edit
    {
        std::string Name;
        std::vector<Operation> Operations;
    };

    class TableState : public std::enable_shared_from_this<TableState>
    {
    public:
        explicit TableState(const TableSettings& settings) : Settings(settings) {}

        bool Valid(std::uint32_t slot, std::uint32_t generation) const
        {
            return slot < Rows.size() && Rows[slot].Alive && Rows[slot].Generation == generation;
        }

        Node Handle(std::uint32_t slot);
        Node Handle(std::uint32_t slot, std::uint32_t generation);

        // Changes to the table. Each records its change and, inside an edit, the
        // operation that reverses it.
        std::uint32_t AddNode(std::uint32_t parent, std::string_view name, std::string_view type);
        void RemoveNode(std::uint32_t slot);
        void SetProperty(std::uint32_t slot, const std::string& property, std::optional<DataValue> value);
        void RenameNode(std::uint32_t slot, std::string_view name);
        void MoveNode(std::uint32_t slot, std::uint32_t parent);
        void DefineType(const std::string& name, std::vector<std::pair<std::string, DataValue>> values);

        // The node's own value, or its type's, or nothing.
        std::optional<DataValue> OwnValue(std::uint32_t slot, std::string_view property) const;
        std::optional<DataValue> TypeValue(std::uint32_t slot, std::string_view property) const;
        DataValue Read(std::uint32_t slot, std::string_view property) const;

        std::uint32_t FindChild(std::uint32_t parent, std::string_view name) const;
        std::vector<std::uint32_t> ChildrenOf(std::uint32_t parent) const;
        void CollectTree(std::uint32_t first, std::vector<std::uint32_t>& into) const;

        void BeginEdit(std::string_view name);
        void EndEdit();
        bool Undo();
        bool Redo();

        TableSettings Settings;
        std::string ErrorText;
        bool Made = false;

        std::vector<Row> Rows;
        std::map<std::string, Column, std::less<>> Columns;
        std::map<std::string, std::vector<std::pair<std::string, DataValue>>, std::less<>> Types;

        // The top of the tree is a list of its own.
        std::uint32_t FirstTop = NoSlot;
        std::uint32_t LastTop = NoSlot;

        std::uint64_t CurrentVersion = 0;
        std::deque<ChangeRecord> Changes;

        std::vector<Edit> UndoStack;
        std::vector<Edit> RedoStack;
        std::optional<Edit> OpenEdit;
        int EditDepth = 0;

    private:
        std::uint32_t NewRow();
        void Link(std::uint32_t slot, std::uint32_t parent, std::uint32_t before);
        void Unlink(std::uint32_t slot);
        void SetAlive(std::uint32_t slot, bool alive);
        void Record(ChangeRecord record);
        void Remember(Operation operation);
        void Apply(const Operation& operation, bool reverse);
        void Hold(const Edit& edit, int amount);
        void Recycle(std::uint32_t slot);
        void RecycleIfFree(std::uint32_t slot);
        void DropEdit(Edit& edit);

        std::vector<std::uint32_t> FreeRows;

        // Rows removed while an edit is open, held until it ends.
        std::vector<std::uint32_t> PendingHolds;

        std::uint64_t NextIdentifier = 1;
        bool Replaying = false;
    };
}
