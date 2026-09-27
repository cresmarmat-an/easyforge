#include <easyforge/core.h>
#include <easyforge/core/Testing.h>

#include <string>
#include <type_traits>

using namespace easyforge;

namespace
{
    // A small handle-like owner: the properties read and write its fields and
    // count the writes, the way a window would react to a new title.
    struct Settings
    {
        std::string Title = "Untitled";
        int Width = 640;
        Vector2 Size = { 3.0f, 4.0f };
        int Writes = 0;
    };

    struct Panel
    {
        explicit Panel(Settings& settings)
            : Title(&settings,
                  [](const void* owner) { return static_cast<const Settings*>(owner)->Title; },
                  [](void* owner, const std::string& value) {
                      auto* settings = static_cast<Settings*>(owner);
                      settings->Title = value;
                      ++settings->Writes;
                  }),
              Width(&settings,
                  [](const void* owner) { return static_cast<const Settings*>(owner)->Width; },
                  [](void* owner, const int& value) {
                      auto* settings = static_cast<Settings*>(owner);
                      settings->Width = value;
                      ++settings->Writes;
                  }),
              Size(&settings,
                  [](const void* owner) { return static_cast<const Settings*>(owner)->Size; },
                  [](void* owner, const Vector2& value) { static_cast<Settings*>(owner)->Size = value; })
        {
        }

        Property<std::string> Title;
        Property<int> Width;
        Property<Vector2> Size;
    };
}

static_assert(!std::is_copy_constructible_v<Property<int>>, "`auto x = object.Property;` must not compile");

EASYFORGE_TEST(PropertyReadsAndWritesTheOwner)
{
    Settings settings;
    Panel panel(settings);

    std::string title = panel.Title;
    EASYFORGE_EXPECT_EQUAL(title, std::string("Untitled"));

    panel.Title = "Notes";
    EASYFORGE_EXPECT_EQUAL(settings.Title, std::string("Notes"));
    EASYFORGE_EXPECT_EQUAL(settings.Writes, 1);
    EASYFORGE_EXPECT_EQUAL(panel.Title.Get(), std::string("Notes"));
}

EASYFORGE_TEST(PropertyComparesWithValues)
{
    Settings settings;
    Panel panel(settings);

    EASYFORGE_EXPECT(panel.Title == "Untitled");
    EASYFORGE_EXPECT(panel.Title != "Other");
    EASYFORGE_EXPECT(panel.Width == 640);
    EASYFORGE_EXPECT(640 == panel.Width);
}

EASYFORGE_TEST(PropertyCompoundAssignment)
{
    Settings settings;
    Panel panel(settings);

    panel.Width += 10;
    panel.Width *= 2;
    panel.Title += " copy";
    panel.Size -= Vector2 { 1.0f, 1.0f };

    EASYFORGE_EXPECT_EQUAL(settings.Width, 1300);
    EASYFORGE_EXPECT_EQUAL(settings.Title, std::string("Untitled copy"));
    EASYFORGE_EXPECT_EQUAL(settings.Size, (Vector2 { 2.0f, 3.0f }));
}

EASYFORGE_TEST(PropertyMembersThroughArrow)
{
    Settings settings;
    Panel panel(settings);

    EASYFORGE_EXPECT_EQUAL(panel.Size->X, 3.0f);
    EASYFORGE_EXPECT_EQUAL(panel.Title->size(), std::size_t { 8 });
}

EASYFORGE_TEST(PropertyAssignmentCopiesTheValue)
{
    Settings firstSettings;
    Settings secondSettings;
    Panel first(firstSettings);
    Panel second(secondSettings);

    first.Title = "First";
    second.Title = first.Title;
    first.Title = "Changed";

    EASYFORGE_EXPECT_EQUAL(secondSettings.Title, std::string("First"));
}

EASYFORGE_TEST(PropertyAssignsThroughConst)
{
    Settings settings;
    const Panel panel(settings);

    panel.Width = 800;
    EASYFORGE_EXPECT_EQUAL(settings.Width, 800);
}

EASYFORGE_TEST(PropertyRebind)
{
    Settings firstSettings;
    Settings secondSettings;
    Panel panel(firstSettings);

    panel.Width.Rebind(&secondSettings);
    panel.Width = 1;
    EASYFORGE_EXPECT_EQUAL(firstSettings.Width, 640);
    EASYFORGE_EXPECT_EQUAL(secondSettings.Width, 1);
}
