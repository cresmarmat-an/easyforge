#include "Benchmark.h"

#include <format>
#include <string>
#include <vector>

#include <easyforge/core/Random.h>
#include <easyforge/graphics.h>

using namespace easyforge;

EASYFORGE_BENCHMARK(Canvas)
{
    Renderer renderer = Renderer::New({ .Width = 1920, .Height = 1080 });
    if (!renderer)
    {
        benchmarks::Note("no renderer: " + renderer.Error());
        return;
    }
    benchmarks::Note(renderer.Description());

    Random random(7);
    struct Shape
    {
        Vector2 Position;
        Vector2 Size;
        Color Tint;
    };
    std::vector<Shape> shapes(50'000);
    for (Shape& shape : shapes)
    {
        shape.Position = { random.Between(0.0f, 1900.0f), random.Between(0.0f, 1060.0f) };
        shape.Size = { random.Between(4.0f, 40.0f), random.Between(4.0f, 40.0f) };
        shape.Tint = Color { random.Between(0.2f, 1.0f), random.Between(0.2f, 1.0f), random.Between(0.2f, 1.0f), 0.8f };
    }

    benchmarks::Measure("50,000 shapes: 20,000 rectangles, 20,000 circles, 10,000 lines", 60, [&] {
        Canvas canvas = renderer.BeginFrame(Color::Black);
        for (std::size_t index = 0; index < 20'000; ++index)
        {
            const Shape& shape = shapes[index];
            canvas.Rectangle({ .Position = shape.Position, .Size = shape.Size, .Color = shape.Tint });
        }
        for (std::size_t index = 20'000; index < 40'000; ++index)
        {
            const Shape& shape = shapes[index];
            canvas.Circle(shape.Position, shape.Size.X * 0.5f, { .Color = shape.Tint });
        }
        for (std::size_t index = 40'000; index < 50'000; ++index)
        {
            const Shape& shape = shapes[index];
            canvas.Line(shape.Position, shape.Position + shape.Size, { .Color = shape.Tint, .Width = 2.0f });
        }
        renderer.EndFrame();
    });

    benchmarks::Measure("5,000 rectangles with rounded corners and borders", 60, [&] {
        Canvas canvas = renderer.BeginFrame(Color::Black);
        for (std::size_t index = 0; index < 5'000; ++index)
        {
            const Shape& shape = shapes[index];
            canvas.Rectangle({ .Position = shape.Position, .Size = shape.Size, .Color = shape.Tint, .CornerRadius = 6.0f,
                .BorderWidth = 1.0f, .BorderColor = Color::White });
        }
        renderer.EndFrame();
    });

    Font font = Font::Load(EASYFORGE_BENCHMARK_FONT);
    if (!font)
    {
        benchmarks::Note("no font: " + font.Error());
        return;
    }
    std::vector<std::string> lines;
    for (int index = 0; index < 2'000; ++index)
    {
        lines.push_back(std::format("Line {} of the text, with some words", index));
    }
    benchmarks::Measure("2,000 lines of text", 60, [&] {
        Canvas canvas = renderer.BeginFrame(Color::Black);
        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            canvas.Text(font, lines[index], { .Position = shapes[index].Position, .Size = 14.0f });
        }
        renderer.EndFrame();
    });
}
