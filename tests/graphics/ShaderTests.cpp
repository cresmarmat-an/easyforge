#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>

#include <cmath>
#include <fstream>

using namespace easyforge;

namespace
{
    Renderer Offscreen(int width, int height, float scale = 1.0f)
    {
        return Renderer::New({ .Adapter = GraphicsAdapter::Software, .Width = width, .Height = height, .Scale = scale });
    }

    int Channel(const ImageData& image, int x, int y, int channel)
    {
        return image.Pixels[static_cast<std::size_t>(y) * image.Stride() + static_cast<std::size_t>(x) * 4 +
                            static_cast<std::size_t>(channel)];
    }

    // The shader's problems, which the test expects there to be.
    std::string ProblemsOf(std::string_view source)
    {
        Shader shader = Shader::FromText(source, "test.shader");
        if (shader)
        {
            return "no problems";
        }
        return shader.Error();
    }

    constexpr const char* Ripple = R"(-- ripple.shader
value Speed: number = 1
value Tint: color = #FFFFFF

function Pixel(input: PixelInput) returns color then
    constant wave = Sine(input.Position.X * 20 + input.Time * Speed)
    return Sample(input.Content, input.Coordinates + vector2(0, wave * 0.01)) * Tint
end
)";
}

EASYFORGE_TEST(ShaderFromThePlanCompiles)
{
    Shader ripple = Shader::FromText(Ripple, "ripple.shader");
    EASYFORGE_REQUIRE(ripple);
    EASYFORGE_EXPECT(ripple.Error().empty());
    EASYFORGE_EXPECT(ripple.ValueNames() == std::vector<std::string>({ "Speed", "Tint" }));
    EASYFORGE_EXPECT(ripple.GeneratedCode().find("UserPixel") != std::string::npos);

    // Helper functions, constants, loops, and branches.
    Shader rings = Shader::FromText(R"(
constant Rings = 4

function Band(distance: number) returns number then
    variable total = 0
    for index in 1 to Rings then
        if Fraction(distance * index) > 0.5 then
            total += 1
        else if index == 2 then
            continue
        end
    end
    return total / Rings
end

function Pixel(input: PixelInput) returns color then
    constant center = input.Size / 2
    constant level = Band(Distance(input.Position, center) / 50)
    variable result = color(level, level, level)
    result.Alpha = Clamp(level, 0.2, 1)
    return Lerp(result, #FF0000, 0.1)
end
)", "rings.shader");
    EASYFORGE_EXPECT(rings);
    EASYFORGE_EXPECT_EQUAL(rings.Error(), std::string());
}

EASYFORGE_TEST(ShaderProblemsSayWhere)
{
    EASYFORGE_EXPECT_EQUAL(ProblemsOf(R"(function Pixel(input: PixelInput) returns color then
    return color(wave, 0, 0)
end)"),
        std::string("test.shader:2:18: 'wave' is not declared"));

    std::string mismatch = ProblemsOf(R"(function Pixel(input: PixelInput) returns color then
    return input.Position
end)");
    EASYFORGE_EXPECT(mismatch.find("test.shader:2:12:") == 0);
    EASYFORGE_EXPECT(mismatch.find("returns a color, but this is a vector2") != std::string::npos);

    EASYFORGE_EXPECT(ProblemsOf("value Speed = 1").find("needs a Pixel function") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("function Pixel(input: PixelInput) returns number then\n    return 1\nend")
                         .find("must take one PixelInput and return a color") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("value Tint: color = Something()\nfunction Pixel(input: PixelInput) returns color then\n"
                                "    return Tint\nend")
                         .find("must be a color written out") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf(R"(function A(x: number) returns number then
    return B(x)
end
function B(x: number) returns number then
    return A(x)
end
function Pixel(input: PixelInput) returns color then
    return color(A(1), 0, 0)
end)")
                         .find("cannot call themselves") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("variable x = 1\nfunction Pixel(input: PixelInput) returns color then\n    return #FFFFFF\nend")
                         .find("cannot have variables outside") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("function Pixel(input: PixelInput) returns color then\n    variable t = \"text\"\n"
                                "    return #FFFFFF\nend")
                         .find("no text") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("function Pixel(input: PixelInput) returns color then\n    if input.Time then\n"
                                "        return #FFFFFF\n    end\n    return #000000\nend")
                         .find("must be true or false") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("function Pixel(input: PixelInput) returns color then\n    if input.Time > 1 then\n"
                                "        return #FFFFFF\n    end\nend")
                         .find("on every path") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("function Pixel(input: PixelInput) returns color then\n    return input.Position.Z\nend")
                         .find("has X and Y, not Z") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("function Pixel(input: PixelInput) returns color then\n    return color(1, 2)\nend")
                         .find("needs 4 numbers in all, but 2 were given") != std::string::npos);
    EASYFORGE_EXPECT(ProblemsOf("function Pixel(input) returns color then\n    return #FFFFFF\nend")
                         .find("needs a type") != std::string::npos);

    // Every problem is listed, not only the first.
    std::string many = ProblemsOf(R"(function Pixel(input: PixelInput) returns color then
    variable a = missing
    variable b = alsoMissing
    return #FFFFFF
end)");
    EASYFORGE_EXPECT(many.find("'missing'") != std::string::npos);
    EASYFORGE_EXPECT(many.find("'alsoMissing'") != std::string::npos);

    Shader missing = Shader::Load("no such.shader");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(!missing.Error().empty());
}

EASYFORGE_TEST(ShadedDrawsTheShader)
{
    Shader tint = Shader::FromText(R"(value Tint: color = #336699
function Pixel(input: PixelInput) returns color then
    return Tint
end)");
    EASYFORGE_REQUIRE(tint);
    Renderer renderer = Offscreen(40, 40);
    Canvas canvas = renderer.BeginFrame(Color::Black);
    canvas.Shaded(tint, { 0, 0, 20, 20 });
    canvas.Shaded(tint, { 20, 0, 20, 20 }, { { "Tint", Color::Hex("#FF8000") } });
    renderer.EndFrame();
    ImageData image = renderer.Capture();
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(10, 10).ToHex(), std::string("#336699"));
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(30, 10).ToHex(), std::string("#FF8000"));
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(10, 30).ToHex(), std::string("#000000"));
}

EASYFORGE_TEST(ShaderInputsAreInPoints)
{
    // Red rises across the area, green down it.
    Shader gradient = Shader::FromText(R"(function Pixel(input: PixelInput) returns color then
    return color(input.Coordinates.X, input.Position.Y / input.Size.Y, 0)
end)");
    EASYFORGE_REQUIRE(gradient);
    Renderer renderer = Offscreen(200, 100, 2.0f);
    Canvas canvas = renderer.BeginFrame(Color::Black);
    canvas.Shaded(gradient, { 0, 0, 100, 50 });
    renderer.EndFrame();
    ImageData image = renderer.Capture();
    // Pixel (50, 25) is at a quarter across and a quarter down; its center is half
    // a pixel further.
    EASYFORGE_EXPECT(std::abs(Channel(image, 50, 25, 0) - static_cast<int>(std::lround(50.5 / 200 * 255))) <= 1);
    EASYFORGE_EXPECT(std::abs(Channel(image, 50, 25, 1) - static_cast<int>(std::lround(25.5 / 100 * 255))) <= 1);
    EASYFORGE_EXPECT(Channel(image, 190, 10, 0) > 240);
}

EASYFORGE_TEST(LayersGoThroughShaders)
{
    Shader swap = Shader::FromText(R"(function Pixel(input: PixelInput) returns color then
    constant seen = Sample(input.Content, input.Coordinates)
    return color(seen.Blue, seen.Green, seen.Red, seen.Alpha)
end)");
    EASYFORGE_REQUIRE(swap);
    Renderer renderer = Offscreen(60, 30);
    Canvas canvas = renderer.BeginFrame(Color::Black);

    canvas.BeginLayer({ 0, 0, 30, 30 });
    canvas.Rectangle({ .Position = { 0, 0 }, .Size = { 30, 15 }, .Color = Color::Hex("#FF0000") });
    canvas.EndLayer({ .Shader = swap });

    // A layer without a shader, faded, and one inside another.
    canvas.BeginLayer({ 30, 0, 30, 30 });
    canvas.Rectangle({ .Position = { 30, 0 }, .Size = { 30, 30 }, .Color = Color::White });
    canvas.BeginLayer({ 40, 10, 10, 10 });
    canvas.Rectangle({ .Position = { 40, 10 }, .Size = { 10, 10 }, .Color = Color::Hex("#00FF00") });
    canvas.EndLayer();
    canvas.EndLayer({ .Opacity = 0.5f });
    renderer.EndFrame();
    ImageData image = renderer.Capture();

    EASYFORGE_EXPECT_EQUAL(image.ColorAt(10, 5).ToHex(), std::string("#0000FF"));
    // Where the layer was transparent, the shader's result is transparent too.
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(10, 25).ToHex(), std::string("#000000"));
    // Half of white over black, and half of green over black.
    EASYFORGE_EXPECT(std::abs(Channel(image, 32, 2, 0) - 128) <= 1);
    EASYFORGE_EXPECT(std::abs(Channel(image, 45, 15, 1) - 128) <= 1);
    EASYFORGE_EXPECT(Channel(image, 45, 15, 0) <= 1);
}

EASYFORGE_TEST(ShaderFromAFile)
{
    std::string path = EASYFORGE_TEST_OUTPUT "flat.shader";
    {
        std::ofstream file(path);
        file << "value Level: number = 0.5\nfunction Pixel(input: PixelInput) returns color then\n"
                "    return color(Level, Level, Level)\nend\n";
    }
    Shader flat = Shader::Load(path);
    EASYFORGE_REQUIRE(flat);
    Renderer renderer = Offscreen(8, 8);
    Canvas canvas = renderer.BeginFrame();
    canvas.Shaded(flat, { 0, 0, 8, 8 });
    renderer.EndFrame();
    EASYFORGE_EXPECT(std::abs(Channel(renderer.Capture(), 4, 4, 0) - 128) <= 1);
}

EASYFORGE_TEST(ShadersCanBeDroppedWhileTheGpuDraws)
{
    // The GPU may still be drawing with a shader when the program lets go of it.
    // The validation layer reports it as an error if the shader's GPU objects go
    // with it.
    Renderer renderer = Offscreen(16, 16);
    for (int frame = 0; frame < 4; ++frame)
    {
        Shader shade = Shader::FromText("function Pixel(input: PixelInput) returns color then\n"
                                        "    return color(input.Coordinates.X, 0, 0)\nend\n");
        EASYFORGE_REQUIRE(shade);
        Canvas canvas = renderer.BeginFrame();
        canvas.Shaded(shade, { 0, 0, 16, 16 });
        renderer.EndFrame();
    }
    EASYFORGE_EXPECT(Channel(renderer.Capture(), 15, 8, 0) > 200);
}
