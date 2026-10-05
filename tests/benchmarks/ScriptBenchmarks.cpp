#include "Benchmark.h"

#include <easyforge/script.h>

using namespace easyforge;

namespace
{
    constexpr const char* Loop = R"(
variable total = 0
for index in 1 to 1000000 then
    total = total + index
end
return total
)";

    constexpr const char* Calls = R"(
function Add(first: number, second: number) returns number then
    return first + second
end

variable total = 0
for index in 1 to 100000 then
    total = Add(total, index)
end
return total
)";

    constexpr const char* Lists = R"(
variable values: list of number = []
for index in 1 to 100000 then
    values.Add(index)
end
variable total = 0
for item in values then
    total = total + item
end
return total
)";

    constexpr const char* Texts = R"(
variable text = ""
for index in 1 to 10000 then
    text = "{text}{index},"
end
return text.Length
)";

    constexpr const char* Recursion = R"(
function Fibonacci(count: number) returns number then
    if count < 2 then
        return count
    end
    return Fibonacci(count - 1) + Fibonacci(count - 2)
end
return Fibonacci(25)
)";

    constexpr const char* Twice = R"(
function Twice(amount: number) returns number then
    return amount * 2
end
)";
}

EASYFORGE_BENCHMARK(Script)
{
    ScriptEngine scripts = ScriptEngine::New();
    auto measure = [&](const char* name, const char* source, int runs) {
        Result<ScriptValue> first = scripts.Run(source, "benchmark.script");
        if (!first)
        {
            benchmarks::Note(std::string(name) + " did not run: " + first.Error());
            return;
        }
        benchmarks::Measure(name, runs, [&] { (void)scripts.Run(source, "benchmark.script"); });
    };
    measure("a loop of 1,000,000 additions", Loop, 10);
    measure("100,000 calls to a script function", Calls, 10);
    measure("a list of 100,000 numbers, filled and summed", Lists, 10);
    measure("10,000 pieces of text joined one by one", Texts, 10);
    measure("Fibonacci(25), 242,785 recursive calls", Recursion, 10);

    (void)scripts.Run(Twice, "twice.script");
    benchmarks::Measure("10,000 calls into a script from C++", 10, [&] {
        for (int index = 0; index < 10'000; ++index)
        {
            (void)scripts.Call("Twice", index);
        }
    });
}
