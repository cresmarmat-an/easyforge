#include <easyforge/core/Testing.h>

#include <cmath>
#include <memory>

#include "TestWindows.h"

using namespace easyforge;
using testwindows::HandleOf;
using testwindows::KeyParameter;
using testwindows::NewHidden;
using testwindows::Pump;

namespace
{
    // A view that writes down everything the window asks of it.
    class RecordingView final : public View
    {
    public:
        RecordingView(std::string name, std::vector<std::string>& log) : Name(std::move(name)), Log(log) {}

        void Attach(Host& host) override
        {
            AttachedTo = &host;
            Log.push_back(Name + " attached");
        }

        void Detach() override
        {
            AttachedTo = nullptr;
            Log.push_back(Name + " detached");
        }

        void Place(easyforge::Rectangle area) override { Area = area; }

        void HandleEvent(Event& event) override
        {
            if (event.Type == EventType::KeyPressed)
            {
                Log.push_back(Name + " saw a key");
                event.Handled = event.Handled || MarksHandled;
            }
        }

        void Frame(float) override { Log.push_back(Name + " frame"); }

        Vector2 PreferredSize(Vector2 available) const override { return { available.X, PreferredHeight }; }

        // A title bar 40 points tall: a close button at the right end, a
        // maximize button left of it, and everything else dragging the window.
        HitArea HitTest(Vector2 point) const override
        {
            if (point.Y >= 40)
            {
                return HitArea::Content;
            }
            if (point.X >= Area.Width - 40)
            {
                return HitArea::CloseButton;
            }
            if (point.X >= Area.Width - 80)
            {
                return HitArea::MaximizeButton;
            }
            if (point.X < 30)
            {
                return HitArea::Content;
            }
            return HitArea::Caption;
        }

        std::string Name;
        std::vector<std::string>& Log;
        Host* AttachedTo = nullptr;
        easyforge::Rectangle Area;
        float PreferredHeight = 0.0f;
        bool MarksHandled = false;
    };

    class RecordingListener final : public HostListener
    {
    public:
        explicit RecordingListener(std::vector<std::string>& log) : Log(log) {}

        void HandleEvent(const Event& event) override
        {
            if (event.Type == EventType::KeyPressed)
            {
                Log.push_back(event.Handled ? "listener saw a handled key" : "listener saw a key");
            }
        }

        void FrameStarted(float) override { Log.push_back("listener frame started"); }
        void FrameEnded() override { Log.push_back("listener frame ended"); }

        std::vector<std::string>& Log;
    };

    LPARAM ScreenPoint(HWND handle, float x, float y, float scale)
    {
        POINT point { static_cast<LONG>(std::lround(x * scale)), static_cast<LONG>(std::lround(y * scale)) };
        ClientToScreen(handle, &point);
        return MAKELPARAM(point.x, point.y);
    }
}

EASYFORGE_TEST(ViewsAreAttachedPlacedAndDetached)
{
    std::vector<std::string> log;
    Window window = NewHidden(400, 300);
    auto content = std::make_shared<RecordingView>("content", log);

    window.Content = content;
    EASYFORGE_EXPECT(content->AttachedTo != nullptr);
    EASYFORGE_EXPECT(window.Content.Get() == content);
    EASYFORGE_EXPECT_NEAR(content->Area.Width, 400.0f, 0.5f);
    EASYFORGE_EXPECT_NEAR(content->Area.Height, 300.0f, 0.5f);
    EASYFORGE_EXPECT_EQUAL(content->Area.Y, 0.0f);
    EASYFORGE_EXPECT_NEAR(content->AttachedTo->Size().X, 400.0f, 0.5f);

    // Resizing places the view again.
    window.Size = { 420, 310 };
    EASYFORGE_EXPECT_NEAR(content->Area.Width, 420.0f, 0.5f);

    auto replacement = std::make_shared<RecordingView>("replacement", log);
    window.Content = replacement;
    EASYFORGE_EXPECT(content->AttachedTo == nullptr);
    EASYFORGE_EXPECT(replacement->AttachedTo != nullptr);

    window.Close();
    EASYFORGE_EXPECT(replacement->AttachedTo == nullptr);
    std::vector<std::string> expected = {
        "content attached", "content detached", "replacement attached", "replacement detached" };
    EASYFORGE_EXPECT(log == expected);
}

EASYFORGE_TEST(EventsGoToTitleBarContentListenersThenOnEvent)
{
    std::vector<std::string> log;
    Window window = NewHidden();
    auto titleBar = std::make_shared<RecordingView>("title bar", log);
    titleBar->PreferredHeight = 40;
    auto content = std::make_shared<RecordingView>("content", log);
    content->MarksHandled = true;
    RecordingListener listener(log);

    window.TitleBar = titleBar;
    window.Content = content;
    window.AsHost()->AddListener(listener);
    window.OnEvent = [&log](const Event& event) {
        if (event.Type == EventType::KeyPressed)
        {
            log.push_back("OnEvent saw a key");
        }
    };
    window.OnFrame = [&log](float) { log.push_back("OnFrame"); };
    log.clear();

    PostMessageW(HandleOf(window), WM_KEYDOWN, 'W', KeyParameter(0x11));
    Pump();

    std::vector<std::string> expected = {
        "title bar saw a key",
        "content saw a key",
        "listener saw a handled key",
        "OnEvent saw a key",
        "listener frame started",
        "OnFrame",
        "title bar frame",
        "content frame",
        "listener frame ended",
    };
    EASYFORGE_EXPECT(log == expected);

    // A removed listener hears nothing more.
    window.AsHost()->RemoveListener(listener);
    log.clear();
    Pump();
    expected = { "OnFrame", "title bar frame", "content frame" };
    EASYFORGE_EXPECT(log == expected);
    window.OnEvent = nullptr;
    window.OnFrame = nullptr;
    window.Close();
}

EASYFORGE_TEST(ListenerRemovedDuringAnEventIsSafe)
{
    std::vector<std::string> log;
    Window window = NewHidden();
    auto second = std::make_unique<RecordingListener>(log);

    class RemovingListener final : public HostListener
    {
    public:
        RemovingListener(std::shared_ptr<Host> host, std::unique_ptr<RecordingListener>& other)
            : TheHost(std::move(host)), Other(other)
        {
        }
        void HandleEvent(const Event&) override
        {
            if (Other)
            {
                TheHost->RemoveListener(*Other);
                Other.reset();
            }
        }
        void FrameStarted(float) override {}
        void FrameEnded() override {}

        std::shared_ptr<Host> TheHost;
        std::unique_ptr<RecordingListener>& Other;
    };

    RemovingListener first(window.AsHost(), second);
    window.AsHost()->AddListener(first);
    window.AsHost()->AddListener(*second);
    PostMessageW(HandleOf(window), WM_KEYDOWN, 'W', KeyParameter(0x11));
    Pump(2);
    EASYFORGE_EXPECT(second == nullptr);
    EASYFORGE_EXPECT(log.empty());
    window.AsHost()->RemoveListener(first);
    window.Close();
}

EASYFORGE_TEST(CustomTitleBarReplacesTheSystemOne)
{
    std::vector<std::string> log;
    Window window = NewHidden(400, 300);
    HWND handle = HandleOf(window);
    float scale = window.Scale();

    auto titleBar = std::make_shared<RecordingView>("title bar", log);
    titleBar->PreferredHeight = 40;
    auto content = std::make_shared<RecordingView>("content", log);
    window.TitleBar = titleBar;
    window.Content = content;

    // The content now starts at the top edge of the window, and keeps its size.
    RECT frame {};
    GetWindowRect(handle, &frame);
    POINT corner { 0, 0 };
    ClientToScreen(handle, &corner);
    EASYFORGE_EXPECT_EQUAL(static_cast<int>(corner.y), static_cast<int>(frame.top));
    EASYFORGE_EXPECT_NEAR(window.Width.Get(), 400.0f, 0.5f);
    EASYFORGE_EXPECT_NEAR(window.Height.Get(), 300.0f, 0.5f);

    // The title bar takes the height it asked for, and the content the rest.
    EASYFORGE_EXPECT_EQUAL(titleBar->Area.Height, 40.0f);
    EASYFORGE_EXPECT_EQUAL(content->Area.Y, 40.0f);
    EASYFORGE_EXPECT_NEAR(content->Area.Height, 260.0f, 0.5f);

    auto hit = [&](float x, float y) { return SendMessageW(handle, WM_NCHITTEST, 0, ScreenPoint(handle, x, y, scale)); };
    EASYFORGE_EXPECT_EQUAL(hit(200, 20), LRESULT { HTCAPTION });
    EASYFORGE_EXPECT_EQUAL(hit(390, 20), LRESULT { HTCLOSE });
    EASYFORGE_EXPECT_EQUAL(hit(350, 20), LRESULT { HTMAXBUTTON });
    EASYFORGE_EXPECT_EQUAL(hit(10, 20), LRESULT { HTCLIENT });
    EASYFORGE_EXPECT_EQUAL(hit(200, 150), LRESULT { HTCLIENT });
    // The top edge still resizes the window.
    EASYFORGE_EXPECT_EQUAL(hit(200, 0), LRESULT { HTTOP });
    EASYFORGE_EXPECT_EQUAL(hit(0, 0), LRESULT { HTTOPLEFT });

    // Taking the title bar away brings the system's back.
    window.TitleBar = nullptr;
    GetWindowRect(handle, &frame);
    corner = { 0, 0 };
    ClientToScreen(handle, &corner);
    EASYFORGE_EXPECT(corner.y > frame.top);
    EASYFORGE_EXPECT_EQUAL(content->Area.Y, 0.0f);
    EASYFORGE_EXPECT_NEAR(window.Height.Get(), 300.0f, 0.5f);
    EASYFORGE_EXPECT_EQUAL(hit(200, 20), LRESULT { HTCLIENT });
    window.Close();
}

EASYFORGE_TEST(CustomTitleBarButtonsActOnRelease)
{
    std::vector<std::string> log;
    Window window = NewHidden(400, 300);
    HWND handle = HandleOf(window);
    float scale = window.Scale();
    auto titleBar = std::make_shared<RecordingView>("title bar", log);
    titleBar->PreferredHeight = 40;
    window.TitleBar = titleBar;
    testwindows::EventLog events;
    events.Follow(window);
    int closeRequests = 0;
    window.OnCloseRequested = [&closeRequests] {
        ++closeRequests;
        return false;
    };

    // Pressing and releasing on the close button asks to close; the view sees
    // the press and release so it can draw them.
    PostMessageW(handle, WM_NCLBUTTONDOWN, HTCLOSE, ScreenPoint(handle, 390, 20, scale));
    PostMessageW(handle, WM_NCLBUTTONUP, HTCLOSE, ScreenPoint(handle, 390, 20, scale));
    Pump(2);
    EASYFORGE_EXPECT_EQUAL(closeRequests, 1);
    EASYFORGE_EXPECT_EQUAL(events.OfType(EventType::MouseButtonPressed).size(), std::size_t { 1 });
    EASYFORGE_EXPECT_EQUAL(events.OfType(EventType::MouseButtonReleased).size(), std::size_t { 1 });

    // Pressing on one button and releasing on another does nothing.
    PostMessageW(handle, WM_NCLBUTTONDOWN, HTMAXBUTTON, ScreenPoint(handle, 350, 20, scale));
    PostMessageW(handle, WM_NCLBUTTONUP, HTCLOSE, ScreenPoint(handle, 390, 20, scale));
    Pump(2);
    EASYFORGE_EXPECT_EQUAL(closeRequests, 1);
    window.OnCloseRequested = nullptr;
    window.OnEvent = nullptr;
    window.Close();
}

EASYFORGE_TEST(TitleBarHiddenInFullscreen)
{
    std::vector<std::string> log;
    Window window = NewHidden(400, 300);
    auto titleBar = std::make_shared<RecordingView>("title bar", log);
    titleBar->PreferredHeight = 40;
    auto content = std::make_shared<RecordingView>("content", log);
    window.TitleBar = titleBar;
    window.Content = content;

    window.Fullscreen = true;
    EASYFORGE_EXPECT_EQUAL(titleBar->Area.Height, 0.0f);
    EASYFORGE_EXPECT_EQUAL(content->Area.Y, 0.0f);
    log.clear();
    Pump();
    EASYFORGE_EXPECT(log == std::vector<std::string> { "content frame" });

    window.Fullscreen = false;
    EASYFORGE_EXPECT_EQUAL(titleBar->Area.Height, 40.0f);
    EASYFORGE_EXPECT_NEAR(window.Height.Get(), 300.0f, 0.5f);
    window.Close();
}
