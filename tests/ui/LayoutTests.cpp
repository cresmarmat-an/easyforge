#include <cmath>

#include "Helpers.h"

using namespace easyforge;
using namespace easyforge::testing;

EASYFORGE_TEST(ColumnsStackChildrenWithPaddingAndGaps)
{
    ui::Spacer first(20);
    ui::Spacer second(30);
    Screen screen(200, 200, ui::Column({ .Padding = 10, .Gap = 5, .Children = { first, second } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(first.Frame(), (Rectangle { 10, 10, 20, 20 }));
    EASYFORGE_EXPECT_EQUAL(second.Frame(), (Rectangle { 10, 35, 30, 30 }));

    // The column is the root's content, so it fills the canvas.
    EASYFORGE_EXPECT_EQUAL(screen.Root.Content.Get().Frame(), (Rectangle { 0, 0, 200, 200 }));
}

EASYFORGE_TEST(FillingChildrenShareTheSpaceLeft)
{
    ui::Spacer fixed(50);
    ui::Spacer left;
    ui::Spacer right;
    Screen screen(300, 100, ui::Row({ .Children = { fixed, left, right } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(fixed.Frame(), (Rectangle { 0, 0, 50, 50 }));
    EASYFORGE_EXPECT_EQUAL(left.Frame(), (Rectangle { 50, 0, 125, 100 }));
    EASYFORGE_EXPECT_EQUAL(right.Frame(), (Rectangle { 175, 0, 125, 100 }));
}

EASYFORGE_TEST(DistributionSpreadsChildren)
{
    ui::Spacer first(50);
    ui::Spacer second(50);
    ui::Row row({ .Distribution = ui::Distribution::Center, .Children = { first, second } });
    Screen screen(300, 100, row);
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(first.Frame().X, 100.0f);
    EASYFORGE_EXPECT_EQUAL(second.Frame().X, 150.0f);

    row.Distribution = ui::Distribution::SpaceBetween;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(first.Frame().X, 0.0f);
    EASYFORGE_EXPECT_EQUAL(second.Frame().X, 250.0f);

    row.Distribution = ui::Distribution::SpaceEvenly;
    screen.Frame();
    EASYFORGE_EXPECT_NEAR(first.Frame().X, 200.0f / 3.0f, 0.01f);

    row.Distribution = ui::Distribution::End;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(second.Frame().Right(), 300.0f);
}

EASYFORGE_TEST(AlignmentPlacesChildrenAcross)
{
    ui::Spacer child(50);
    ui::Column column({ .Alignment = ui::Alignment::Center, .Children = { child } });
    Screen screen(200, 200, column);
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(child.Frame().X, 75.0f);

    column.Alignment = ui::Alignment::End;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(child.Frame().X, 150.0f);

    // Stretching widens children that fit their content, not those with a size.
    ui::Label label("A");
    column.Alignment = ui::Alignment::Stretch;
    column.Add(label);
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(child.Frame().Width, 50.0f);
    EASYFORGE_EXPECT_EQUAL(label.Frame().Width, 200.0f);
}

EASYFORGE_TEST(SizesInPercentAndLimits)
{
    ui::Spacer half;
    half.Width = ui::Percent(50);
    half.Height = 10;
    ui::Spacer limited;
    limited.Height = 10;
    limited.MaximumWidth = 60;
    ui::Spacer margined(10);
    margined.Margin = { 5, 7 };
    Screen screen(200, 200,
        ui::Column({ .Padding = 20, .Alignment = ui::Alignment::Start, .Children = { half, limited, margined } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(half.Frame().Width, 80.0f);
    EASYFORGE_EXPECT_EQUAL(limited.Frame().Width, 60.0f);
    EASYFORGE_EXPECT_EQUAL(margined.Frame(), (Rectangle { 25, 20 + 10 + 10 + 7, 10, 10 }));
}

EASYFORGE_TEST(HiddenElementsTakeNoSpace)
{
    ui::Spacer first(20);
    ui::Spacer hidden(20);
    ui::Spacer last(20);
    Screen screen(100, 100, ui::Column({ .Gap = 4, .Children = { first, hidden, last } }));
    hidden.Visible = false;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(last.Frame().Y, 24.0f);
    hidden.Visible = true;
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(last.Frame().Y, 48.0f);
}

EASYFORGE_TEST(StacksAndGridsPlaceChildren)
{
    ui::Spacer small(20);
    ui::Stack stack({ .Width = 100, .Height = 100, .Alignment = ui::Alignment::Center, .Children = { small } });
    std::vector<ui::Element> cells;
    std::vector<ui::Spacer> spacers;
    for (int index = 0; index < 5; ++index)
    {
        ui::Spacer cell;
        cell.Height = 10.0f + static_cast<float>(index);
        spacers.push_back(cell);
        cells.push_back(cell);
    }
    ui::Grid grid({ .Width = 200, .Columns = 2, .ColumnGap = 10, .RowGap = 4, .Children = cells });
    Screen screen(300, 300, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { stack, grid } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(small.Frame(), (Rectangle { 40, 40, 20, 20 }));

    // Two columns of 95 points; each row as tall as its tallest cell.
    Rectangle top = grid.Frame();
    EASYFORGE_EXPECT_EQUAL(spacers[0].Frame(), (Rectangle { 0, top.Y, 95, 10 }));
    EASYFORGE_EXPECT_EQUAL(spacers[1].Frame(), (Rectangle { 105, top.Y, 95, 11 }));
    EASYFORGE_EXPECT_EQUAL(spacers[2].Frame().Y, top.Y + 11 + 4);
    EASYFORGE_EXPECT_EQUAL(spacers[4].Frame().X, 0.0f);
    EASYFORGE_EXPECT_EQUAL(top.Height, 11.0f + 4 + 13 + 4 + 14);
}

EASYFORGE_TEST(LabelsMeasureTheirText)
{
    Font font = Font::Load(EASYFORGE_TEST_FONT);
    EASYFORGE_REQUIRE(font);
    ui::Label label("AO");
    ui::Label big("AO", { .FontSize = 40 });
    Screen screen(400, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { label, big } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(label.Frame().Size(), font.Measure("AO", 20));
    EASYFORGE_EXPECT_EQUAL(big.Frame().Size(), font.Measure("AO", 40));

    // Text changes lay the label out again.
    label.Text = "AOAO";
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(label.Frame().Width, font.Measure("AOAO", 20).X);
}

EASYFORGE_TEST(WrappedLabelsBreakBetweenWords)
{
    Font font = Font::Load(EASYFORGE_TEST_FONT);
    float word = font.Measure("AO", 20).X;
    ui::Label label("AO AO AO", { .Wrap = true });
    Screen screen(400, 400, ui::Column({ .Width = word + 1.0f, .Children = { label } }));
    screen.Frame();
    EASYFORGE_EXPECT_NEAR(label.Frame().Height, font.LineHeight(20) * 3.0f, 0.01f);

    // A wider column fits two words on the first line.
    ui::Label wide("AO AO AO", { .Wrap = true });
    Screen wider(400, 400, ui::Column({ .Width = font.Measure("AO AO", 20).X + 1.0f, .Children = { wide } }));
    wider.Frame();
    EASYFORGE_EXPECT_NEAR(wide.Frame().Height, font.LineHeight(20) * 2.0f, 0.01f);
}

EASYFORGE_TEST(LayoutFollowsTheScale)
{
    ui::Spacer child(20);
    Screen screen(200, 200, ui::Column({ .Padding = 10, .Children = { child } }), 2.0f);
    screen.Frame();
    // Points stay points: the canvas is 100 by 100 points at two pixels each.
    EASYFORGE_EXPECT_EQUAL(screen.Root.Content.Get().Frame(), (Rectangle { 0, 0, 100, 100 }));
    EASYFORGE_EXPECT_EQUAL(child.Frame(), (Rectangle { 10, 10, 20, 20 }));
}

EASYFORGE_TEST(MeasuringMatchesPlacing)
{
    Font font = Font::Load(EASYFORGE_TEST_FONT);

    // A wrapped label that fills a row is measured at the width it is given, so
    // the row is as tall as the label's lines.
    std::string text = "AO AO AO AO AO AO AO AO AO AO AO AO AO AO";
    ui::Label probe(text, { .Width = 240, .Wrap = true });
    Screen probeScreen(300, 400, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { probe } }));
    probeScreen.Frame();
    ui::Label wrapped(text, { .Width = ui::Fill, .Wrap = true });
    ui::Row row({ .Children = { ui::Panel({ .Width = 60, .Height = 10 }), wrapped } });
    ui::Label next("Next");
    Screen screen(300, 400, ui::Column({ .Children = { row, next } }));
    screen.Frame();
    EASYFORGE_EXPECT_EQUAL(wrapped.Frame().Width, 240.0f);
    EASYFORGE_EXPECT_EQUAL(wrapped.Frame().Height, probe.Frame().Height);
    EASYFORGE_EXPECT_EQUAL(next.Frame().Y, row.Frame().Bottom());

    // Children that fill a line sized by its content each get what the widest needs.
    ui::Button ok("OK", { .Width = ui::Fill });
    ui::Button cancel("Cancel and discard", { .Width = ui::Fill });
    Screen buttons(600, 200, ui::Column({ .Alignment = ui::Alignment::Center, .Children = { ui::Row({ .Children = { ok, cancel } }) } }));
    buttons.Frame();
    EASYFORGE_EXPECT_EQUAL(ok.Frame().Width, cancel.Frame().Width);
    EASYFORGE_EXPECT(std::abs(cancel.Frame().Width - (font.Measure("Cancel and discard", 20.0f).X + 28.0f)) < 0.5f);

    // Space a filling child cannot take because of its maximum goes to the others.
    ui::Column side({ .Width = ui::Fill, .MaximumWidth = 250 });
    ui::Column body({ .Width = ui::Fill });
    Screen wide(1000, 100, ui::Row({ .Children = { side, body } }));
    wide.Frame();
    EASYFORGE_EXPECT_EQUAL(side.Frame().Width, 250.0f);
    EASYFORGE_EXPECT_EQUAL(body.Frame().Width, 750.0f);

    // A grid with fewer children than columns keeps room for every column.
    ui::Button open("Open", { .Width = 60 });
    ui::Grid grid({ .Columns = 3, .ColumnGap = 8, .Children = { open, ui::Button("Save", { .Width = 60 }) } });
    Screen gridScreen(400, 200, ui::Column({ .Alignment = ui::Alignment::Center, .Children = { grid } }));
    gridScreen.Frame();
    EASYFORGE_EXPECT_EQUAL(grid.Frame().Width, 196.0f);
    EASYFORGE_EXPECT_EQUAL(open.Frame().Width, 60.0f);

    // A row gap follows every row, even an empty first one.
    ui::Grid gapped({ .Columns = 1, .RowGap = 12, .Children = { ui::Panel({ .Height = 0 }), ui::Panel({ .Height = 20 }) } });
    Screen gapScreen(200, 200, ui::Column({ .Alignment = ui::Alignment::Start, .Children = { gapped } }));
    gapScreen.Frame();
    EASYFORGE_EXPECT_EQUAL(gapped.Frame().Height, 32.0f);

    // A root's content keeps its limits when it fits the window.
    ui::Column limited({ .MaximumWidth = 720 });
    Screen limitScreen(1400, 200, limited);
    limitScreen.Frame();
    EASYFORGE_EXPECT_EQUAL(limited.Frame().Width, 720.0f);
}
