#include <easyforge/core/Testing.h>

#include <cmath>

#include "TestWindows.h"

using namespace easyforge;
using testwindows::HandleOf;
using testwindows::NewHidden;
using testwindows::Pump;

EASYFORGE_TEST(WindowOpensWithItsSettings)
{
    Window window = Window::New({
        .Title = "Notes ✓",
        .Width = 640,
        .Height = 480,
        .MinimumWidth = 200,
        .MinimumHeight = 100,
        .Visible = false,
        .Background = Color::Hex("#15151A"),
    });
    EASYFORGE_REQUIRE(window);
    EASYFORGE_EXPECT(window.Error().empty());
    EASYFORGE_EXPECT(window.IsOpen());
    EASYFORGE_EXPECT(!window.Visible);

    std::string title = window.Title;
    EASYFORGE_EXPECT_EQUAL(title, std::string("Notes ✓"));
    wchar_t text[64] {};
    GetWindowTextW(HandleOf(window), text, 64);
    EASYFORGE_EXPECT(std::wstring(text) == L"Notes ✓");

    // The content is the size asked for, in points, whatever the screen's scale.
    EASYFORGE_EXPECT_NEAR(window.Width.Get(), 640.0f, 0.5f);
    EASYFORGE_EXPECT_NEAR(window.Height.Get(), 480.0f, 0.5f);
    RECT client {};
    GetClientRect(HandleOf(window), &client);
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(client.right), static_cast<int>(std::lround(640 * window.Scale())));
    EASYFORGE_EXPECT_EQUAL(window.PixelSize(), Vector2(static_cast<float>(client.right), static_cast<float>(client.bottom)));

    EASYFORGE_EXPECT_EQUAL(window.MinimumWidth.Get(), 200.0f);
    EASYFORGE_EXPECT_EQUAL(window.Background.Get(), Color::Hex("#15151A"));
    EASYFORGE_EXPECT(window.Scale() >= 1.0f);
    EASYFORGE_EXPECT(!window.IsTransparent());

    Surface surface = window.NativeSurface();
    EASYFORGE_EXPECT(surface.Kind == Surface::Platform::Windows);
    EASYFORGE_EXPECT(surface.Handle != nullptr);
    EASYFORGE_EXPECT(surface.Connection == GetModuleHandleW(nullptr));
    window.Close();
}

EASYFORGE_TEST(WindowStartsCenteredOnThePrimaryScreen)
{
    Window window = NewHidden(400, 300);
    Monitor primary = Monitor::Primary();
    Vector2 position = window.Position;
    Vector2 pixels = window.PixelSize();
    float middle = position.X + pixels.X / 2;
    float workMiddle = primary.WorkArea.X + primary.WorkArea.Width / 2;
    // Centered by the whole window, frame included, so allow for the frame.
    EASYFORGE_EXPECT(std::abs(middle - workMiddle) < 20.0f);

    Window placed = Window::New({ .Width = 200, .Height = 100, .Position = Vector2 { 150, 120 }, .Visible = false });
    EASYFORGE_EXPECT_EQUAL(placed.Position.Get(), Vector2(150, 120));
    placed.Position = Vector2 { 170, 140 };
    EASYFORGE_EXPECT_EQUAL(placed.Position.Get(), Vector2(170, 140));
    window.Close();
    placed.Close();
}

EASYFORGE_TEST(WindowPropertiesChangeTheWindow)
{
    Window window = NewHidden();
    HWND handle = HandleOf(window);

    window.Title = "Renamed";
    wchar_t text[64] {};
    GetWindowTextW(handle, text, 64);
    EASYFORGE_EXPECT(std::wstring(text) == L"Renamed");

    window.Size = { 500, 250 };
    EASYFORGE_EXPECT_NEAR(window.Width.Get(), 500.0f, 0.5f);
    EASYFORGE_EXPECT_NEAR(window.Height.Get(), 250.0f, 0.5f);
    window.Width = 520;
    EASYFORGE_EXPECT_NEAR(window.Size->X, 520.0f, 0.5f);
    EASYFORGE_EXPECT_NEAR(window.Size->Y, 250.0f, 0.5f);

    window.Resizable = false;
    EASYFORGE_EXPECT((GetWindowLongPtrW(handle, GWL_STYLE) & WS_THICKFRAME) == 0);
    EASYFORGE_EXPECT_NEAR(window.Width.Get(), 520.0f, 0.5f);
    window.Resizable = true;
    EASYFORGE_EXPECT((GetWindowLongPtrW(handle, GWL_STYLE) & WS_THICKFRAME) != 0);

    // A hidden window remembers to be maximized when it shows.
    window.Maximized = true;
    EASYFORGE_EXPECT(window.Maximized);
    window.Maximized = false;
    EASYFORGE_EXPECT(!window.Maximized);

    window.Cursor = Cursor::Hand;
    EASYFORGE_EXPECT(window.Cursor == Cursor::Hand);
    window.VerticalSync = false;
    EASYFORGE_EXPECT(!window.VerticalSync);
    window.Icon = "a file that does not exist.png";
    std::string icon = window.Icon;
    EASYFORGE_EXPECT_EQUAL(icon, std::string("a file that does not exist.png"));
    window.Close();
}

EASYFORGE_TEST(WindowStaysOnTop)
{
    // Some setups never let a program's oldest window stay on top, so an older
    // window is made first.
    Window older = NewHidden();
    Window window = NewHidden();
    HWND handle = HandleOf(window);
    window.AlwaysOnTop = true;
    EASYFORGE_EXPECT(window.AlwaysOnTop);
    EASYFORGE_EXPECT((GetWindowLongPtrW(handle, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
    window.AlwaysOnTop = false;
    EASYFORGE_EXPECT(!window.AlwaysOnTop);
    EASYFORGE_EXPECT((GetWindowLongPtrW(handle, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0);

    Window fromSettings = Window::New({ .AlwaysOnTop = true, .Visible = false });
    EASYFORGE_EXPECT((GetWindowLongPtrW(HandleOf(fromSettings), GWL_EXSTYLE) & WS_EX_TOPMOST) != 0);
    fromSettings.Close();
    window.Close();
    older.Close();
}

EASYFORGE_TEST(WindowFullscreenCoversItsScreen)
{
    Window window = NewHidden(300, 200);
    window.Fullscreen = true;
    EASYFORGE_EXPECT(window.Fullscreen);
    Monitor monitor = window.Monitor();
    EASYFORGE_EXPECT_EQUAL(window.PixelSize(), monitor.Area.Size());
    EASYFORGE_EXPECT((GetWindowLongPtrW(HandleOf(window), GWL_STYLE) & WS_CAPTION) == 0);

    window.Fullscreen = false;
    EASYFORGE_EXPECT(!window.Fullscreen);
    EASYFORGE_EXPECT_NEAR(window.Width.Get(), 300.0f, 0.5f);
    EASYFORGE_EXPECT((GetWindowLongPtrW(HandleOf(window), GWL_STYLE) & WS_CAPTION) != 0);
    window.Close();
}

EASYFORGE_TEST(WindowShowsMaximizesAndMinimizes)
{
    // The one test that shows windows on screen.
    Window window = Window::New({ .Title = "easyforge test", .Width = 300, .Height = 200 });
    EASYFORGE_REQUIRE(window);
    Pump();
    EASYFORGE_EXPECT(window.Visible);

    window.Maximized = true;
    Pump();
    EASYFORGE_EXPECT(window.Maximized);
    EASYFORGE_EXPECT(window.AsHost()->Mode() == WindowMode::Maximized);
    window.Maximized = false;
    Pump();
    EASYFORGE_EXPECT(!window.Maximized);

    window.Minimized = true;
    Pump();
    EASYFORGE_EXPECT(window.Minimized);
    EASYFORGE_EXPECT(window.AsHost()->Mode() == WindowMode::Minimized);
    window.Minimized = false;
    Pump();
    EASYFORGE_EXPECT(!window.Minimized);
    EASYFORGE_EXPECT(window.AsHost()->Mode() == WindowMode::Normal);

    Window maximizedAtStart = Window::New({ .Width = 300, .Height = 200, .Maximized = true });
    Pump();
    EASYFORGE_EXPECT(maximizedAtStart.Maximized);
    EASYFORGE_EXPECT(IsZoomed(HandleOf(maximizedAtStart)) != FALSE);

    window.Visible = false;
    EASYFORGE_EXPECT(!window.Visible);
    window.Close();
    maximizedAtStart.Close();
}

EASYFORGE_TEST(WindowCloseRequestCanBeRefused)
{
    Window window = NewHidden();
    int asked = 0;
    bool allow = false;
    window.OnCloseRequested = [&] {
        ++asked;
        return allow;
    };

    PostMessageW(HandleOf(window), WM_CLOSE, 0, 0);
    Pump();
    EASYFORGE_EXPECT_EQUAL(asked, 1);
    EASYFORGE_EXPECT(window.IsOpen());

    allow = true;
    HWND handle = HandleOf(window);
    PostMessageW(handle, WM_CLOSE, 0, 0);
    Pump();
    EASYFORGE_EXPECT_EQUAL(asked, 2);
    EASYFORGE_EXPECT(!window.IsOpen());
    EASYFORGE_EXPECT(!IsWindow(handle));

    // Still a window that was made, but closed: everything is safe to call.
    EASYFORGE_EXPECT(window);
    EASYFORGE_EXPECT(window.NativeSurface().Handle == nullptr);
    window.Title = "After closing";
    window.Size = { 100, 100 };
    EASYFORGE_EXPECT_EQUAL(window.PixelSize(), Vector2());
    window.Close();
}

EASYFORGE_TEST(WindowClosesFromInsideItsOwnCallbacks)
{
    // Closing while Windows is inside one of the window's messages must wait to
    // free the window until the message is finished.
    Window fromEvent = NewHidden();
    fromEvent.OnEvent = [fromEvent](const Event& event) {
        if (event.Type == EventType::KeyPressed)
        {
            fromEvent.Close();
        }
    };
    PostMessageW(HandleOf(fromEvent), WM_KEYDOWN, 'W', testwindows::KeyParameter(0x11));
    Pump();
    EASYFORGE_EXPECT(!fromEvent.IsOpen());
    fromEvent.OnEvent = nullptr;

    Window fromFrame = NewHidden();
    int frames = 0;
    fromFrame.OnFrame = [&frames, fromFrame](float) {
        ++frames;
        fromFrame.Close();
    };
    Pump(3);
    EASYFORGE_EXPECT_EQUAL(frames, 1);
    EASYFORGE_EXPECT(!fromFrame.IsOpen());
    fromFrame.OnFrame = nullptr;
}

EASYFORGE_TEST(WindowStaysOpenWithoutHandles)
{
    int frames = 0;
    HWND handle = nullptr;
    {
        Window window = NewHidden();
        handle = HandleOf(window);
        window.OnFrame = [&frames](float) { ++frames; };
    }
    Pump(2);
    EASYFORGE_EXPECT_EQUAL(frames, 2);
    EASYFORGE_EXPECT(IsWindow(handle) != FALSE);

    PostMessageW(handle, WM_CLOSE, 0, 0);
    Pump(2);
    EASYFORGE_EXPECT_EQUAL(frames, 2);
    EASYFORGE_EXPECT(!IsWindow(handle));
}

EASYFORGE_TEST(WindowHandlesShareOneWindow)
{
    Window first = NewHidden();
    Window second = first;
    second.Title = "Shared";
    std::string title = first.Title;
    EASYFORGE_EXPECT_EQUAL(title, std::string("Shared"));

    Window third;
    EASYFORGE_EXPECT(!third);
    EASYFORGE_EXPECT(!third.IsOpen());
    third.Title = "Nothing happens";
    third = first;
    EASYFORGE_EXPECT(third);
    third.Title = "Through the third";
    title = second.Title;
    EASYFORGE_EXPECT_EQUAL(title, std::string("Through the third"));

    // Handles work from lambdas that captured them by value.
    auto rename = [first] { first.Title = "From a lambda"; };
    rename();
    title = third.Title;
    EASYFORGE_EXPECT_EQUAL(title, std::string("From a lambda"));

    second.Close();
    EASYFORGE_EXPECT(!first.IsOpen());
}

EASYFORGE_TEST(WindowRunsFramesUntilClosed)
{
    Window window = NewHidden();
    int frames = 0;
    float total = 0.0f;
    window.OnFrame = [&](float deltaSeconds) {
        EASYFORGE_EXPECT(deltaSeconds >= 0.0f);
        total += deltaSeconds;
        if (++frames == 5)
        {
            PostMessageW(HandleOf(window), WM_CLOSE, 0, 0);
        }
    };
    window.Run();
    EASYFORGE_EXPECT_EQUAL(frames, 5);
    EASYFORGE_EXPECT(!window.IsOpen());
    EASYFORGE_EXPECT(total > 0.0f);
    window.OnFrame = nullptr;
}

EASYFORGE_TEST(WindowIconFromFileAndFromProgram)
{
    // The test program is built with easyforge_app_icon, so a window without an
    // icon of its own uses the program's.
    Window plain = NewHidden();
    EASYFORGE_EXPECT(SendMessageW(HandleOf(plain), WM_GETICON, ICON_SMALL, 0) != 0);
    EASYFORGE_EXPECT(SendMessageW(HandleOf(plain), WM_GETICON, ICON_BIG, 0) != 0);

    Window withIcon = Window::New({ .Icon = EASYFORGE_TEST_ICON, .Visible = false });
    auto icon = reinterpret_cast<HICON>(SendMessageW(HandleOf(withIcon), WM_GETICON, ICON_BIG, 0));
    EASYFORGE_REQUIRE(icon != nullptr);
    ICONINFO information {};
    EASYFORGE_REQUIRE(GetIconInfo(icon, &information));
    BITMAP bitmap {};
    GetObjectW(information.hbmColor, sizeof(bitmap), &bitmap);
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(bitmap.bmWidth), GetSystemMetricsForDpi(SM_CXICON, GetDpiForWindow(HandleOf(withIcon))));
    DeleteObject(information.hbmColor);
    DeleteObject(information.hbmMask);

    ImageData red(8, 8);
    for (int y = 0; y < 8; ++y)
    {
        for (int x = 0; x < 8; ++x)
        {
            red.SetColorAt(x, y, Color::Hex("#FF0000"));
        }
    }
    withIcon.SetIcon(red);
    EASYFORGE_EXPECT(SendMessageW(HandleOf(withIcon), WM_GETICON, ICON_BIG, 0) != reinterpret_cast<LRESULT>(icon));
    withIcon.SetCursorImage(red, { 4, 4 });

    plain.Close();
    withIcon.Close();
}

EASYFORGE_TEST(WindowScaleChangeKeepsPoints)
{
    Window window = NewHidden(400, 300);
    testwindows::EventLog log;
    log.Follow(window);
    HWND handle = HandleOf(window);
    float original = window.Scale();
    UINT originalDpi = GetDpiForWindow(handle);

    // What Windows sends when the window moves to a screen at 150% of this one.
    UINT dpi = originalDpi * 3 / 2;
    SIZE size {};
    EASYFORGE_REQUIRE(SendMessageW(handle, WM_GETDPISCALEDSIZE, dpi, reinterpret_cast<LPARAM>(&size)));
    RECT windowArea {};
    GetWindowRect(handle, &windowArea);
    RECT suggested { windowArea.left, windowArea.top, windowArea.left + size.cx, windowArea.top + size.cy };
    SendMessageW(handle, WM_DPICHANGED, MAKEWPARAM(dpi, dpi), reinterpret_cast<LPARAM>(&suggested));

    // The size asked for keeps the content at 400 by 300 points at the new scale,
    // plus the frame at the new scale.
    float scale = static_cast<float>(dpi) / 96.0f;
    RECT frame {};
    AdjustWindowRectExForDpi(&frame, static_cast<DWORD>(GetWindowLongPtrW(handle, GWL_STYLE)), FALSE,
        static_cast<DWORD>(GetWindowLongPtrW(handle, GWL_EXSTYLE)), dpi);
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(size.cx), static_cast<int>(std::lround(400 * scale)) + (frame.right - frame.left));
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(size.cy), static_cast<int>(std::lround(300 * scale)) + (frame.bottom - frame.top));

    // The window takes the place Windows suggests, and reports the new scale.
    RECT now {};
    GetWindowRect(handle, &now);
    EASYFORGE_EXPECT(EqualRect(&now, &suggested) != FALSE);
    EASYFORGE_EXPECT_NEAR(window.Scale(), original * 1.5f, 0.01f);

    std::vector<Event> scaled = log.OfType(EventType::ScaleChanged);
    EASYFORGE_REQUIRE(scaled.size() == 1);
    EASYFORGE_EXPECT_NEAR(scaled[0].Scale, original * 1.5f, 0.01f);
    window.OnEvent = nullptr;
    window.Close();
}

EASYFORGE_TEST(WindowTransparentAndSurface)
{
    Window window = Window::New({ .Visible = false, .Transparent = true });
    EASYFORGE_REQUIRE(window);
    EASYFORGE_EXPECT(window.IsTransparent());
    EASYFORGE_EXPECT((GetWindowLongPtrW(HandleOf(window), GWL_EXSTYLE) & WS_EX_NOREDIRECTIONBITMAP) != 0);

    std::shared_ptr<Host> host = window;
    EASYFORGE_EXPECT(host->IsTransparent());
    EASYFORGE_EXPECT(host->ClaimSurface());
    EASYFORGE_EXPECT(!host->ClaimSurface());
    host->ReleaseSurface();
    EASYFORGE_EXPECT(host->ClaimSurface());
    host->ReleaseSurface();

    std::shared_ptr<void>& shared = host->Shared("easyforge.test");
    EASYFORGE_EXPECT(shared == nullptr);
    shared = std::make_shared<int>(7);
    EASYFORGE_EXPECT_EQUAL(*static_cast<int*>(host->Shared("easyforge.test").get()), 7);
    window.Close();
    EASYFORGE_EXPECT(host->Shared("easyforge.test") == nullptr);
    EASYFORGE_EXPECT(!host->ClaimSurface());
}

EASYFORGE_TEST(HiddenWindowsRunAboutSixtyFramesASecond)
{
    Window window = NewHidden();
    int frames = 0;
    window.OnFrame = [&frames](float) { ++frames; };
    Clock clock;
    while (clock.Seconds() < 0.5)
    {
        Pump();
    }
    // Loose bounds, since a busy computer can delay any frame.
    EASYFORGE_EXPECT(frames >= 20);
    EASYFORGE_EXPECT(frames <= 45);
    window.Close();
}
