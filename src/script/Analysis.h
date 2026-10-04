#pragma once

// The pass before compiling: finds which locals functions inside capture, and
// checks names and the types written in the source.

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <easyforge/core/language/Syntax.h>

#include "Compiler.h"

namespace easyforge::internal::scripting
{
    // A type as the checker knows it: one of the language's types, a record's
    // name, or `any` when nothing is known.
    struct TypeInfo
    {
        std::string Name = "any";
        std::shared_ptr<TypeInfo> Element;

        bool IsAny() const { return Name == "any"; }
        std::string Spelling() const { return Element ? Name + " of " + Element->Spelling() : Name; }
    };

    struct SignatureInfo
    {
        std::string Name;
        std::vector<TypeInfo> Parameters;
        TypeInfo Returns;
    };

    struct RecordInfo
    {
        std::string Name;
        std::vector<std::pair<std::string, TypeInfo>> Fields;
    };

    struct AnalysisResult
    {
        // Locals that a function inside reads or writes, so they live in boxes.
        std::unordered_set<const void*> Captured;
    };

    AnalysisResult Analyze(const language::Block& statements, const CompileSettings& settings,
        std::vector<language::Problem>& problems);
}
