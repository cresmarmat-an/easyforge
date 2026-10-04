#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <easyforge/core/Result.h>
#include <easyforge/data/DataValue.h>

namespace easyforge
{
    namespace internal
    {
        class TableState;
    }

    class Node;
    class Table;

    // Property names and values, in order: { { "Health", 100 }, { "Speed", 4.5f } }.
    using PropertyValues = std::vector<std::pair<std::string, DataValue>>;

    // One property of one node, read and assigned like a variable:
    //
    //     player["Health"] = 100;
    //     int health = player["Health"];
    //     player["Health"] += 5;
    //
    // Reading gives the node's own value, or else its type's value, or else
    // nothing. Write the type you read into; `auto` keeps the cell itself.
    class Cell
    {
    public:
        Cell(std::shared_ptr<internal::TableState> table, std::uint32_t slot, std::uint32_t generation, std::string property)
            : Owner(std::move(table)), Slot(slot), Generation(generation), Property(std::move(property))
        {
        }

        const Cell& operator=(const DataValue& value) const;
        const Cell& operator=(const Cell& other) const { return *this = other.Get(); }

        // Adds to a whole number, number, or vector, keeping its type.
        const Cell& operator+=(const DataValue& amount) const;
        const Cell& operator-=(const DataValue& amount) const;

        DataValue Get() const;

        // The value as a type, for use in an expression: `player["Health"].As<int>() <= 0`.
        template <DataReadable Type>
        Type As() const
        {
            return Get().template As<Type>();
        }

        template <DataReadable Type>
        operator Type() const
        {
            return Get().template As<Type>();
        }

        // True when the node, or its type, has the property.
        bool Exists() const;

        // Removes the node's own value, so the type's value, if any, shows again.
        void Clear() const;

    private:
        std::shared_ptr<internal::TableState> Owner;
        std::uint32_t Slot;
        std::uint32_t Generation;
        std::string Property;
    };

    // One node of a table: a name, a type, properties, and children.
    //
    // Node is a handle. It stays valid until the node is removed, and becomes
    // valid again if the removal is undone.
    class Node
    {
    public:
        // No node. Tests as false.
        Node() = default;

        // True while the node is in its table.
        explicit operator bool() const;

        Cell operator[](std::string_view property) const;

        // Reads and sets a property directly, as a cell does, for code that reads
        // many properties in a row. Setting nothing clears the node's own value.
        DataValue Get(std::string_view property) const;
        void Set(std::string_view property, const DataValue& value) const;

        // Adds a child at the end of this node's children, optionally with a type
        // and starting values.
        Node Add(std::string_view name, std::string_view type = {}) const;
        Node Add(std::string_view name, const PropertyValues& values) const;
        Node Add(std::string_view name, std::string_view type, const PropertyValues& values) const;

        // Removes the node and everything under it.
        void Remove() const;

        std::string Name() const;
        void Rename(std::string_view name) const;

        // The type named when the node was added, or empty.
        std::string TypeName() const;

        // The node above, or no node for a node at the top of its table.
        Node Parent() const;

        std::vector<Node> Children() const;
        std::size_t ChildCount() const;

        // The first child with the name, or no node.
        Node Child(std::string_view name) const;

        // Moves the node, and everything under it, to the end of another node's
        // children, or to the top of the table when `parent` is no node.
        void MoveTo(const Node& parent) const;

        bool Has(std::string_view property) const;

        // The node's own properties, not its type's, in the order they were set.
        std::vector<std::string> Properties() const;

        // A number that no other node of the table has had or will have. Loading a
        // saved table numbers its nodes again.
        std::uint64_t Identifier() const;

        // The names from the top of the table down to this node: "Player/Sword".
        std::string Path() const;

        bool operator==(const Node& other) const
        {
            return Owner == other.Owner && Slot == other.Slot && Generation == other.Generation;
        }

    private:
        friend class Table;
        friend class internal::TableState;
        Node(std::shared_ptr<internal::TableState> table, std::uint32_t slot, std::uint32_t generation)
            : Owner(std::move(table)), Slot(slot), Generation(generation)
        {
        }

        bool Valid() const;

        std::shared_ptr<internal::TableState> Owner;
        std::uint32_t Slot = 0;
        std::uint32_t Generation = 0;
    };

    enum class ChangeKind
    {
        // Target was added under Parent.
        Added,

        // Target, and everything under it, was removed from Parent.
        Removed,

        // Property of Target went from Old to New. Old is nothing for a property
        // that was not set, and New is nothing for one that was cleared.
        PropertySet,

        // Target's name went from Old to New.
        Renamed,

        // Target moved from OldParent to Parent.
        Moved,

        // The type named Property got new values.
        TypeDefined,
    };

    // One change to a table, as ChangesSince reports it.
    struct Change
    {
        std::uint64_t Version = 0;
        ChangeKind Kind = ChangeKind::PropertySet;
        Node Target;
        Node Parent;
        Node OldParent;
        std::string Property;
        DataValue Old;
        DataValue New;
    };

    struct TableSettings
    {
        // How many changes ChangesSince remembers. Older ones are forgotten; see
        // OldestVersion.
        std::size_t ChangeLimit = 100000;

        // How many edits Undo can go back.
        std::size_t UndoLimit = 100;
    };

    // A tree of nodes, stored like a table: one row per node, one column per
    // property. Every change is recorded, and edits can be undone.
    //
    //     Table game = Table::New();
    //     Node player = game.Add("Player");
    //     player["Health"] = 100;
    //     Node sword = player.Add("Sword");
    //     sword["Damage"] = 12;
    //     game.Save("save.tree");
    //
    // Table is a handle: copies refer to the same table.
    class Table
    {
    public:
        static Table New(const TableSettings& settings = {});

        // Reads a table saved with Save. A table that could not be read tests as
        // false, and Error() says why.
        static Table Load(std::string_view path);
        static Table FromText(std::string_view text, std::string_view name = "table");

        // No table. Tests as false.
        Table();

        explicit operator bool() const;
        const std::string& Error() const;

        // Adds a node at the top of the table, optionally with a type and starting
        // values.
        Node Add(std::string_view name, std::string_view type = {}) const;
        Node Add(std::string_view name, const PropertyValues& values) const;
        Node Add(std::string_view name, std::string_view type, const PropertyValues& values) const;

        // The nodes at the top of the table.
        std::vector<Node> TopNodes() const;

        // A node by its path of names, such as "Player/Sword", or no node.
        Node Find(std::string_view path) const;

        Node FindByIdentifier(std::uint64_t identifier) const;

        // Every node, parents before children, in order.
        std::vector<Node> Nodes() const;
        std::size_t NodeCount() const;

        // Every node that has the property, its own or its type's. Each property
        // is stored as one column, so this is one pass over that column.
        std::vector<Node> NodesWith(std::string_view property) const;

        std::vector<Node> NodesOfType(std::string_view type) const;

        // Defines, or redefines, a type: the values its nodes read when they
        // have none of their own.
        //
        //     game.DefineType("Enemy", { { "Health", 100 }, { "Speed", 4.5f } });
        void DefineType(std::string_view name, const PropertyValues& values) const;
        PropertyValues TypeValues(std::string_view name) const;
        std::vector<std::string> TypeNames() const;

        // Goes up by one with every change.
        std::uint64_t Version() const;

        // Every change after `version`, oldest first.
        std::vector<Change> ChangesSince(std::uint64_t version) const;

        // The first version ChangesSince still remembers. A reader that last looked
        // before it has missed changes, and should read the table again.
        std::uint64_t OldestVersion() const;

        // Groups the changes until EndEdit into one edit, which Undo reverses as
        // one. Edits can nest; the outermost one counts. Changes made outside an
        // edit are recorded, but cannot be undone.
        void BeginEdit(std::string_view name) const;
        void EndEdit() const;

        bool Undo() const;
        bool Redo() const;
        bool CanUndo() const;
        bool CanRedo() const;

        // The name of the edit Undo or Redo would reverse, or empty.
        std::string UndoName() const;
        std::string RedoName() const;

        // The table in the .tree format.
        std::string ToText() const;
        Result<> Save(std::string_view path) const;

        bool operator==(const Table& other) const { return State == other.State; }

    private:
        explicit Table(std::shared_ptr<internal::TableState> state);

        std::shared_ptr<internal::TableState> State;
    };
}
