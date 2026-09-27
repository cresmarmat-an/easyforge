#include <easyforge/core/Testing.h>

#include "../../src/window/windows/Keyboard.h"
#include "TestWindows.h"

using namespace easyforge;

EASYFORGE_TEST(ScanCodesMapToKeyPositions)
{
    using internal::KeyFromScanCode;
    EASYFORGE_EXPECT(KeyFromScanCode(0x11, false, 'W') == Key::W);
    // The same position whatever the layout says the key types.
    EASYFORGE_EXPECT(KeyFromScanCode(0x10, false, 'A') == Key::Q);
    EASYFORGE_EXPECT(KeyFromScanCode(0x1C, false, VK_RETURN) == Key::Enter);
    EASYFORGE_EXPECT(KeyFromScanCode(0x1C, true, VK_RETURN) == Key::NumberPadEnter);
    EASYFORGE_EXPECT(KeyFromScanCode(0x1D, true, VK_CONTROL) == Key::RightControl);
    EASYFORGE_EXPECT(KeyFromScanCode(0x2A, false, VK_SHIFT) == Key::LeftShift);
    EASYFORGE_EXPECT(KeyFromScanCode(0x36, false, VK_SHIFT) == Key::RightShift);
    EASYFORGE_EXPECT(KeyFromScanCode(0x45, true, VK_NUMLOCK) == Key::NumberLock);
    EASYFORGE_EXPECT(KeyFromScanCode(0x45, false, VK_PAUSE) == Key::Pause);
    EASYFORGE_EXPECT(KeyFromScanCode(0x5B, true, VK_LWIN) == Key::LeftMeta);
    EASYFORGE_EXPECT(KeyFromScanCode(0x56, false, VK_OEM_102) == Key::InternationalBackslash);
    EASYFORGE_EXPECT(KeyFromScanCode(0x76, false, VK_F24) == Key::F24);
    // Without a scan code, the virtual key finds it.
    EASYFORGE_EXPECT(KeyFromScanCode(0, false, VK_ESCAPE) == Key::Escape);
    EASYFORGE_EXPECT(KeyFromScanCode(0, false, VK_LEFT) == Key::Left);
    EASYFORGE_EXPECT(KeyFromScanCode(0x7F, false, 0) == Key::Unknown);
}

EASYFORGE_TEST(MonitorsAreListedPrimaryFirst)
{
    std::vector<Monitor> monitors = Monitor::All();
    EASYFORGE_REQUIRE(!monitors.empty());
    EASYFORGE_EXPECT(monitors.front().IsPrimary);
    int primaries = 0;
    for (const Monitor& monitor : monitors)
    {
        primaries += monitor.IsPrimary ? 1 : 0;
        EASYFORGE_EXPECT(!monitor.Area.IsEmpty());
        EASYFORGE_EXPECT(!monitor.Name.empty());
        EASYFORGE_EXPECT(monitor.Scale >= 1.0f);
        EASYFORGE_EXPECT(monitor.RefreshRate > 1.0f);
        EASYFORGE_EXPECT(Intersection(monitor.Area, monitor.WorkArea) == monitor.WorkArea);
    }
    EASYFORGE_EXPECT_EQUAL(primaries, 1);
    // The primary screen's top left is where the desktop's coordinates start.
    EASYFORGE_EXPECT_EQUAL(Monitor::Primary().Area.Position(), Vector2());

    Window window = testwindows::NewHidden();
    Monitor current = window.Monitor();
    EASYFORGE_EXPECT(!current.Area.IsEmpty());
    window.Close();
}

EASYFORGE_TEST(ClipboardHoldsText)
{
    Window window = testwindows::NewHidden();
    // Put back what was on the clipboard, so running the tests does not lose it.
    std::string before = window.ClipboardText();

    std::string text = "copied \xC3\xA9 \xE2\x9C\x93 \xF0\x9F\x98\x80\nsecond line";
    window.SetClipboardText(text);
    EASYFORGE_EXPECT_EQUAL(window.ClipboardText(), text);
    EASYFORGE_EXPECT_EQUAL(window.AsHost()->ClipboardText(), text);

    window.SetClipboardText(before);
    window.Close();
}
