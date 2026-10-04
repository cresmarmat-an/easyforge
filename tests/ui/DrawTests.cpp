#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

namespace
{
    Color At(const ImageData& image, float x, float y)
    {
        return image.ColorAt(static_cast<int>(x), static_cast<int>(y));
    }

    // Everything in a column that does not stretch its children.
    ui::Column Holder(std::vector<ui::Element> children)
    {
        return ui::Column({ .Alignment = ui::Alignment::Start, .Children = std::move(children) });
    }
}

EASYFORGE_TEST(PanelsAndButtonsDrawTheirBoxes)
{
    ui::Theme theme = TestTheme();
    ui::Panel panel({ .Width = 100, .Height = 60 });
    ui::Button accent("", { .Width = 80, .Height = 30, .Style = ui::ButtonStyle::Accent });
    ui::Panel red({ .Width = 40, .Height = 40, .Background = Color::Hex("#FF0000") });
    Screen screen(300, 200, Holder({ panel, accent, red }));
    ImageData image = screen.Picture();
    EASYFORGE_EXPECT(NearColor(At(image, 50, 30), theme.Surface));
    EASYFORGE_EXPECT(NearColor(At(image, 250, 150), theme.Background));
    EASYFORGE_EXPECT(NearColor(At(image, 40, 75), theme.Accent));
    EASYFORGE_EXPECT_EQUAL(PixelAt(image, 20, 110), std::string("#FF0000"));

    // Hovering eases the accent toward its hovered color.
    screen.Move({ 40, 75 });
    screen.Run(0.5f);
    image = screen.Picture();
    EASYFORGE_EXPECT(NearColor(At(image, 40, 75), theme.AccentHovered));
}

EASYFORGE_TEST(OpacityFadesEverythingInside)
{
    ui::Panel panel({ .Width = 100, .Height = 100, .Background = Color::Black, .Opacity = 0.5f,
        .Children = { ui::Panel({ .Width = 50, .Height = 50, .Background = Color::Black }) } });
    Screen screen(200, 200, Holder({ panel }));
    ui::Theme theme = screen.Root.Theme;
    theme.Background = Color::White;
    screen.Root.Theme = theme;
    ImageData image = screen.Picture();
    // The child and the panel fade as one, so where they overlap is no darker.
    EASYFORGE_EXPECT(NearColor(At(image, 25, 25), Color::Hex("#808080"), 0.03f));
    EASYFORGE_EXPECT(NearColor(At(image, 75, 75), Color::Hex("#808080"), 0.03f));
}

EASYFORGE_TEST(EffectsDrawAroundTheBox)
{
    ui::Theme theme = TestTheme();
    ui::Panel shadowed({ .Width = 60, .Height = 40, .Margin = 20,
        .Effects = { ui::Shadow { .Offset = { 0, 10 }, .Blur = 4, .Color = Color::Black } } });
    ui::Panel outlined({ .Width = 60, .Height = 40, .Margin = 20,
        .Effects = { ui::Outline { .Width = 3, .Gap = 2, .Color = Color::Hex("#00FF00") } } });
    Screen screen(300, 300, ui::Row({ .Alignment = ui::Alignment::Start, .Children = { shadowed, outlined } }));
    ImageData image = screen.Picture();

    // Below the panel, the shadow darkens the background.
    Color below = At(image, 50, 66);
    EASYFORGE_EXPECT(below.Red < theme.Background.Red - 0.3f);
    // Beside it, where the shadow does not reach, the background is as it was.
    EASYFORGE_EXPECT(NearColor(At(image, 5, 40), theme.Background));

    // The outline sits outside the box, past a gap.
    Rectangle box = outlined.Frame();
    EASYFORGE_EXPECT_EQUAL(PixelAt(image, static_cast<int>(box.X) - 4, static_cast<int>(box.Center().Y)), std::string("#00FF00"));
    EASYFORGE_EXPECT(NearColor(At(image, box.X - 1, box.Center().Y), theme.Background));
}

EASYFORGE_TEST(ColorAdjustAndMasksShadeTheElement)
{
    ui::Panel gray({ .Width = 40, .Height = 40, .Background = Color::Hex("#FF0000"),
        .Effects = { ui::ColorAdjust { .Saturation = 0 } } });
    ui::Panel rounded({ .Width = 60, .Height = 60, .CornerRadius = 0.0f, .Background = Color::Hex("#0000FF"),
        .Effects = { ui::Mask { .CornerRadius = 30 } } });
    Screen screen(200, 200, Holder({ gray, rounded }));
    ui::Theme theme = screen.Root.Theme;
    ImageData image = screen.Picture();

    Color shade = At(image, 20, 20);
    EASYFORGE_EXPECT(std::abs(shade.Red - shade.Green) < 0.02f);
    EASYFORGE_EXPECT(std::abs(shade.Green - shade.Blue) < 0.02f);
    EASYFORGE_EXPECT(shade.Red > 0.1f && shade.Red < 0.5f);

    // The mask makes a circle: the middle stays, the corner shows the background.
    EASYFORGE_EXPECT_EQUAL(PixelAt(image, 30, 70), std::string("#0000FF"));
    EASYFORGE_EXPECT(NearColor(At(image, 2, 42), theme.Background));
}

EASYFORGE_TEST(ElementsCanHaveShaders)
{
    Shader green = Shader::FromText("function Pixel(input: PixelInput) returns color then\n"
                                    "    return color(0, 1, 0)\nend\n");
    EASYFORGE_REQUIRE(green);
    ui::Panel panel({ .Width = 50, .Height = 50, .Shader = green });
    Screen screen(100, 100, Holder({ panel }));
    ImageData image = screen.Picture();
    EASYFORGE_EXPECT_EQUAL(PixelAt(image, 25, 25), std::string("#00FF00"));
}

EASYFORGE_TEST(ImagesFitTheirBox)
{
    ImageData pixels;
    pixels.Width = 2;
    pixels.Height = 1;
    pixels.Pixels = { 255, 0, 0, 255, 0, 0, 255, 255 };
    Texture texture = Texture::FromImage(pixels, { .Smooth = false });
    ui::Image stretched(texture, { .Width = 40, .Height = 40, .Fit = ui::ImageFit::Stretch });
    ui::Image contained(texture, { .Width = 40, .Height = 40 });
    ui::Image natural(texture);
    Screen screen(200, 200, Holder({ stretched, contained, natural }));
    ImageData image = screen.Picture();
    EASYFORGE_EXPECT_EQUAL(PixelAt(image, 5, 35), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(PixelAt(image, 35, 35), std::string("#0000FF"));

    // Contained keeps its shape: 40 by 20, centered from top to bottom.
    ui::Theme theme = screen.Root.Theme;
    EASYFORGE_EXPECT(NearColor(At(image, 5, 42), theme.Background));
    EASYFORGE_EXPECT_EQUAL(PixelAt(image, 5, 60), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(natural.Frame().Size(), (Vector2 { 2, 1 }));
}

EASYFORGE_TEST(DisplaysSwitchWithTransitions)
{
    std::vector<std::string> shown;
    int hidden = 0;
    ui::Display first("First", { .Background = Color::Hex("#FF0000") });
    first.OnHidden = [&] { ++hidden; };
    ui::Displays pages({ .Width = 100, .Height = 100,
        .OnChange = [&](const std::string& name) { shown.push_back(name); },
        .Children = { first, ui::Display("Second", { .Background = Color::Hex("#0000FF") }) } });
    Screen screen(100, 100, pages);
    EASYFORGE_EXPECT_EQUAL(pages.Current.Get(), std::string("First"));
    EASYFORGE_EXPECT_EQUAL(PixelAt(screen.Picture(), 50, 50), std::string("#FF0000"));

    // Halfway through a fade, each is drawn at half strength: the old over the
    // background, and the new over both.
    EASYFORGE_EXPECT(pages.Show("Second", ui::Transition::Fade(1.0f)));
    screen.Frame(0.5f);
    Color middle = At(screen.Picture(), 50, 50);
    ui::Theme theme = screen.Root.Theme;
    EASYFORGE_EXPECT(std::abs(middle.Red - (0.25f + theme.Background.Red * 0.25f)) < 0.06f);
    EASYFORGE_EXPECT(std::abs(middle.Blue - (0.5f + theme.Background.Blue * 0.25f)) < 0.06f);
    EASYFORGE_EXPECT_EQUAL(hidden, 0);
    screen.Frame(0.6f);
    EASYFORGE_EXPECT_EQUAL(PixelAt(screen.Picture(), 50, 50), std::string("#0000FF"));
    EASYFORGE_EXPECT_EQUAL(hidden, 1);

    // Back goes to the first, sliding the other way.
    EASYFORGE_EXPECT(pages.CanGoBack());
    EASYFORGE_EXPECT(pages.Back());
    EASYFORGE_EXPECT(!pages.CanGoBack());
    EASYFORGE_EXPECT_EQUAL(pages.Current.Get(), std::string("First"));
    screen.Frame(0.0f);
    EASYFORGE_EXPECT_EQUAL(shown, (std::vector<std::string> { "Second", "First" }));

    // Sliding in from the right: halfway, each covers half.
    pages.Show("Second", ui::Transition::Slide(1.0f));
    screen.Frame(0.5f);
    ImageData sliding = screen.Picture();
    EASYFORGE_EXPECT_EQUAL(PixelAt(sliding, 25, 50), std::string("#FF0000"));
    EASYFORGE_EXPECT_EQUAL(PixelAt(sliding, 75, 50), std::string("#0000FF"));

    EASYFORGE_EXPECT(!pages.Show("Missing"));
}

EASYFORGE_TEST(ThemesSwitchAndAnimate)
{
    ui::Panel panel({ .Width = 50, .Height = 50 });
    Screen screen(100, 100, Holder({ panel }));
    ui::Theme light = TestTheme();
    ui::Theme dark = ui::Theme::Dark();
    dark.Font = light.Font;
    dark.FontSize = light.FontSize;

    screen.Root.Theme = dark;
    EASYFORGE_EXPECT(NearColor(At(screen.Picture(), 25, 25), dark.Surface));

    screen.Root.Theme.AnimateTo(light, { .Duration = 1.0f, .Easing = ui::Easing::Linear });
    screen.Frame(0.5f);
    Color halfway = At(screen.Picture(), 25, 25);
    EASYFORGE_EXPECT(NearColor(halfway, Lerp(dark.Surface, light.Surface, 0.5f), 0.03f));
    screen.Frame(0.6f);
    EASYFORGE_EXPECT(NearColor(At(screen.Picture(), 25, 25), light.Surface));
}

EASYFORGE_TEST(LabelsFollowData)
{
    Table game = Table::New();
    Node player = game.Add("Player");
    player["Health"] = 100;
    ui::Label health(ui::Bind(player["Health"], "Health: {}"));
    Screen screen(300, 100, Holder({ health }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(health.Text.Get(), std::string("Health: 100"));

    player["Health"] = 90;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(health.Text.Get(), std::string("Health: 90"));

    // Assigning text stops following.
    health.Text = "Gone";
    player["Health"] = 80;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(health.Text.Get(), std::string("Gone"));
}

EASYFORGE_TEST(PropertiesAnimate)
{
    bool finished = false;
    ui::Panel panel({ .Width = 50, .Height = 50 });
    Screen screen(100, 100, Holder({ panel }));
    panel.Opacity.AnimateTo(0.0f, { .Duration = 1.0f, .Easing = ui::Easing::Linear, .OnFinished = [&] { finished = true; } });
    screen.Frame(0.25f);
    EASYFORGE_EXPECT_NEAR(panel.Opacity.Get(), 0.75f, 0.001f);
    screen.Frame(0.5f);
    EASYFORGE_EXPECT_NEAR(panel.Opacity.Get(), 0.25f, 0.001f);
    EASYFORGE_EXPECT(!finished);
    screen.Frame(0.5f);
    EASYFORGE_EXPECT_EQUAL(panel.Opacity.Get(), 0.0f);
    EASYFORGE_EXPECT(finished);

    // Assigning stops an animation.
    panel.Offset.AnimateTo({ 40, 0 }, { .Duration = 1.0f, .Easing = ui::Easing::Linear });
    screen.Frame(0.5f);
    panel.Offset = Vector2 { 5, 5 };
    screen.Frame(0.5f);
    EASYFORGE_EXPECT_EQUAL(panel.Offset.Get(), (Vector2 { 5, 5 }));

    // A delay waits before starting.
    panel.Scale.AnimateTo(2.0f, { .Duration = 1.0f, .Easing = ui::Easing::Linear, .Delay = 1.0f });
    screen.Frame(0.5f);
    EASYFORGE_EXPECT_EQUAL(panel.Scale.Get(), 1.0f);
    screen.Frame(1.0f);
    EASYFORGE_EXPECT_NEAR(panel.Scale.Get(), 1.5f, 0.001f);
}

EASYFORGE_TEST(MovedElementsAreClickedWhereTheyAreDrawn)
{
    int clicks = 0;
    ui::Button button("", { .Width = 40, .Height = 40, .OnClick = [&] { ++clicks; } });
    Screen screen(200, 200, Holder({ button }));
    button.Offset = Vector2 { 100, 100 };
    screen.Frame();
    screen.Click({ 20, 20 });
    EASYFORGE_EXPECT_EQUAL(clicks, 0);
    screen.Click({ 120, 120 });
    EASYFORGE_EXPECT_EQUAL(clicks, 1);
}

EASYFORGE_TEST(EffectsStayOutsideAndAboveTheirBox)
{
    ui::Theme theme = TestTheme();

    // A shadow under a box without a background shows only outside the box.
    ui::Panel clear({ .Width = 60, .Height = 60, .Background = Color::Transparent,
        .Effects = { ui::Shadow { .Offset = { 0, 0 }, .Blur = 10, .Color = Color::Black } } });
    Screen screen(200, 200, Holder({ ui::Column({ .Padding = 30, .Children = { clear } }) }));
    ImageData image = screen.Picture();
    EASYFORGE_EXPECT(NearColor(At(image, 60, 60), theme.Background));
    EASYFORGE_EXPECT(!NearColor(At(image, 27, 60), theme.Background, 0.05f));

    // A masked element keeps its outline, which lies outside the mask.
    ui::Panel masked({ .Width = 60, .Height = 60, .Background = Color::Hex("#FF0000"),
        .Effects = { ui::Mask { .CornerRadius = 8 }, ui::Outline { .Width = 2, .Gap = 2, .Color = Color::Hex("#0000FF") } } });
    Screen maskScreen(200, 200, Holder({ ui::Column({ .Padding = 30, .Children = { masked } }) }));
    ImageData maskImage = maskScreen.Picture();
    EASYFORGE_EXPECT_EQUAL(PixelAt(maskImage, 27, 60), std::string("#0000FF"));

    // A faded element with a background blur still blurs what is under it.
    std::vector<ui::Element> stripes;
    for (int index = 0; index < 20; ++index)
    {
        stripes.push_back(ui::Panel({ .Width = 4, .Height = 100, .CornerRadius = 0.0f,
            .Background = Color::Hex(index % 2 ? "#000000" : "#FFFFFF") }));
    }
    ui::Panel frosted({ .Width = 60, .Height = 60, .Margin = { 10, 20, 0, 0 }, .Background = Color::Transparent,
        .Opacity = 0.99f, .Effects = { ui::BackgroundBlur { .Radius = 8 } } });
    Screen blurScreen(100, 100, ui::Stack({ .Children = { ui::Row({ .Children = stripes }), ui::Column({ .Alignment = ui::Alignment::Start, .Children = { frosted } }) } }));
    Color middle = At(blurScreen.Picture(), 40, 50);
    EASYFORGE_EXPECT(middle.Red > 0.2f && middle.Red < 0.8f);

    // A faded column far taller than any picture draws without trouble.
    std::vector<ui::Element> rows;
    for (int index = 0; index < 400; ++index)
    {
        rows.push_back(ui::Panel({ .Height = 44 }));
    }
    ui::Scroll tall({ .Width = 200, .Height = 200, .Children = { ui::Column({ .Opacity = 0.9f, .Children = rows }) } });
    Screen tallScreen(200, 200, tall);
    ImageData tallImage = tallScreen.Picture();
    EASYFORGE_EXPECT(!NearColor(tallImage.ColorAt(100, 100), Color::Transparent, 0.01f));
}
