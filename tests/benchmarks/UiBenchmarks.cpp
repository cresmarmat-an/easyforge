#include "Benchmark.h"

#include <format>
#include <vector>

#include <easyforge/graphics.h>
#include <easyforge/ui.h>

using namespace easyforge;

namespace
{
    // 1,000 rows of a row, two labels, a checkbox, and a button: 5,000
    // elements in a scroll that shows about 30 rows at a time.
    ui::Scroll ManyRows(std::vector<ui::Label>& counts)
    {
        std::vector<ui::Element> rows;
        for (int index = 0; index < 1'000; ++index)
        {
            ui::Label count(std::format("{}", index), { .Width = 60 });
            counts.push_back(count);
            rows.push_back(ui::Row({ .Gap = 8, .Children = {
                ui::Label(std::format("Row {}", index), { .Width = ui::Fill }),
                count,
                ui::Checkbox("On"),
                ui::Button("Open"),
            } }));
        }
        return ui::Scroll({ .Width = ui::Fill, .Height = ui::Fill, .Padding = 12, .Gap = 4, .Children = rows });
    }
}

EASYFORGE_BENCHMARK(Ui)
{
    Renderer renderer = Renderer::New({ .Width = 1280, .Height = 1000 });
    if (!renderer)
    {
        benchmarks::Note("no renderer: " + renderer.Error());
        return;
    }
    ui::Theme theme = ui::Theme::Dark();
    theme.Font = Font::Load(EASYFORGE_BENCHMARK_FONT);

    auto frame = [&](ui::Root& root) {
        Canvas canvas = renderer.BeginFrame(theme.Background);
        root.Draw(canvas, 1.0f / 60.0f);
        renderer.EndFrame();
    };

    std::vector<ui::Label> counts;
    ui::Root root;
    benchmarks::Measure(
        "make 5,000 elements and lay them out in the first frame", 5,
        [&] {
            counts.clear();
            root = ui::Root::New({ .Content = ManyRows(counts), .Theme = theme });
            frame(root);
        },
        [&] { root = ui::Root(); });

    benchmarks::Measure("a frame of 5,000 elements where nothing changed", 120, [&] { frame(root); });

    int round = 0;
    benchmarks::Measure("a frame after changing one label's text", 120, [&] {
        counts[500].Text = std::format("{}", ++round);
        frame(root);
    });

    benchmarks::Measure("a frame after changing 1,000 labels' text", 30, [&] {
        ++round;
        for (ui::Label& count : counts)
        {
            count.Text = std::format("{}", round);
        }
        frame(root);
    });
}
