#include <string>
#include <vector>

#include <easyforge/core/Testing.h>
#include <easyforge/script.h>

using namespace easyforge;

namespace
{
    // An engine whose print writes into a list of lines.
    struct Printing
    {
        Printing()
        {
            Engine = ScriptEngine::New({ .Print = [this](std::string_view line) { Lines.emplace_back(line); } });
        }

        Result<ScriptValue> Run(std::string_view source) { return Engine.Run(source, "test.script"); }

        // Runs source and returns what it printed, a line each, or the error.
        std::string Printed(std::string_view source)
        {
            Lines.clear();
            Result<ScriptValue> ran = Run(source);
            if (!ran)
            {
                return "error: " + ran.Error();
            }
            return Joined(Lines);
        }

        static std::string Joined(const std::vector<std::string>& lines)
        {
            std::string text;
            for (const std::string& line : lines)
            {
                text += (text.empty() ? "" : "\n") + line;
            }
            return text;
        }

        ScriptEngine Engine;
        std::vector<std::string> Lines;
    };

    std::string Expected(const std::vector<std::string>& lines)
    {
        return Printing::Joined(lines);
    }
}

EASYFORGE_TEST(SomeNamePrintsItsOutput)
{
    Printing scripts;
    Result<ScriptValue> ran = scripts.Run(R"(
function SomeName(parameter) returns string then
    print("Output: {parameter}")
    return "Output: {parameter}"
end

SomeName("Hello")
)");
    EASYFORGE_REQUIRE(ran);
    EASYFORGE_EXPECT_EQUAL(Printing::Joined(scripts.Lines), Expected({ "Output: Hello" }));

    Result<ScriptValue> called = scripts.Engine.Call("SomeName", "again");
    EASYFORGE_REQUIRE(called);
    EASYFORGE_EXPECT_EQUAL(called->AsText(), std::string("Output: again"));
}

EASYFORGE_TEST(ValuesAndArithmetic)
{
    Printing scripts;
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
print(1 + 2 * 3, 7 / 2, 7 % 3, -7 % 3, 2 ^ 3 ^ 2, -2 ^ 2)
print("a" + "b", "count: {1 + 1}", "{{braces}}")
print(true and false, true or false, not true, nothing)
print(1 < 2, "apple" < "banana", 3 == 3, "x" != "x")
print([1, "two", [3]], { X = 1, Name = "a" })
print(#FF8000.Red, #FF8000.Green > 0.49 and #FF8000.Green < 0.51)
)"),
        Expected({ "7 3.5 1 2 512 -4", "ab count: 2 {braces}", "false true false nothing", "true true true false",
            "[1, \"two\", [3]] { X = 1, Name = \"a\" }", "1 true" }));
}

EASYFORGE_TEST(ControlFlow)
{
    Printing scripts;
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
variable count = 0
constant limit = 3
while count < limit then
    count += 1
end
if count == 0 then
    print("none")
else if count == 3 then
    print("three")
else
    print("other")
end

variable total = 0
for index in 1 to 10 then
    if index % 2 == 0 then
        continue
    end
    if index > 7 then
        break
    end
    total = total + index
end
print(total)

variable letters = ""
for letter in "héllo" then
    letters += letter + "."
end
print(letters)

for name in { First = 1, Second = 2 } then
    print(name)
end
)"),
        Expected({ "three", "16", "h.é.l.l.o.", "First", "Second" }));
}

EASYFORGE_TEST(FunctionsAndClosures)
{
    Printing scripts;
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
function Counter() returns function then
    variable count = 0
    return function() then
        count += 1
        return count
    end
end

constant first = Counter()
constant second = Counter()
first()
first()
print(first(), second())

function Factorial(number: number) returns number then
    if number <= 1 then
        return 1
    end
    return number * Factorial(number - 1)
end
print(Factorial(10))

function Outer() then
    function Inner(amount) then
        if amount > 0 then
            return Inner(amount - 1) + 1
        end
        return 0
    end
    print(Inner(4))
end
Outer()

variable adders = []
for index in 1 to 3 then
    adders.Add(function(amount) then
        return amount + index
    end)
end
print(adders[1](10), adders[3](10))
)"),
        Expected({ "3 1", "3628800", "4", "11 13" }));
}

EASYFORGE_TEST(ListsStringsAndTables)
{
    Printing scripts;
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
variable items = [3, 1, 2]
items.Add(5)
items.Sort()
print(items, items.Count, items.First, items.Last, items[2])
items.Insert(1, 0)
print(items.Remove(2), items, items.Contains(5), items.Find(5), items.Join("-"))

constant name = "  Hello World  "
print(name.Trim().Upper(), name.Trim().Length, name.Contains("World"), name.Trim().Find("World"))
print("a,b,c".Split(","), "abc".Replace("b", "x"), "héllo".Part(2, 3), "héllo".Part(3))

variable point = { X = 1 }
point.Y = 2
point["Z"] = 3
print(point, FieldNames(point), HasField(point, "Y"), point.Missing)
RemoveField(point, "X")
print(point)
)"),
        Expected({ "[1, 2, 3, 5] 4 1 5 2", "1 [0, 2, 3, 5] true 4 0-2-3-5", "HELLO WORLD 11 true 7",
            "[\"a\", \"b\", \"c\"] axc éll llo", "{ X = 1, Y = 2, Z = 3 } [\"X\", \"Y\", \"Z\"] true nothing",
            "{ Y = 2, Z = 3 }" }));
}

EASYFORGE_TEST(TypesAndRecords)
{
    Printing scripts;
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
type Point
    X: number
    Y: number
end

function Distance(first: Point, second: Point) returns number then
    return SquareRoot((second.X - first.X) ^ 2 + (second.Y - first.Y) ^ 2)
end

constant start = Point(0, 0)
constant finish = Point(3, 4)
print(Distance(start, finish), start, Type(start), Type(3), Type("x"), Type([]))
)"),
        Expected({ "5 Point { X = 0, Y = 0 } Point number string list" }));

    // What is written is checked before anything runs.
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
print("this does not run")
variable count: number = "three"
)"),
        Expected({ "error: test.script:3:26: count is a number, but this is a string" }));
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
function Half(amount: number) returns number then
    return amount / 2
end
Half("ten")
)"),
        Expected({ "error: test.script:5:6: Half's first argument should be a number, but this is a string" }));
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
constant fixed = 1
fixed = 2
)"),
        Expected({ "error: test.script:3:1: fixed is a constant and cannot be changed" }));

    // A record keeps its type's fields, also where the checker cannot see the type.
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
type Size
    Width: number
end
constant size: any = Size(2)
size.Width = 3
size.Height = 4
)"),
        Expected({ "error: test.script:7:1: a Size has no field Height" }));
}

EASYFORGE_TEST(ErrorsSayWhereAndCanBeCaught)
{
    Printing scripts;
    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
function Risky(amount) then
    if amount > 1 then
        Fail("too big: {amount}")
    end
    return amount
end

try
    Risky(5)
    print("not reached")
catch problem then
    print("failed: {problem}")
end

try
    variable list = [1]
    print(list[4])
catch problem then
    print(problem)
end
print("carried on")
)"),
        Expected({ "failed: too big: 5", "position 4 is outside the list, which has 1 item", "carried on" }));

    EASYFORGE_EXPECT_EQUAL(scripts.Printed("print(1 + \"one\")\n"),
        Expected({ "error: test.script:1:7: a string and a number cannot be added; to join them, write \"{first}{second}\"" }));
    EASYFORGE_EXPECT_EQUAL(scripts.Printed("variable missing = nothing\nmissing.Health = 3\n"),
        Expected({ "error: test.script:2:1: this is nothing, so its Health cannot be set" }));
    EASYFORGE_EXPECT_EQUAL(scripts.Printed("NoSuchThing()\n"), Expected({ "error: test.script:1:1: there is no NoSuchThing" }));
}

EASYFORGE_TEST(ProgramFunctionsAndObjects)
{
    Printing scripts;
    scripts.Engine.Define("Shout", [](std::string text) { return text + "!"; });
    scripts.Engine.Define("Add", [](double first, double second) { return first + second; });
    scripts.Engine.Define("Sum", [](const std::vector<ScriptValue>& values) {
        double total = 0.0;
        for (const ScriptValue& value : values)
        {
            total += value.AsNumber();
        }
        return total;
    });
    scripts.Engine.Define("Score", 10);

    class Player : public ScriptObject
    {
    public:
        std::string TypeName() const override { return "Player"; }
        ScriptValue Get(std::string_view name) override
        {
            if (name == "Health")
            {
                return Health;
            }
            if (name == "Heal")
            {
                return ScriptValue::Function("Heal", [this](int amount) { Health += amount; });
            }
            return {};
        }
        bool Set(std::string_view name, const ScriptValue& value) override
        {
            if (name != "Health")
            {
                return false;
            }
            Health = value.As<int>();
            return true;
        }
        int Health = 100;
    };
    auto player = std::make_shared<Player>();
    scripts.Engine.Define("player", player);

    EASYFORGE_EXPECT_EQUAL(scripts.Printed(R"(
print(Shout("hey"), Add(2, 3), Sum(1, 2, 3, 4), Score)
player.Health -= 30
player.Heal(5)
print(player.Health, Type(player))
Score = 20
)"),
        Expected({ "hey! 5 10 10", "75 Player" }));
    EASYFORGE_EXPECT_EQUAL(player->Health, 75);
    EASYFORGE_EXPECT_EQUAL(scripts.Engine.Get("Score").As<int>(), 20);

    EASYFORGE_EXPECT_EQUAL(scripts.Printed("Shout(1)\n"),
        Expected({ "error: test.script:1:1: Shout's first argument should be a string, but it is a number" }));
    EASYFORGE_EXPECT_EQUAL(scripts.Printed("player.Mana = 1\n"),
        Expected({ "error: test.script:1:1: Mana cannot be changed on a Player" }));

    // Values come back to the program, and functions can be called from it.
    Result<ScriptValue> made = scripts.Run(R"(
function Greet(name) then
    return "hello {name}"
end
return { Items = [1, 2], Greeter = Greet }
)");
    EASYFORGE_REQUIRE(made);
    EASYFORGE_EXPECT_EQUAL(made->Field("Items").Count(), std::size_t(2));
    Result<ScriptValue> greeting = made->Field("Greeter").Call({ "Ari" });
    EASYFORGE_REQUIRE(greeting);
    EASYFORGE_EXPECT_EQUAL(greeting->AsText(), std::string("hello Ari"));
}

EASYFORGE_TEST(SpawnedFunctionsWaitAcrossUpdates)
{
    Printing scripts;
    Result<ScriptValue> ran = scripts.Run(R"(
function Blink(times) then
    for index in 1 to times then
        print("on {index}")
        wait 0.5
    end
    print("done")
end

function EveryFrame() then
    for index in 1 to 3 then
        print("frame {index}")
        yield
    end
end

spawn Blink(2)
spawn EveryFrame()
print("started")
)");
    EASYFORGE_REQUIRE(ran);
    EASYFORGE_EXPECT_EQUAL(Printing::Joined(scripts.Lines), Expected({ "on 1", "frame 1", "started" }));
    EASYFORGE_EXPECT_EQUAL(scripts.Engine.RunningTasks(), std::size_t(2));

    scripts.Lines.clear();
    EASYFORGE_REQUIRE(scripts.Engine.Update(0.25f));
    EASYFORGE_EXPECT_EQUAL(Printing::Joined(scripts.Lines), Expected({ "frame 2" }));
    scripts.Lines.clear();
    EASYFORGE_REQUIRE(scripts.Engine.Update(0.3f));
    EASYFORGE_EXPECT_EQUAL(Printing::Joined(scripts.Lines), Expected({ "on 2", "frame 3" }));
    scripts.Lines.clear();
    EASYFORGE_REQUIRE(scripts.Engine.Update(0.5f));
    EASYFORGE_EXPECT_EQUAL(Printing::Joined(scripts.Lines), Expected({ "done" }));
    EASYFORGE_EXPECT_EQUAL(scripts.Engine.RunningTasks(), std::size_t(0));

    EASYFORGE_EXPECT_EQUAL(scripts.Printed("wait 1\n"),
        Expected({ "error: test.script:1:1: wait and yield only work in a function started with spawn, such as spawn Blink()" }));
}

EASYFORGE_TEST(LimitsStopRunawayScripts)
{
    ScriptEngine counted = ScriptEngine::New({ .InstructionLimit = 10000 });
    Result<ScriptValue> looped = counted.Run("while true then\nend\n", "loop.script");
    EASYFORGE_EXPECT(!looped);
    EASYFORGE_EXPECT(looped.Error().find("ran more than 10000 instructions") != std::string::npos);

    // A try cannot catch running out.
    Result<ScriptValue> caught = counted.Run("try\n    while true then\n    end\ncatch problem then\nend\n", "loop.script");
    EASYFORGE_EXPECT(!caught);

    // The engine works again for the next call.
    EASYFORGE_EXPECT(counted.Run("variable fine = 1\n", "fine.script"));

    // Deep calls between script functions work, and endless ones stop.
    ScriptEngine calls = ScriptEngine::New();
    Result<ScriptValue> deep = calls.Run(R"(
function Depth(count) then
    if count == 0 then
        return 0
    end
    return 1 + Depth(count - 1)
end
return Depth(900)
)",
        "deep.script");
    EASYFORGE_REQUIRE(deep);
    EASYFORGE_EXPECT_EQUAL(deep->AsNumber(), 900.0);
    Result<ScriptValue> endless = calls.Run("function Forever() then\n    Forever()\nend\nForever()\n", "endless.script");
    EASYFORGE_EXPECT(!endless);
    EASYFORGE_EXPECT(endless.Error().find("more than 1000 deep") != std::string::npos);

    // A script and the program calling each other without end stop too.
    calls.Define("Again", [&calls](ScriptValue function) { return function.Call({ function }); });
    Result<ScriptValue> nested = calls.Run("function Loop(self) then\n    return Again(self)\nend\nLoop(Loop)\n", "nested.script");
    EASYFORGE_EXPECT(!nested);
    EASYFORGE_EXPECT(nested.Error().find("call each other more than 32 deep") != std::string::npos);

    ScriptEngine small = ScriptEngine::New({ .MemoryLimit = 2 * 1024 * 1024 });
    Result<ScriptValue> grown = small.Run(R"(
variable kept = []
while true then
    kept.Add("{kept.Count} some text that takes room")
end
)",
        "grow.script");
    EASYFORGE_EXPECT(!grown);
    EASYFORGE_EXPECT(grown.Error().find("need more than") != std::string::npos);

    // Garbage is collected: a loop making lists it lets go of keeps running.
    ScriptEngine collected = ScriptEngine::New({ .MemoryLimit = 4 * 1024 * 1024 });
    EASYFORGE_EXPECT(collected.Run(R"(
for index in 1 to 200000 then
    variable thrown = [index, "{index}"]
end
)",
        "garbage.script"));
}

EASYFORGE_TEST(CheckingReportsEveryProblem)
{
    ScriptEngine scripts = ScriptEngine::New();
    std::vector<std::string> problems = scripts.Check(R"(
variable count: number = 1
count = "two"
Undefined()
function Take(amount: string) then
end
Take(3)
)",
        "check.script");
    EASYFORGE_EXPECT_EQUAL(Printing::Joined(problems),
        Expected({ "check.script:3:9: count is a number, but this is a string",
            "check.script:4:1: there is no Undefined",
            "check.script:7:6: Take's first argument should be a string, but this is a number" }));
    EASYFORGE_EXPECT(scripts.Check("print(\"fine\")\n").empty());
}

EASYFORGE_TEST(ScriptsImportOthers)
{
    Printing scripts;
    Result<ScriptValue> ran = scripts.Engine.RunFile(EASYFORGE_TEST_SCRIPTS "main.script");
    EASYFORGE_REQUIRE(ran);
    EASYFORGE_EXPECT_EQUAL(ran->AsText(), std::string("a troll"));

    // A module runs once, however often it is imported.
    EASYFORGE_REQUIRE(scripts.Engine.RunFile(EASYFORGE_TEST_SCRIPTS "main.script"));
    EASYFORGE_EXPECT_EQUAL(Printing::Joined(scripts.Lines), std::string("enemies loaded"));

    Result<ScriptValue> circular = scripts.Engine.RunFile(EASYFORGE_TEST_SCRIPTS "first.script");
    EASYFORGE_EXPECT(!circular);
    EASYFORGE_EXPECT(circular.Error().find("import each other") != std::string::npos);

    ScriptEngine searching = ScriptEngine::New({ .SearchFolders = { EASYFORGE_TEST_SCRIPTS }, .Print = [](std::string_view) {} });
    Result<ScriptValue> found = searching.Run("import \"enemies\"\nreturn enemies.Kinds.Count\n", "inline.script");
    EASYFORGE_REQUIRE(found);
    EASYFORGE_EXPECT_EQUAL(found->As<int>(), 2);

    Result<ScriptValue> missing = searching.Run("import \"nowhere\"\n", "inline.script");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(missing.Error().find("there is no nowhere.script to import") != std::string::npos);

    ScriptEngine closed = ScriptEngine::New({ .AllowImports = false });
    Result<ScriptValue> refused = closed.RunFile(EASYFORGE_TEST_SCRIPTS "main.script");
    EASYFORGE_EXPECT(!refused);
    EASYFORGE_EXPECT(refused.Error().find("does not let scripts import") != std::string::npos);
}