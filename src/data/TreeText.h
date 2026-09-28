#pragma once

// The .tree format: a readable text form of a table.
//
//     type Enemy
//         Health = 100
//         Speed = 4.5
//
//     Player
//         Health = 100
//         Position = 10, 20
//         Sword
//             Damage = 12
//     Goblin : Enemy
//         Health = 90
//
// Each level is four spaces deeper. A line with `=` sets a property of the node
// above it; any other line is a node, with its type after a colon. Types come
// first, each starting with `type`. `--` starts a comment. Names with `=`, `:`,
// or spaces at either end are written in quotes.

#include <string>
#include <string_view>

#include <easyforge/core/Result.h>

namespace easyforge::internal
{
    class TableState;

    std::string WriteTree(const TableState& table);
    Result<> ReadTree(TableState& table, std::string_view text, std::string_view name);
}
