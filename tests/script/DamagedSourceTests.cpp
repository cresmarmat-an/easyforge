#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <easyforge/core/Paths.h>
#include <easyforge/core/Testing.h>
#include <easyforge/core/language/Syntax.h>
#include <easyforge/core/language/Tokens.h>
#include <easyforge/script.h>

#include "Damage.h"

using namespace easyforge;
using namespace easyforge::testing;

// Damaged scripts are read, checked, and run. Whatever they say, the reader,
// the checker, the compiler, and the machine must not crash, hang, or read
// outside their data; a running script is held to an instruction and memory
// limit, so a loop that never ends stops with an error.

namespace
{
    constexpr const char* Everything = R"(-- every kind of statement, so damage reaches every part of the reader
--[[
    a comment over
    several lines
]]
variable count = 0
constant limit --[[ inside a line ]] = 10
constant names: list of string = ["Ari", "Bo", "Cy"]

type Point
    X: number
    Y: number
end

function Distance(first: Point, second: Point) returns number then
    return SquareRoot((second.X - first.X) ^ 2 + (second.Y - first.Y) ^ 2)
end

function SomeName(parameter) returns string then
    print("Output: {parameter} {{literal}}")
    return "Output: {parameter}"
end

function Blink(times: number) then
    for index in 1 to times then
        wait 0.1
    end
    yield
end

if count < limit and not (count == 3) then
    count = count + 1
else if count == limit or count > 100 then
    print("full")
else
    print("over")
end

while count < limit then
    count = count + 1
    if count == 7 then
        break
    end
    continue
end

for name in names then
    print(name)
end

for key in { X = 1, Y = 2 } then
    print(key)
end

try
    Distance(nothing, nothing)
catch problem then
    print("failed: {problem}")
end

spawn Blink(2)
SomeName("Hello")
)";

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }

    std::vector<std::string> Sources()
    {
        std::vector<std::string> sources { Everything };
        for (const auto& item : std::filesystem::directory_iterator(EASYFORGE_TEST_SCRIPTS))
        {
            if (item.path().extension() == ".script")
            {
                sources.push_back(ReadFile(item.path()));
            }
        }
        return sources;
    }
}

EASYFORGE_TEST(TheEverythingScriptRuns)
{
    ScriptEngine scripts = ScriptEngine::New({ .InstructionLimit = 1'000'000 });
    Result<ScriptValue> ran = scripts.Run(Everything, "everything.script");
    EASYFORGE_EXPECT(ran);
}

EASYFORGE_TEST(DamagedSourceNeverCrashesTheReader)
{
    std::uint64_t seed = 300;
    for (const std::string& source : Sources())
    {
        Damage(
            source,
            [](const std::string& damaged) {
                std::vector<language::Problem> problems;
                (void)language::Tokenize(damaged, problems);
                (void)language::Parse(damaged);
            },
            seed++, 400);
    }
}

EASYFORGE_TEST(DamagedScriptsNeverCrashTheEngine)
{
    std::uint64_t seed = 400;
    for (const std::string& source : Sources())
    {
        Damage(
            source,
            [](const std::string& damaged) {
                ScriptEngine scripts = ScriptEngine::New({ .MemoryLimit = 16 * 1024 * 1024, .InstructionLimit = 200'000 });
                (void)scripts.Check(damaged, "damaged.script");
                (void)scripts.Run(damaged, "damaged.script");
                // Tasks a damaged script started are carried on too.
                for (int update = 0; update < 3; ++update)
                {
                    (void)scripts.Update(0.1f);
                }
            },
            seed++, 300);
    }
}

EASYFORGE_TEST(ScriptsRunAndImportFromFoldersNamedInAnyLanguage)
{
    std::filesystem::path folder = PathFromText(std::string(EASYFORGE_TEST_OUTPUT) + "Café 文字 κείμενο");
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    {
        std::ofstream helper(folder / PathFromText("helper.script"), std::ios::binary);
        helper << "function Twice(amount) returns number then\n    return amount * 2\nend\n";
        std::ofstream main(folder / PathFromText("главный.script"), std::ios::binary);
        main << "import \"helper\"\nreturn helper.Twice(21)\n";
    }
    ScriptEngine scripts = ScriptEngine::New();
    Result<ScriptValue> ran = scripts.RunFile(TextFromPath(folder / PathFromText("главный.script")));
    if (!ran)
    {
        Log("{}", ran.Error());
    }
    EASYFORGE_REQUIRE(ran);
    EASYFORGE_EXPECT_EQUAL(ran->AsNumber(), 42.0);
}
