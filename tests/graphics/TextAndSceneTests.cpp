#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>

#include <cmath>

using namespace easyforge;

namespace
{
    Renderer Offscreen(int width, int height)
    {
        return Renderer::New({ .Adapter = GraphicsAdapter::Software, .Width = width, .Height = height });
    }

    int Red(const ImageData& image, int x, int y)
    {
        return image.Pixels[static_cast<std::size_t>(y) * image.Stride() + static_cast<std::size_t>(x) * 4];
    }

    // A cube two units wide around the origin, each face a quad wound
    // counterclockwise seen from outside.
    constexpr const char* CubeObject = R"(
v -1 -1 -1
v 1 -1 -1
v 1 1 -1
v -1 1 -1
v -1 -1 1
v 1 -1 1
v 1 1 1
v -1 1 1
f 5 6 7 8
f 2 1 4 3
f 1 5 8 4
f 6 2 3 7
f 8 7 3 4
f 1 2 6 5
)";
}

EASYFORGE_TEST(TextMatchesTheRasterizedGlyphs)
{
    Font font = Font::Load(EASYFORGE_TEST_FONT);
    EASYFORGE_REQUIRE(font);

    Renderer renderer = Offscreen(120, 60);
    Canvas canvas = renderer.BeginFrame(Color::Black);
    canvas.Text(font, "AO", { .Position = { 4, 6 }, .Size = 32, .Color = Color::White });
    renderer.EndFrame();
    ImageData image = renderer.Capture();

    // White text on black shows each glyph's coverage exactly, where the glyph
    // belongs: on the baseline, one ascender below the top of the text.
    const FontData& data = font.Data();
    FontMetrics metrics = data.Metrics();
    float scale = 32.0f / metrics.UnitsPerEm;
    int glyph = data.GlyphIndex(U'A');
    GlyphBitmap bitmap = data.Rasterize(glyph, 32.0f);
    EASYFORGE_REQUIRE(bitmap.Width > 0);
    int left = 4 + bitmap.Left;
    int top = static_cast<int>(std::round(6.0f + metrics.Ascender * scale)) - bitmap.Top;
    int differences = 0;
    for (int y = 0; y < bitmap.Height; ++y)
    {
        for (int x = 0; x < bitmap.Width; ++x)
        {
            int expected = bitmap.Coverage[static_cast<std::size_t>(y * bitmap.Width + x)];
            differences += std::abs(Red(image, left + x, top + y) - expected) > 1 ? 1 : 0;
        }
    }
    EASYFORGE_EXPECT_EQUAL(differences, 0);

    // Nothing is drawn far from the text.
    EASYFORGE_EXPECT_EQUAL(Red(image, 110, 50), 0);
    EASYFORGE_EXPECT_EQUAL(Red(image, 1, 1), 0);
}

EASYFORGE_TEST(TextMeasures)
{
    Font font = Font::Load(EASYFORGE_TEST_FONT);
    EASYFORGE_REQUIRE(font);
    FontMetrics metrics = font.Data().Metrics();
    float line = metrics.LineHeight() / metrics.UnitsPerEm * 20.0f;
    EASYFORGE_EXPECT_NEAR(font.LineHeight(20), line, 0.001f);

    Vector2 one = font.Measure("AO", 20);
    float advances = (font.Data().GlyphMetricsOf(font.Data().GlyphIndex(U'A')).Advance +
                         font.Data().GlyphMetricsOf(font.Data().GlyphIndex(U'O')).Advance +
                         font.Data().Kerning(font.Data().GlyphIndex(U'A'), font.Data().GlyphIndex(U'O'))) /
                     metrics.UnitsPerEm * 20.0f;
    EASYFORGE_EXPECT_NEAR(one.X, advances, 0.001f);
    EASYFORGE_EXPECT_NEAR(one.Y, line, 0.001f);

    Vector2 two = font.Measure("AO\nA", 20);
    EASYFORGE_EXPECT_NEAR(two.X, one.X, 0.001f);
    EASYFORGE_EXPECT_NEAR(two.Y, line * 2.0f, 0.001f);

    // Twice the size, twice the width.
    EASYFORGE_EXPECT_NEAR(font.Measure("AO", 40).X, one.X * 2.0f, 0.001f);

    Font missing = Font::Load("no such font.ttf");
    EASYFORGE_EXPECT(!missing);
    EASYFORGE_EXPECT(!missing.Error().empty());
    EASYFORGE_EXPECT_EQUAL(missing.Measure("AO", 20), Vector2());
}

EASYFORGE_TEST(SceneDrawsALitModel)
{
    Model cube = Model::FromData(ModelData::DecodeObj(CubeObject));
    EASYFORGE_REQUIRE(cube);
    Scene scene = Scene::New();
    SceneObject object = scene.Add(cube);
    EASYFORGE_EXPECT(object);
    EASYFORGE_EXPECT_EQUAL(scene.ObjectCount(), std::size_t { 1 });
    scene.Camera = { .Position = { 0, 0, 5 }, .Target = { 0, 0, 0 } };
    scene.Background = Color::Hex("#102030");
    scene.Ambient = Color::Hex("#202020");

    Renderer renderer = Offscreen(64, 64);
    auto draw = [&] {
        Canvas canvas = renderer.BeginFrame(Color::Black);
        canvas.Draw(scene);
        renderer.EndFrame();
        return renderer.Capture();
    };

    // The sun shines from behind the camera onto the face toward it.
    scene.Sun = { .Direction = { 0, 0, -1 } };
    ImageData lit = draw();
    EASYFORGE_EXPECT_EQUAL(lit.ColorAt(1, 1).ToHex(), std::string("#102030"));
    int litFace = Red(lit, 32, 32);
    EASYFORGE_EXPECT(litFace > 200);

    // From behind the cube, the same face only gets the ambient light.
    scene.Sun = { .Direction = { 0, 0, 1 } };
    ImageData dark = draw();
    int darkFace = Red(dark, 32, 32);
    EASYFORGE_EXPECT(darkFace < 100);
    EASYFORGE_EXPECT(darkFace > 10);

    // Moving the object changes where it is drawn.
    object.Position = { 10, 0, 0 };
    ImageData moved = draw();
    EASYFORGE_EXPECT_EQUAL(moved.ColorAt(32, 32).ToHex(), std::string("#102030"));

    object.Position = { 0, 0, 0 };
    object.Visible = false;
    EASYFORGE_EXPECT_EQUAL(draw().ColorAt(32, 32).ToHex(), std::string("#102030"));

    scene.Remove(object);
    EASYFORGE_EXPECT(!object);
    EASYFORGE_EXPECT_EQUAL(scene.ObjectCount(), std::size_t { 0 });
}

EASYFORGE_TEST(SceneInPartOfTheCanvas)
{
    Model cube = Model::FromData(ModelData::DecodeObj(CubeObject));
    Scene scene = Scene::New();
    scene.Add(cube, { .Scale = { 0.5f, 0.5f, 0.5f } });
    scene.Background = Color::Hex("#0000FF");
    scene.Camera = { .Position = { 0, 0, 5 } };

    Renderer renderer = Offscreen(64, 64);
    Canvas canvas = renderer.BeginFrame(Color::Hex("#FF0000"));
    canvas.Draw(scene, { 32, 0, 32, 32 });
    canvas.Rectangle({ .Position = { 40, 8 }, .Size = { 4, 4 }, .Color = Color::White });
    renderer.EndFrame();
    ImageData image = renderer.Capture();

    EASYFORGE_EXPECT_EQUAL(image.ColorAt(10, 10).ToHex(), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(33, 1).ToHex(), std::string("#0000FF"));
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(10, 40).ToHex(), std::string("#FF0000"));
    // Shapes drawn after the scene cover it.
    EASYFORGE_EXPECT_EQUAL(image.ColorAt(41, 9).ToHex(), std::string("#FFFFFF"));
    // The cube is in the middle of the scene's area.
    EASYFORGE_EXPECT(image.ColorAt(48, 16).ToHex() != "#0000FF");
}

EASYFORGE_TEST(ModelsWithMaterialsAndTextures)
{
    Model model = Model::Load(EASYFORGE_TEST_MODEL);
    EASYFORGE_REQUIRE(model);
    EASYFORGE_EXPECT(!model.Data().Materials.empty());
    EASYFORGE_EXPECT(model.Bounds().Maximum.X > model.Bounds().Minimum.X);

    Scene scene = Scene::New();
    scene.Add(model);
    Renderer renderer = Offscreen(32, 32);
    Canvas canvas = renderer.BeginFrame();
    canvas.Draw(scene);
    renderer.EndFrame();
    EASYFORGE_EXPECT(renderer.Capture());

    Model missing = Model::Load("no such model.obj");
    EASYFORGE_EXPECT(!missing);
    Scene empty;
    EASYFORGE_EXPECT(!empty);
    EASYFORGE_EXPECT(!empty.Add(model));
}
