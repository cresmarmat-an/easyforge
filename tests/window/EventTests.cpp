#include <easyforge/core/Testing.h>

#include <cstring>

#include "TestWindows.h"

#include <shellapi.h>
#include <shlobj.h>

using namespace easyforge;
using testwindows::EventLog;
using testwindows::HandleOf;
using testwindows::KeyParameter;
using testwindows::NewHidden;
using testwindows::Pump;

namespace
{
    LPARAM PixelParameter(int x, int y)
    {
        return MAKELPARAM(x, y);
    }

    LPARAM ScreenParameter(HWND handle, int x, int y)
    {
        POINT point { x, y };
        ClientToScreen(handle, &point);
        return MAKELPARAM(point.x, point.y);
    }
}

EASYFORGE_TEST(KeysArriveByPosition)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    HWND handle = HandleOf(window);

    PostMessageW(handle, WM_KEYDOWN, 'W', KeyParameter(0x11));
    PostMessageW(handle, WM_KEYDOWN, 'W', KeyParameter(0x11, false, true));
    PostMessageW(handle, WM_KEYUP, 'W', KeyParameter(0x11, false, false, true));
    // The right arrow shares its scan code with 6 on the number pad, and is told
    // apart by the extended flag.
    PostMessageW(handle, WM_KEYDOWN, VK_RIGHT, KeyParameter(0x4D, true));
    PostMessageW(handle, WM_KEYDOWN, VK_NUMPAD6, KeyParameter(0x4D));
    Pump();

    std::vector<Event> pressed = log.OfType(EventType::KeyPressed);
    EASYFORGE_REQUIRE(pressed.size() == 4);
    EASYFORGE_EXPECT(pressed[0].Key == Key::W);
    EASYFORGE_EXPECT(!pressed[0].Repeat);
    EASYFORGE_EXPECT(pressed[1].Key == Key::W);
    EASYFORGE_EXPECT(pressed[1].Repeat);
    EASYFORGE_EXPECT(pressed[2].Key == Key::Right);
    EASYFORGE_EXPECT(pressed[3].Key == Key::NumberPad6);

    std::vector<Event> released = log.OfType(EventType::KeyReleased);
    EASYFORGE_REQUIRE(released.size() == 1);
    EASYFORGE_EXPECT(released[0].Key == Key::W);
    window.Close();
}

EASYFORGE_TEST(HeldKeysAreReleasedWhenFocusIsLost)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    HWND handle = HandleOf(window);

    PostMessageW(handle, WM_KEYDOWN, 'A', KeyParameter(0x1E));
    PostMessageW(handle, WM_KEYDOWN, VK_SPACE, KeyParameter(0x39));
    Pump();
    SendMessageW(handle, WM_KILLFOCUS, 0, 0);

    std::vector<Event> released = log.OfType(EventType::KeyReleased);
    EASYFORGE_REQUIRE(released.size() == 2);
    EASYFORGE_EXPECT(released[0].Key == Key::A);
    EASYFORGE_EXPECT(released[1].Key == Key::Space);
    EASYFORGE_EXPECT_EQUAL(log.OfType(EventType::FocusLost).size(), std::size_t { 1 });
    EASYFORGE_EXPECT(log.Events.back().Type == EventType::FocusLost);
    window.Close();
}

EASYFORGE_TEST(PrintScreenGetsAPress)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    PostMessageW(HandleOf(window), WM_KEYUP, VK_SNAPSHOT, KeyParameter(0x37, true, false, true));
    Pump();
    EASYFORGE_REQUIRE(log.Events.size() == 2);
    EASYFORGE_EXPECT(log.Events[0].Type == EventType::KeyPressed);
    EASYFORGE_EXPECT(log.Events[0].Key == Key::PrintScreen);
    EASYFORGE_EXPECT(log.Events[1].Type == EventType::KeyReleased);
    window.Close();
}

EASYFORGE_TEST(TextArrivesAsUtf8)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    HWND handle = HandleOf(window);

    PostMessageW(handle, WM_CHAR, L'a', 0);
    PostMessageW(handle, WM_CHAR, 0x00E9, 0);
    // An emoji outside the first 65536 characters comes in two halves.
    PostMessageW(handle, WM_CHAR, 0xD83D, 0);
    PostMessageW(handle, WM_CHAR, 0xDE00, 0);
    // Backspace and Enter are keys, not text.
    PostMessageW(handle, WM_CHAR, 0x08, 0);
    PostMessageW(handle, WM_CHAR, 0x0D, 0);
    Pump();

    std::vector<Event> text = log.OfType(EventType::TextEntered);
    EASYFORGE_REQUIRE(text.size() == 3);
    EASYFORGE_EXPECT_EQUAL(text[0].Text, std::string("a"));
    EASYFORGE_EXPECT_EQUAL(text[1].Text, std::string("\xC3\xA9"));
    EASYFORGE_EXPECT_EQUAL(text[2].Text, std::string("\xF0\x9F\x98\x80"));
    window.Close();
}

EASYFORGE_TEST(MouseMovesInPoints)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    HWND handle = HandleOf(window);
    float scale = window.Scale();

    PostMessageW(handle, WM_MOUSEMOVE, 0, PixelParameter(30, 40));
    PostMessageW(handle, WM_MOUSEMOVE, 0, PixelParameter(36, 40));
    Pump();

    EASYFORGE_EXPECT_EQUAL(log.OfType(EventType::MouseEntered).size(), std::size_t { 1 });
    std::vector<Event> moved = log.OfType(EventType::MouseMoved);
    EASYFORGE_REQUIRE(moved.size() == 2);
    EASYFORGE_EXPECT_NEAR(moved[0].Position.X, 30 / scale, 0.001f);
    EASYFORGE_EXPECT_NEAR(moved[0].Position.Y, 40 / scale, 0.001f);
    EASYFORGE_EXPECT_EQUAL(moved[0].Movement, Vector2());
    EASYFORGE_EXPECT_NEAR(moved[1].Movement.X, 6 / scale, 0.001f);
    EASYFORGE_EXPECT_NEAR(moved[1].Movement.Y, 0.0f, 0.001f);
    window.Close();
}

EASYFORGE_TEST(MouseButtonsCountClicks)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    HWND handle = HandleOf(window);

    for (int click = 0; click < 3; ++click)
    {
        PostMessageW(handle, WM_LBUTTONDOWN, MK_LBUTTON, PixelParameter(10, 10));
        PostMessageW(handle, WM_LBUTTONUP, 0, PixelParameter(10, 10));
    }
    PostMessageW(handle, WM_RBUTTONDOWN, MK_RBUTTON, PixelParameter(10, 10));
    PostMessageW(handle, WM_RBUTTONUP, 0, PixelParameter(10, 10));
    PostMessageW(handle, WM_XBUTTONDOWN, MAKEWPARAM(0, XBUTTON1), PixelParameter(10, 10));
    PostMessageW(handle, WM_XBUTTONUP, MAKEWPARAM(0, XBUTTON1), PixelParameter(10, 10));
    Pump();

    std::vector<Event> pressed = log.OfType(EventType::MouseButtonPressed);
    EASYFORGE_REQUIRE(pressed.size() == 5);
    EASYFORGE_EXPECT_EQUAL(pressed[0].ClickCount, 1);
    EASYFORGE_EXPECT_EQUAL(pressed[1].ClickCount, 2);
    EASYFORGE_EXPECT_EQUAL(pressed[2].ClickCount, 3);
    EASYFORGE_EXPECT(pressed[3].Button == MouseButton::Right);
    EASYFORGE_EXPECT_EQUAL(pressed[3].ClickCount, 1);
    EASYFORGE_EXPECT(pressed[4].Button == MouseButton::Back);
    EASYFORGE_EXPECT_EQUAL(log.OfType(EventType::MouseButtonReleased).size(), std::size_t { 5 });

    // A release without a press, such as after a click that began elsewhere, is not reported.
    log.Events.clear();
    PostMessageW(handle, WM_MBUTTONUP, 0, PixelParameter(10, 10));
    Pump();
    EASYFORGE_EXPECT(log.OfType(EventType::MouseButtonReleased).empty());
    window.Close();
}

EASYFORGE_TEST(MouseWheelBothWays)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    HWND handle = HandleOf(window);
    PostMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), ScreenParameter(handle, 5, 5));
    PostMessageW(handle, WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<WORD>(-WHEEL_DELTA / 2)), ScreenParameter(handle, 5, 5));
    PostMessageW(handle, WM_MOUSEHWHEEL, MAKEWPARAM(0, WHEEL_DELTA), ScreenParameter(handle, 5, 5));
    Pump();

    std::vector<Event> wheel = log.OfType(EventType::MouseWheel);
    EASYFORGE_REQUIRE(wheel.size() == 3);
    EASYFORGE_EXPECT_EQUAL(wheel[0].Wheel, Vector2(0, 1));
    EASYFORGE_EXPECT_EQUAL(wheel[1].Wheel, Vector2(0, -0.5f));
    EASYFORGE_EXPECT_EQUAL(wheel[2].Wheel, Vector2(1, 0));
    EASYFORGE_EXPECT_NEAR(wheel[0].Position.X, 5 / window.Scale(), 0.001f);
    window.Close();
}

EASYFORGE_TEST(DroppedFilesArriveWithTheirPaths)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);

    // The block Explorer sends: a DROPFILES header, then the paths, each ending
    // with a zero and the list with another.
    std::wstring paths = std::wstring(L"C:\\one.txt") + L'\0' + L"C:\\two \u2713.png" + L'\0' + L'\0';
    SIZE_T bytes = sizeof(DROPFILES) + paths.size() * sizeof(wchar_t);
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, bytes);
    auto* drop = static_cast<DROPFILES*>(GlobalLock(memory));
    drop->pFiles = sizeof(DROPFILES);
    drop->pt = { 12, 24 };
    drop->fWide = TRUE;
    std::memcpy(reinterpret_cast<std::uint8_t*>(drop) + sizeof(DROPFILES), paths.data(), paths.size() * sizeof(wchar_t));
    GlobalUnlock(memory);

    PostMessageW(HandleOf(window), WM_DROPFILES, reinterpret_cast<WPARAM>(memory), 0);
    Pump();

    std::vector<Event> dropped = log.OfType(EventType::FilesDropped);
    EASYFORGE_REQUIRE(dropped.size() == 1);
    EASYFORGE_REQUIRE(dropped[0].Files.size() == 2);
    EASYFORGE_EXPECT_EQUAL(dropped[0].Files[0], std::string("C:\\one.txt"));
    EASYFORGE_EXPECT_EQUAL(dropped[0].Files[1], std::string("C:\\two \xE2\x9C\x93.png"));
    EASYFORGE_EXPECT_NEAR(dropped[0].Position.X, 12 / window.Scale(), 0.001f);
    window.Close();
}

EASYFORGE_TEST(ResizingReportsTheNewSize)
{
    Window window = NewHidden(300, 200);
    EventLog log;
    log.Follow(window);
    window.Size = { 350, 250 };

    std::vector<Event> resized = log.OfType(EventType::Resized);
    EASYFORGE_REQUIRE(!resized.empty());
    EASYFORGE_EXPECT_NEAR(resized.back().Size.X, 350.0f, 0.5f);
    EASYFORGE_EXPECT_NEAR(resized.back().Size.Y, 250.0f, 0.5f);

    // Minimizing is not a resize to nothing.
    log.Events.clear();
    SendMessageW(HandleOf(window), WM_SIZE, SIZE_MINIMIZED, 0);
    EASYFORGE_EXPECT(log.OfType(EventType::Resized).empty());
    EASYFORGE_EXPECT_EQUAL(log.OfType(EventType::Minimized).size(), std::size_t { 1 });
    window.Close();
}

EASYFORGE_TEST(ColorSchemeChangeIsReported)
{
    Window window = NewHidden();
    EventLog log;
    log.Follow(window);
    Color before = window.Background;
    SendMessageW(HandleOf(window), WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"ImmersiveColorSet"));
    SendMessageW(HandleOf(window), WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(L"Something else"));
    EASYFORGE_EXPECT_EQUAL(log.OfType(EventType::ColorSchemeChanged).size(), std::size_t { 1 });
    // The background follows the system unless one was chosen.
    ColorScheme scheme = window.SystemColorScheme();
    EASYFORGE_EXPECT_EQUAL(window.Background.Get(), scheme == ColorScheme::Dark ? Color::Hex("#202024") : Color::Hex("#F3F3F3"));
    EASYFORGE_EXPECT_EQUAL(window.Background.Get(), before);
    window.Close();
}
