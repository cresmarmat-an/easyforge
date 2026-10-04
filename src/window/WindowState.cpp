#include "WindowState.h"

#include <algorithm>

#include <easyforge/assets/ImageData.h>
#include <easyforge/core/Log.h>

namespace easyforge::internal
{
    std::vector<std::shared_ptr<WindowState>>& OpenWindows()
    {
        static std::vector<std::shared_ptr<WindowState>> windows;
        return windows;
    }

    void RunFramesOfOpenWindows()
    {
        // Frames can open and close windows, so work on a copy.
        std::vector<std::shared_ptr<WindowState>> windows = OpenWindows();
        for (const std::shared_ptr<WindowState>& window : windows)
        {
            window->RunFrame();
        }
    }

    Color DefaultBackground(ColorScheme scheme)
    {
        return scheme == ColorScheme::Dark ? Color::Hex("#202024") : Color::Hex("#F3F3F3");
    }

    Result<> WindowState::Open(const WindowSettings& settings)
    {
        Title = settings.Title;
        IconPath = settings.Icon;
        MinimumSize = { Max(settings.MinimumWidth, 0.0f), Max(settings.MinimumHeight, 0.0f) };
        Resizable = settings.Resizable;
        AlwaysOnTop = settings.AlwaysOnTop;
        Transparent = settings.Transparent;
        VerticalSyncEnabled = settings.VerticalSync;
        ChosenBackground = settings.Background;
        Background = ChosenBackground ? *ChosenBackground : DefaultBackground(PlatformColorScheme());

        Result<PlatformWindowPointer> platform = CreatePlatformWindow(*this, settings);
        if (!platform)
        {
            return Failure(platform.Error());
        }
        Platform = std::move(platform).Get();
        Made = true;
        OpenWindows().push_back(shared_from_this());
        return {};
    }

    void WindowState::Close()
    {
        if (!Platform)
        {
            return;
        }
        std::shared_ptr<WindowState> keepAlive = shared_from_this();

        if (std::shared_ptr<View> view = std::move(TitleBarView))
        {
            view->Detach();
        }
        if (std::shared_ptr<View> view = std::move(ContentView))
        {
            view->Detach();
        }
        SharedObjects.clear();

        // A closed window is never called again, so let go of the callbacks. A
        // callback that captured the window's own handle would otherwise keep it
        // alive forever.
        FrameCallback = nullptr;
        EventCallback = nullptr;
        CloseCallback = nullptr;

        Platform.reset();

        std::vector<std::shared_ptr<WindowState>>& windows = OpenWindows();
        windows.erase(std::remove(windows.begin(), windows.end(), keepAlive), windows.end());
    }

    Surface WindowState::NativeSurface() const
    {
        return Platform ? Platform->NativeSurface() : Surface {};
    }

    Vector2 WindowState::Size() const
    {
        return Platform ? Platform->ContentSize() : Vector2 {};
    }

    Vector2 WindowState::PixelSize() const
    {
        return Platform ? Platform->PixelSize() : Vector2 {};
    }

    float WindowState::Scale() const
    {
        return Platform ? Platform->Scale() : 1.0f;
    }

    bool WindowState::IsFocused() const
    {
        return Platform && Platform->IsFocused();
    }

    WindowMode WindowState::Mode() const
    {
        if (!Platform)
        {
            return WindowMode::Normal;
        }
        if (Platform->IsFullscreen())
        {
            return WindowMode::Fullscreen;
        }
        if (Platform->IsMinimized())
        {
            return WindowMode::Minimized;
        }
        return Platform->IsMaximized() ? WindowMode::Maximized : WindowMode::Normal;
    }

    bool WindowState::ClaimSurface()
    {
        if (!Platform || SurfaceClaimed)
        {
            return false;
        }
        SurfaceClaimed = true;
        Platform->SurfaceClaimed(true);
        return true;
    }

    void WindowState::ReleaseSurface()
    {
        if (!SurfaceClaimed)
        {
            return;
        }
        SurfaceClaimed = false;
        if (Platform)
        {
            Platform->SurfaceClaimed(false);
        }
    }

    void WindowState::SetCursor(Cursor cursor)
    {
        CurrentCursor = cursor;
        if (Platform)
        {
            Platform->SetCursor(cursor);
        }
    }

    void WindowState::SetTextInput(bool enabled, Rectangle caret)
    {
        if (Platform)
        {
            Platform->SetTextInput(enabled, caret);
        }
    }

    ColorScheme WindowState::SystemColorScheme() const
    {
        return PlatformColorScheme();
    }

    std::string WindowState::ClipboardText() const
    {
        return PlatformClipboardText();
    }

    void WindowState::SetClipboardText(std::string_view text)
    {
        SetPlatformClipboardText(text);
    }

    void WindowState::AddListener(HostListener& listener)
    {
        if (std::find(Listeners.begin(), Listeners.end(), &listener) == Listeners.end())
        {
            Listeners.push_back(&listener);
        }
    }

    void WindowState::RemoveListener(HostListener& listener)
    {
        auto found = std::find(Listeners.begin(), Listeners.end(), &listener);
        if (found == Listeners.end())
        {
            return;
        }
        if (Dispatching > 0)
        {
            *found = nullptr;
        }
        else
        {
            Listeners.erase(found);
        }
    }

    void WindowState::RemoveFinishedListeners()
    {
        if (Dispatching == 0)
        {
            Listeners.erase(std::remove(Listeners.begin(), Listeners.end(), nullptr), Listeners.end());
        }
    }

    std::shared_ptr<void>& WindowState::Shared(std::string_view name)
    {
        auto found = SharedObjects.find(name);
        if (found == SharedObjects.end())
        {
            found = SharedObjects.emplace(std::string(name), nullptr).first;
        }
        return found->second;
    }

    bool WindowState::TitleBarShown() const
    {
        return TitleBarView && Platform && !Platform->IsFullscreen();
    }

    void WindowState::PlaceViews()
    {
        if (!Platform)
        {
            return;
        }
        Vector2 size = Platform->ContentSize();
        float titleBarHeight = 0.0f;
        if (TitleBarView)
        {
            if (TitleBarShown())
            {
                titleBarHeight = Clamp(TitleBarView->PreferredSize(size).Y, 0.0f, size.Y);
            }
            TitleBarView->Place({ 0.0f, 0.0f, size.X, titleBarHeight });
            TitleBarHeight = titleBarHeight;
        }
        if (ContentView)
        {
            ContentView->Place({ 0.0f, titleBarHeight, size.X, size.Y - titleBarHeight });
        }
    }

    void WindowState::Report(Event& event)
    {
        std::shared_ptr<WindowState> keepAlive = shared_from_this();

        if (event.Type == EventType::Resized || event.Type == EventType::ScaleChanged)
        {
            PlaceViews();
        }
        if (event.Type == EventType::ColorSchemeChanged && !ChosenBackground && Platform)
        {
            Background = DefaultBackground(PlatformColorScheme());
            Platform->SetBackground(Background);
        }

        std::shared_ptr<View> titleBar = TitleBarShown() ? TitleBarView : nullptr;
        std::shared_ptr<View> content = ContentView;
        if (titleBar)
        {
            titleBar->HandleEvent(event);
        }
        if (content)
        {
            content->HandleEvent(event);
        }

        ++Dispatching;
        std::size_t count = Listeners.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            if (HostListener* listener = Listeners[index])
            {
                listener->HandleEvent(event);
            }
        }
        --Dispatching;
        RemoveFinishedListeners();

        if (EventCallback)
        {
            std::function<void(const Event&)> callback = EventCallback;
            callback(event);
        }
    }

    void WindowState::ReportCloseRequest()
    {
        std::shared_ptr<WindowState> keepAlive = shared_from_this();
        Event event;
        event.Type = EventType::CloseRequested;
        Report(event);

        bool close = true;
        if (CloseCallback)
        {
            std::function<bool()> callback = CloseCallback;
            close = callback();
        }
        if (close)
        {
            Close();
        }
    }

    HitArea WindowState::TitleBarHitTest(Vector2 point) const
    {
        // Below the title bar is the content, whatever the title bar's view says.
        if (!TitleBarShown() || point.Y >= TitleBarHeight)
        {
            return HitArea::Content;
        }
        return TitleBarView->HitTest(point);
    }

    WindowState::~WindowState()
    {
        // A window still open when the program ends is destroyed without Close.
        // Its views go first, while the rest of the window is there for them.
        if (std::shared_ptr<View> view = std::move(TitleBarView))
        {
            view->Detach();
        }
        if (std::shared_ptr<View> view = std::move(ContentView))
        {
            view->Detach();
        }
        SharedObjects.clear();
    }

    void WindowState::RunFrame()
    {
        if (!Platform || InFrame)
        {
            return;
        }
        std::shared_ptr<WindowState> keepAlive = shared_from_this();
        InFrame = true;
        if (PlacementWanted)
        {
            PlacementWanted = false;
            PlaceViews();
        }
        float delta = static_cast<float>(FrameClock.Restart());

        ++Dispatching;
        std::size_t count = Listeners.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            if (HostListener* listener = Listeners[index])
            {
                listener->FrameStarted(delta);
            }
        }
        --Dispatching;

        if (FrameCallback)
        {
            std::function<void(float)> callback = FrameCallback;
            callback(delta);
        }

        std::shared_ptr<View> titleBar = TitleBarShown() ? TitleBarView : nullptr;
        std::shared_ptr<View> content = ContentView;
        if (titleBar && Platform)
        {
            titleBar->Frame(delta);
        }
        if (content && Platform)
        {
            content->Frame(delta);
        }

        ++Dispatching;
        count = Listeners.size();
        for (std::size_t index = 0; index < count; ++index)
        {
            if (HostListener* listener = Listeners[index])
            {
                listener->FrameEnded();
            }
        }
        --Dispatching;
        RemoveFinishedListeners();
        InFrame = false;
    }

    void WindowState::SetContent(std::shared_ptr<View> view)
    {
        if (view == ContentView)
        {
            return;
        }
        std::shared_ptr<WindowState> keepAlive = shared_from_this();
        if (std::shared_ptr<View> old = std::move(ContentView))
        {
            old->Detach();
        }
        if (!Platform)
        {
            return;
        }
        ContentView = std::move(view);
        if (ContentView)
        {
            ContentView->Attach(*this);
            PlaceViews();
        }
    }

    void WindowState::SetTitleBar(std::shared_ptr<View> view)
    {
        if (view == TitleBarView)
        {
            return;
        }
        std::shared_ptr<WindowState> keepAlive = shared_from_this();
        if (std::shared_ptr<View> old = std::move(TitleBarView))
        {
            old->Detach();
        }
        if (!Platform)
        {
            return;
        }
        TitleBarView = std::move(view);
        Platform->SetCustomTitleBar(TitleBarView != nullptr);
        if (TitleBarView)
        {
            TitleBarView->Attach(*this);
        }
        PlaceViews();
    }

    void WindowState::SetBackground(std::optional<Color> color)
    {
        ChosenBackground = color;
        Background = color ? *color : DefaultBackground(PlatformColorScheme());
        if (Platform)
        {
            Platform->SetBackground(Background);
        }
    }

    void WindowState::SetFullscreen(bool fullscreen)
    {
        if (!Platform || Platform->IsFullscreen() == fullscreen)
        {
            return;
        }
        Platform->SetFullscreen(fullscreen);
        PlaceViews();
    }
}
