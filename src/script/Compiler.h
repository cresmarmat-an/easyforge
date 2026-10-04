#pragma once

// Turns a syntax tree into a prototype the machine runs, after checking the
// names and the types written in it.

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/language/Syntax.h>

#include "Heap.h"

namespace easyforge::internal::scripting
{
    struct Compiled
    {
        std::unique_ptr<Prototype> Main;
        std::vector<language::Problem> Problems;
    };

    struct CompileSettings
    {
        // Names the program and the built-ins define, which every module sees.
        std::function<bool(std::string_view name)> IsDefined;

        // Whether unknown names are problems. Running allows them, since the
        // program may define them later; checking reports them.
        bool ReportUnknownNames = false;
    };

    Compiled Compile(const language::SyntaxTree& tree, Heap& heap, const CompileSettings& settings);
}
