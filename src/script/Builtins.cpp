#include "Builtins.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <format>
#include <numbers>
#include <random>

#include "EngineState.h"
#include "Machine.h"

namespace easyforge::internal::scripting
{
    namespace
    {
        bool IsKind(Value value, ObjectKind kind) { return value.IsReference() && value.Pointer->Kind == kind; }

        template <typename Type>
        Type* As(Value value)
        {
            return static_cast<Type*>(value.Pointer);
        }

        Value TextValue(Machine& machine, std::string_view text)
        {
            return Value::Of(static_cast<Object*>(machine.Engine.TheHeap.Text(text)));
        }

        // ---- UTF-8 positions ----------------------------------------------------

        bool Continues(char character) { return (static_cast<unsigned char>(character) & 0xC0) == 0x80; }

        std::size_t CountCharacters(const std::string& text)
        {
            std::size_t count = 0;
            for (char character : text)
            {
                count += Continues(character) ? 0 : 1;
            }
            return count;
        }

        // The byte where character `index` (from 0) starts, or the text's size.
        std::size_t ByteOfCharacter(const std::string& text, std::size_t index)
        {
            std::size_t seen = 0;
            for (std::size_t byte = 0; byte < text.size(); ++byte)
            {
                if (!Continues(text[byte]))
                {
                    if (seen == index)
                    {
                        return byte;
                    }
                    ++seen;
                }
            }
            return text.size();
        }

        // ---- Checking arguments ---------------------------------------------------

        bool Count(Machine& machine, std::string_view name, std::span<Value> arguments, std::size_t least, std::size_t most)
        {
            if (arguments.size() >= least && arguments.size() <= most)
            {
                return true;
            }
            if (least == most)
            {
                return machine.Fail(std::format("{} takes {} argument{}, but was given {}", name, least, least == 1 ? "" : "s",
                    arguments.size()));
            }
            return machine.Fail(std::format("{} takes {} to {} arguments, but was given {}", name, least, most, arguments.size()));
        }

        std::string_view Ordinal(std::size_t index)
        {
            static constexpr std::string_view words[] = { "first", "second", "third", "fourth", "fifth" };
            return index < std::size(words) ? words[index] : "next";
        }

        bool Number(Machine& machine, std::string_view name, std::span<Value> arguments, std::size_t index, double& number)
        {
            if (!arguments[index].IsNumber())
            {
                return machine.Fail(std::format("{}'s {} argument should be a number, but it is {}", name, Ordinal(index),
                    machine.Engine.Describe(arguments[index])));
            }
            number = arguments[index].Number;
            return true;
        }

        bool Whole(Machine& machine, std::string_view name, std::span<Value> arguments, std::size_t index, long long& whole)
        {
            double number = 0.0;
            if (!Number(machine, name, arguments, index, number))
            {
                return false;
            }
            if (number != std::floor(number))
            {
                return machine.Fail(std::format("{}'s {} argument should be a whole number, but it is {}", name, Ordinal(index),
                    NumberText(number)));
            }
            whole = static_cast<long long>(number);
            return true;
        }

        bool Text(Machine& machine, std::string_view name, std::span<Value> arguments, std::size_t index, std::string& text)
        {
            if (!IsKind(arguments[index], ObjectKind::Text))
            {
                return machine.Fail(std::format("{}'s {} argument should be a string, but it is {}", name, Ordinal(index),
                    machine.Engine.Describe(arguments[index])));
            }
            text = As<TextObject>(arguments[index])->Text;
            return true;
        }

        bool Table(Machine& machine, std::string_view name, std::span<Value> arguments, std::size_t index, TableObject*& table)
        {
            if (!IsKind(arguments[index], ObjectKind::Table))
            {
                return machine.Fail(std::format("{}'s {} argument should be a table, but it is {}", name, Ordinal(index),
                    machine.Engine.Describe(arguments[index])));
            }
            table = As<TableObject>(arguments[index]);
            return true;
        }

        void Define(EngineState& engine, std::string_view name, BuiltinFunction function)
        {
            NativeObject* native = engine.TheHeap.Make<NativeObject>();
            native->Name = std::string(name);
            native->Builtin = std::move(function);
            engine.Globals->Set(engine.TheHeap.Text(name), Value::Of(static_cast<Object*>(native)));
        }

        // A function of one number, such as SquareRoot.
        void DefineMath(EngineState& engine, std::string_view name, double (*function)(double))
        {
            std::string kept(name);
            Define(engine, name, [kept, function](Machine& machine, std::span<Value> arguments, Value& result) {
                double number = 0.0;
                if (!Count(machine, kept, arguments, 1, 1) || !Number(machine, kept, arguments, 0, number))
                {
                    return false;
                }
                result = Value::Of(function(number));
                return true;
            });
        }

        // ---- Members of lists ---------------------------------------------------------

        MemberResult ListMember(Machine& machine, const std::string& name, std::span<Value> arguments, Value& result)
        {
            ListObject* list = As<ListObject>(arguments[0]);
            std::span<Value> given = arguments.subspan(1);
            std::string label = "list." + name;
            auto fail = [](bool ok) { return ok ? MemberResult::Found : MemberResult::Failed; };
            auto position = [&](std::size_t index, std::size_t last, std::size_t& at) {
                long long whole = 0;
                if (!Whole(machine, label, given, index, whole))
                {
                    return false;
                }
                if (whole < 1 || static_cast<std::size_t>(whole) > last)
                {
                    return machine.Fail(std::format("position {} is outside the list, which has {} item{}", whole,
                        list->Items.size(), list->Items.size() == 1 ? "" : "s"));
                }
                at = static_cast<std::size_t>(whole - 1);
                return true;
            };
            if (name == "Add")
            {
                if (!Count(machine, label, given, 1, 1))
                {
                    return MemberResult::Failed;
                }
                list->Items.push_back(given[0]);
                machine.Engine.TheHeap.Grew(*list, sizeof(Value));
                return MemberResult::Found;
            }
            if (name == "Insert")
            {
                std::size_t at = 0;
                if (!Count(machine, label, given, 2, 2) || !position(0, list->Items.size() + 1, at))
                {
                    return MemberResult::Failed;
                }
                list->Items.insert(list->Items.begin() + static_cast<std::ptrdiff_t>(at), given[1]);
                machine.Engine.TheHeap.Grew(*list, sizeof(Value));
                return MemberResult::Found;
            }
            if (name == "Remove")
            {
                std::size_t at = 0;
                if (!Count(machine, label, given, 1, 1) || !position(0, list->Items.size(), at))
                {
                    return MemberResult::Failed;
                }
                result = list->Items[at];
                list->Items.erase(list->Items.begin() + static_cast<std::ptrdiff_t>(at));
                return MemberResult::Found;
            }
            if (name == "Contains" || name == "Find")
            {
                if (!Count(machine, label, given, 1, 1))
                {
                    return MemberResult::Failed;
                }
                for (std::size_t index = 0; index < list->Items.size(); ++index)
                {
                    if (machine.Engine.Same(list->Items[index], given[0]))
                    {
                        result = name == "Contains" ? Value::Of(true) : Value::Of(static_cast<double>(index + 1));
                        return MemberResult::Found;
                    }
                }
                result = name == "Contains" ? Value::Of(false) : Value();
                return MemberResult::Found;
            }
            if (name == "Clear")
            {
                list->Items.clear();
                return fail(Count(machine, label, given, 0, 0));
            }
            if (name == "Reverse")
            {
                std::reverse(list->Items.begin(), list->Items.end());
                return fail(Count(machine, label, given, 0, 0));
            }
            if (name == "Copy")
            {
                ListObject* copy = machine.Engine.TheHeap.Make<ListObject>();
                copy->Items = list->Items;
                machine.Engine.TheHeap.Grew(*copy, copy->Items.size() * sizeof(Value));
                result = Value::Of(static_cast<Object*>(copy));
                return fail(Count(machine, label, given, 0, 0));
            }
            if (name == "Sort")
            {
                if (!Count(machine, label, given, 0, 0))
                {
                    return MemberResult::Failed;
                }
                bool numbers = std::all_of(list->Items.begin(), list->Items.end(), [](Value item) { return item.IsNumber(); });
                bool texts = std::all_of(list->Items.begin(), list->Items.end(), [](Value item) { return IsKind(item, ObjectKind::Text); });
                if (!numbers && !texts)
                {
                    return fail(machine.Fail("list.Sort puts numbers or strings in order, and this list holds other things too"));
                }
                std::stable_sort(list->Items.begin(), list->Items.end(), [numbers](Value first, Value second) {
                    return numbers ? first.Number < second.Number : As<TextObject>(first)->Text < As<TextObject>(second)->Text;
                });
                return MemberResult::Found;
            }
            if (name == "Join")
            {
                std::string separator;
                if (!Count(machine, label, given, 0, 1) || (given.size() == 1 && !Text(machine, label, given, 0, separator)))
                {
                    return MemberResult::Failed;
                }
                std::string joined;
                for (std::size_t index = 0; index < list->Items.size(); ++index)
                {
                    joined += (index > 0 ? separator : std::string()) + machine.Engine.Display(list->Items[index]);
                }
                result = TextValue(machine, joined);
                return MemberResult::Found;
            }
            return MemberResult::Missing;
        }

        // ---- Members of strings -----------------------------------------------------

        std::string Trimmed(const std::string& text)
        {
            std::size_t start = text.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
            {
                return {};
            }
            std::size_t end = text.find_last_not_of(" \t\r\n");
            return text.substr(start, end - start + 1);
        }

        MemberResult TextMember(Machine& machine, const std::string& name, std::span<Value> arguments, Value& result)
        {
            const std::string text = As<TextObject>(arguments[0])->Text;
            std::span<Value> given = arguments.subspan(1);
            std::string label = "string." + name;
            if (name == "Upper" || name == "Lower")
            {
                if (!Count(machine, label, given, 0, 0))
                {
                    return MemberResult::Failed;
                }
                std::string changed = text;
                for (char& character : changed)
                {
                    if (name == "Upper" && character >= 'a' && character <= 'z')
                    {
                        character = static_cast<char>(character - 'a' + 'A');
                    }
                    else if (name == "Lower" && character >= 'A' && character <= 'Z')
                    {
                        character = static_cast<char>(character - 'A' + 'a');
                    }
                }
                result = TextValue(machine, changed);
                return MemberResult::Found;
            }
            if (name == "Trim")
            {
                result = TextValue(machine, Trimmed(text));
                return Count(machine, label, given, 0, 0) ? MemberResult::Found : MemberResult::Failed;
            }
            if (name == "Contains" || name == "Find" || name == "StartsWith" || name == "EndsWith")
            {
                std::string part;
                if (!Count(machine, label, given, 1, 1) || !Text(machine, label, given, 0, part))
                {
                    return MemberResult::Failed;
                }
                if (name == "Contains")
                {
                    result = Value::Of(text.find(part) != std::string::npos);
                }
                else if (name == "StartsWith")
                {
                    result = Value::Of(text.starts_with(part));
                }
                else if (name == "EndsWith")
                {
                    result = Value::Of(text.ends_with(part));
                }
                else
                {
                    std::size_t found = text.find(part);
                    result = found == std::string::npos ? Value()
                                                        : Value::Of(static_cast<double>(CountCharacters(text.substr(0, found)) + 1));
                }
                return MemberResult::Found;
            }
            if (name == "Replace")
            {
                std::string from;
                std::string to;
                if (!Count(machine, label, given, 2, 2) || !Text(machine, label, given, 0, from) || !Text(machine, label, given, 1, to))
                {
                    return MemberResult::Failed;
                }
                if (from.empty())
                {
                    return machine.Fail("string.Replace needs something to replace") ? MemberResult::Found : MemberResult::Failed;
                }
                std::string changed;
                std::size_t start = 0;
                for (std::size_t found = text.find(from); found != std::string::npos; found = text.find(from, start))
                {
                    changed += text.substr(start, found - start) + to;
                    start = found + from.size();
                }
                changed += text.substr(start);
                result = TextValue(machine, changed);
                return MemberResult::Found;
            }
            if (name == "Split")
            {
                std::string separator;
                if (!Count(machine, label, given, 1, 1) || !Text(machine, label, given, 0, separator))
                {
                    return MemberResult::Failed;
                }
                if (separator.empty())
                {
                    return machine.Fail("string.Split needs a separator") ? MemberResult::Found : MemberResult::Failed;
                }
                ListObject* pieces = machine.Engine.TheHeap.Make<ListObject>();
                std::size_t start = 0;
                for (std::size_t found = text.find(separator); found != std::string::npos; found = text.find(separator, start))
                {
                    pieces->Items.push_back(TextValue(machine, text.substr(start, found - start)));
                    start = found + separator.size();
                }
                pieces->Items.push_back(TextValue(machine, text.substr(start)));
                machine.Engine.TheHeap.Grew(*pieces, pieces->Items.size() * sizeof(Value));
                result = Value::Of(static_cast<Object*>(pieces));
                return MemberResult::Found;
            }
            if (name == "Part")
            {
                long long first = 0;
                long long count = -1;
                if (!Count(machine, label, given, 1, 2) || !Whole(machine, label, given, 0, first) ||
                    (given.size() == 2 && !Whole(machine, label, given, 1, count)))
                {
                    return MemberResult::Failed;
                }
                std::size_t length = CountCharacters(text);
                if (first < 1 || static_cast<std::size_t>(first) > length + 1 || (given.size() == 2 && count < 0))
                {
                    return machine.Fail(std::format("string.Part starts at a character from 1 to {}, and counts 0 or more",
                               length + 1))
                               ? MemberResult::Found
                               : MemberResult::Failed;
                }
                std::size_t start = ByteOfCharacter(text, static_cast<std::size_t>(first - 1));
                std::size_t end = count < 0 ? text.size() : ByteOfCharacter(text, static_cast<std::size_t>(first - 1 + count));
                result = TextValue(machine, text.substr(start, end - start));
                return MemberResult::Found;
            }
            return MemberResult::Missing;
        }

        bool IsListMethod(std::string_view name)
        {
            return name == "Add" || name == "Insert" || name == "Remove" || name == "Contains" || name == "Find" || name == "Clear" ||
                   name == "Reverse" || name == "Copy" || name == "Sort" || name == "Join";
        }

        bool IsTextMethod(std::string_view name)
        {
            return name == "Upper" || name == "Lower" || name == "Trim" || name == "Contains" || name == "Find" ||
                   name == "StartsWith" || name == "EndsWith" || name == "Replace" || name == "Split" || name == "Part";
        }

        // A member function read without calling it, such as `constant add = items.Add`.
        Value Bound(Machine& machine, Value self, const std::string& name)
        {
            NativeObject* native = machine.Engine.TheHeap.Make<NativeObject>();
            native->Name = name;
            native->Bound = self;
            native->Builtin = [name](Machine& running, std::span<Value> arguments, Value& result) {
                TextObject* member = running.Engine.TheHeap.Text(name);
                return CallMember(running, member, arguments, result) == MemberResult::Found;
            };
            return Value::Of(static_cast<Object*>(native));
        }

        bool Index(Machine& machine, const ListObject& list, Value index, std::size_t extra, std::size_t& at)
        {
            if (!index.IsNumber() || index.Number != std::floor(index.Number))
            {
                return machine.Fail(std::format("a list's items are found by whole numbers from 1, but this is {}",
                    machine.Engine.Describe(index)));
            }
            double position = index.Number;
            if (position < 1 || position > static_cast<double>(list.Items.size() + extra))
            {
                return machine.Fail(std::format("position {} is outside the list, which has {} item{}", NumberText(position),
                    list.Items.size(), list.Items.size() == 1 ? "" : "s"));
            }
            at = static_cast<std::size_t>(position - 1);
            return true;
        }
    }

    std::string NumberText(double number)
    {
        if (std::isfinite(number) && number == std::floor(number) && std::abs(number) < 1e15)
        {
            return std::format("{}", static_cast<long long>(number));
        }
        if (std::isnan(number))
        {
            return "not a number";
        }
        if (std::isinf(number))
        {
            return number > 0 ? "infinity" : "-infinity";
        }
        return std::format("{}", number);
    }

    MemberResult CallMember(Machine& machine, TextObject* name, std::span<Value> arguments, Value& result)
    {
        Value self = arguments[0];
        if (IsKind(self, ObjectKind::List))
        {
            return ListMember(machine, name->Text, arguments, result);
        }
        if (IsKind(self, ObjectKind::Text))
        {
            return TextMember(machine, name->Text, arguments, result);
        }
        return MemberResult::Missing;
    }

    bool GetMember(Machine& machine, Value object, TextObject* name, Value& result)
    {
        EngineState& engine = machine.Engine;
        if (IsKind(object, ObjectKind::Table))
        {
            const Value* found = As<TableObject>(object)->Find(name);
            result = found ? *found : Value();
            return true;
        }
        if (IsKind(object, ObjectKind::Host))
        {
            result = engine.ToValue(As<HostObject>(object)->Held->Get(name->Text));
            return true;
        }
        if (IsKind(object, ObjectKind::List))
        {
            const std::vector<Value>& items = As<ListObject>(object)->Items;
            if (name->Text == "Count")
            {
                result = Value::Of(static_cast<double>(items.size()));
                return true;
            }
            if (name->Text == "First" || name->Text == "Last")
            {
                result = items.empty() ? Value() : name->Text == "First" ? items.front() : items.back();
                return true;
            }
            if (IsListMethod(name->Text))
            {
                result = Bound(machine, object, name->Text);
                return true;
            }
            return machine.Fail(std::format("a list has no {}; it has Count, First, Last, Add, Insert, Remove, Contains, Find, "
                                            "Clear, Reverse, Copy, Sort, and Join",
                name->Text));
        }
        if (IsKind(object, ObjectKind::Text))
        {
            if (name->Text == "Length")
            {
                result = Value::Of(static_cast<double>(CountCharacters(As<TextObject>(object)->Text)));
                return true;
            }
            if (IsTextMethod(name->Text))
            {
                result = Bound(machine, object, name->Text);
                return true;
            }
            return machine.Fail(std::format("a string has no {}; it has Length, Upper, Lower, Trim, Contains, Find, StartsWith, "
                                            "EndsWith, Replace, Split, and Part",
                name->Text));
        }
        if (object.IsNothing())
        {
            return machine.Fail(std::format("this is nothing, so it has no {}", name->Text));
        }
        return machine.Fail(std::format("{} has no {}", engine.Describe(object), name->Text));
    }

    bool SetMember(Machine& machine, Value object, TextObject* name, Value value)
    {
        EngineState& engine = machine.Engine;
        if (IsKind(object, ObjectKind::Table))
        {
            TableObject* table = As<TableObject>(object);
            bool added = table->Find(name) == nullptr;
            // A table made by a type keeps the type's fields.
            if (added && table->Record &&
                std::find(table->Record->Fields.begin(), table->Record->Fields.end(), name) == table->Record->Fields.end())
            {
                return machine.Fail(std::format("{} has no field {}", engine.Describe(object), name->Text));
            }
            table->Set(name, value);
            if (added)
            {
                engine.TheHeap.Grew(*table, sizeof(std::pair<TextObject*, Value>) * 2);
            }
            return true;
        }
        if (IsKind(object, ObjectKind::Host))
        {
            HostObject* host = As<HostObject>(object);
            if (!host->Held->Set(name->Text, engine.ToScript(value)))
            {
                return machine.Fail(std::format("{} cannot be changed on {}", name->Text, engine.Describe(object)));
            }
            return true;
        }
        if (object.IsNothing())
        {
            return machine.Fail(std::format("this is nothing, so its {} cannot be set", name->Text));
        }
        return machine.Fail(std::format("{} cannot be changed on {}", name->Text, engine.Describe(object)));
    }

    bool GetItem(Machine& machine, Value object, Value index, Value& result)
    {
        if (IsKind(object, ObjectKind::List))
        {
            const ListObject& list = *As<ListObject>(object);
            std::size_t at = 0;
            if (!Index(machine, list, index, 0, at))
            {
                return false;
            }
            result = list.Items[at];
            return true;
        }
        if (IsKind(object, ObjectKind::Table) || IsKind(object, ObjectKind::Host))
        {
            if (!IsKind(index, ObjectKind::Text))
            {
                return machine.Fail(std::format("a table's fields are found by name, but this is {}", machine.Engine.Describe(index)));
            }
            return GetMember(machine, object, As<TextObject>(index), result);
        }
        if (IsKind(object, ObjectKind::Text))
        {
            return machine.Fail("a string's characters are read with Part, such as name.Part(1, 1)");
        }
        if (object.IsNothing())
        {
            return machine.Fail("this is nothing, so it has no items");
        }
        return machine.Fail(std::format("{} has no items", machine.Engine.Describe(object)));
    }

    bool SetItem(Machine& machine, Value object, Value index, Value value)
    {
        if (IsKind(object, ObjectKind::List))
        {
            ListObject& list = *As<ListObject>(object);
            std::size_t at = 0;
            if (index.IsNumber() && index.Number == static_cast<double>(list.Items.size() + 1))
            {
                return machine.Fail(std::format("position {} is past the end of the list; add items with list.Add",
                    NumberText(index.Number)));
            }
            if (!Index(machine, list, index, 0, at))
            {
                return false;
            }
            list.Items[at] = value;
            return true;
        }
        if (IsKind(object, ObjectKind::Table) || IsKind(object, ObjectKind::Host))
        {
            if (!IsKind(index, ObjectKind::Text))
            {
                return machine.Fail(std::format("a table's fields are named, but this is {}", machine.Engine.Describe(index)));
            }
            return SetMember(machine, object, As<TextObject>(index), value);
        }
        if (object.IsNothing())
        {
            return machine.Fail("this is nothing, so it has no items to set");
        }
        return machine.Fail(std::format("{} has no items to set", machine.Engine.Describe(object)));
    }

    void InstallBuiltins(EngineState& engine)
    {
        Define(engine, "print", [](Machine& machine, std::span<Value> arguments, Value&) {
            std::string line;
            for (std::size_t index = 0; index < arguments.size(); ++index)
            {
                line += (index > 0 ? " " : "") + machine.Engine.Display(arguments[index]);
            }
            if (machine.Engine.Settings.Print)
            {
                machine.Engine.Settings.Print(line);
            }
            else
            {
                std::fwrite(line.data(), 1, line.size(), stdout);
                std::fputc('\n', stdout);
                std::fflush(stdout);
            }
            return true;
        });
        Define(engine, "Type", [](Machine& machine, std::span<Value> arguments, Value& result) {
            if (!Count(machine, "Type", arguments, 1, 1))
            {
                return false;
            }
            result = TextValue(machine, machine.Engine.TypeName(arguments[0]));
            return true;
        });
        Define(engine, "ToText", [](Machine& machine, std::span<Value> arguments, Value& result) {
            if (!Count(machine, "ToText", arguments, 1, 1))
            {
                return false;
            }
            result = TextValue(machine, machine.Engine.Display(arguments[0]));
            return true;
        });
        Define(engine, "ToNumber", [](Machine& machine, std::span<Value> arguments, Value& result) {
            std::string text;
            if (!Count(machine, "ToNumber", arguments, 1, 1))
            {
                return false;
            }
            if (arguments[0].IsNumber())
            {
                result = arguments[0];
                return true;
            }
            if (!Text(machine, "ToNumber", arguments, 0, text))
            {
                return false;
            }
            text = Trimmed(text);
            double number = 0.0;
            std::from_chars_result read = std::from_chars(text.data(), text.data() + text.size(), number);
            result = read.ec == std::errc() && read.ptr == text.data() + text.size() && !text.empty() ? Value::Of(number) : Value();
            return true;
        });
        Define(engine, "Fail", [](Machine& machine, std::span<Value> arguments, Value&) {
            if (!Count(machine, "Fail", arguments, 1, 1))
            {
                return false;
            }
            return machine.Fail(machine.Engine.Display(arguments[0]));
        });

        DefineMath(engine, "SquareRoot", [](double number) { return std::sqrt(number); });
        DefineMath(engine, "Absolute", [](double number) { return std::abs(number); });
        DefineMath(engine, "Floor", [](double number) { return std::floor(number); });
        DefineMath(engine, "Ceiling", [](double number) { return std::ceil(number); });
        DefineMath(engine, "Round", [](double number) { return std::round(number); });
        DefineMath(engine, "Sine", [](double number) { return std::sin(number); });
        DefineMath(engine, "Cosine", [](double number) { return std::cos(number); });
        DefineMath(engine, "Tangent", [](double number) { return std::tan(number); });
        DefineMath(engine, "Radians", [](double degrees) { return degrees * std::numbers::pi / 180.0; });
        DefineMath(engine, "Degrees", [](double radians) { return radians * 180.0 / std::numbers::pi; });
        engine.Globals->Set(engine.TheHeap.Text("Pi"), Value::Of(std::numbers::pi));

        for (std::string_view name : { "Min", "Max" })
        {
            bool smallest = name == "Min";
            std::string kept(name);
            Define(engine, name, [kept, smallest](Machine& machine, std::span<Value> arguments, Value& result) {
                if (arguments.empty())
                {
                    return machine.Fail(std::format("{} needs at least one number", kept));
                }
                double best = 0.0;
                for (std::size_t index = 0; index < arguments.size(); ++index)
                {
                    double number = 0.0;
                    if (!Number(machine, kept, arguments, index, number))
                    {
                        return false;
                    }
                    best = index == 0 ? number : smallest ? std::min(best, number) : std::max(best, number);
                }
                result = Value::Of(best);
                return true;
            });
        }
        Define(engine, "Clamp", [](Machine& machine, std::span<Value> arguments, Value& result) {
            double value = 0.0;
            double low = 0.0;
            double high = 0.0;
            if (!Count(machine, "Clamp", arguments, 3, 3) || !Number(machine, "Clamp", arguments, 0, value) ||
                !Number(machine, "Clamp", arguments, 1, low) || !Number(machine, "Clamp", arguments, 2, high))
            {
                return false;
            }
            result = Value::Of(std::min(std::max(value, low), high));
            return true;
        });

        auto generator = std::make_shared<std::mt19937_64>(std::random_device {}());
        Define(engine, "Random", [generator](Machine& machine, std::span<Value> arguments, Value& result) {
            if (!Count(machine, "Random", arguments, 0, 0))
            {
                return false;
            }
            result = Value::Of(std::uniform_real_distribution<double>(0.0, 1.0)(*generator));
            return true;
        });
        Define(engine, "RandomWhole", [generator](Machine& machine, std::span<Value> arguments, Value& result) {
            long long low = 0;
            long long high = 0;
            if (!Count(machine, "RandomWhole", arguments, 2, 2) || !Whole(machine, "RandomWhole", arguments, 0, low) ||
                !Whole(machine, "RandomWhole", arguments, 1, high))
            {
                return false;
            }
            if (high < low)
            {
                return machine.Fail("RandomWhole needs the smaller number first");
            }
            result = Value::Of(static_cast<double>(std::uniform_int_distribution<long long>(low, high)(*generator)));
            return true;
        });

        Define(engine, "FieldNames", [](Machine& machine, std::span<Value> arguments, Value& result) {
            TableObject* table = nullptr;
            if (!Count(machine, "FieldNames", arguments, 1, 1) || !Table(machine, "FieldNames", arguments, 0, table))
            {
                return false;
            }
            ListObject* names = machine.Engine.TheHeap.Make<ListObject>();
            for (const auto& [name, value] : table->Fields)
            {
                names->Items.push_back(Value::Of(static_cast<Object*>(name)));
            }
            machine.Engine.TheHeap.Grew(*names, names->Items.size() * sizeof(Value));
            result = Value::Of(static_cast<Object*>(names));
            return true;
        });
        Define(engine, "HasField", [](Machine& machine, std::span<Value> arguments, Value& result) {
            TableObject* table = nullptr;
            std::string name;
            if (!Count(machine, "HasField", arguments, 2, 2) || !Table(machine, "HasField", arguments, 0, table) ||
                !Text(machine, "HasField", arguments, 1, name))
            {
                return false;
            }
            result = Value::Of(table->Find(machine.Engine.TheHeap.Text(name)) != nullptr);
            return true;
        });
        Define(engine, "RemoveField", [](Machine& machine, std::span<Value> arguments, Value& result) {
            TableObject* table = nullptr;
            std::string name;
            if (!Count(machine, "RemoveField", arguments, 2, 2) || !Table(machine, "RemoveField", arguments, 0, table) ||
                !Text(machine, "RemoveField", arguments, 1, name))
            {
                return false;
            }
            result = Value::Of(table->Remove(machine.Engine.TheHeap.Text(name)));
            return true;
        });
    }
}
