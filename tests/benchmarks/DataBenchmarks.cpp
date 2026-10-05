#include "Benchmark.h"

#include <format>
#include <string>
#include <vector>

#include <easyforge/data.h>

using namespace easyforge;

EASYFORGE_BENCHMARK(Data)
{
    Table table;
    benchmarks::Measure(
        "add 10,000 nodes with 3 properties each", 20,
        [&] {
            for (int index = 0; index < 10'000; ++index)
            {
                table.Add(std::format("Unit{}", index), "Unit", { { "Health", 100 }, { "Name", "Ari" }, { "Speed", 2.5 } });
            }
        },
        [&] { table = Table::New(); });

    std::vector<Node> units = table.Nodes();
    int round = 0;
    benchmarks::Measure("set a property on each of 10,000 nodes", 50, [&] {
        ++round;
        for (const Node& unit : units)
        {
            unit["Health"] = round;
        }
    });

    std::uint64_t version = table.Version();
    benchmarks::Measure("read the last 10,000 changes", 50, [&] {
        std::vector<Change> changes = table.ChangesSince(version - 10'000);
        (void)changes;
    });

    std::string text;
    benchmarks::Measure("write 10,000 nodes as .tree text", 20, [&] { text = table.ToText(); });
    benchmarks::Measure("read 10,000 nodes from .tree text", 20, [&] {
        Table read = Table::FromText(text);
        (void)read;
    });
    benchmarks::Note(std::format("the text is {} kilobytes", text.size() / 1024));
}
