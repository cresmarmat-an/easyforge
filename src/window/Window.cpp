#include <easyforge/window/Window.h>

#include <easyforge/assets/ImageData.h>
#include <easyforge/core/Log.h>

#include "WindowState.h"

namespace easyforge
{
    namespace
    {
        using internal::WindowState;

        WindowState& StateOf(void* owner)
        {
            return *static_cast<WindowState*>(owner);
        }

        const WindowState& StateOf(const void* owner)
        {
            return *static_cast<const WindowState*>(owner);
        }

        void ApplyIcon(WindowState& state)
        {
            if (!state.Platform)
            {
                return;
            }
            if (state.IconPath.empty())
            {
                state.Platform->SetIcon(ImageData());
                return;
            }
            ImageData image = ImageData::Load(state.IconPath);
            if (!image)
            {
                Log(LogLevel::Warning, "The window icon was not changed: {}", image.Error());
                return;
            }
            state.Platform->SetIcon(image);
        }

        Vector2 CurrentSize(const WindowState& state)
        {
            return state.Platform ? state.Platform->ContentSize() : Vector2 {};
        }

        void SetSize(WindowState& state, Vector2 size)
        {
            if (state.Platform)
            {
                state.Platform->SetContentSize({ Max(size.X, 1.0f), Max(size.Y, 1.0f) });
            }
        }

        void SetMinimum(WindowState& state, Vector2 minimum)
        {
            state.MinimumSize = { Max(minimum.X, 0.0f), Max(minimum.Y, 0.0f) };
            if (state.Platform)
            {
                state.Platform->SetMinimumSize(state.MinimumSize);
            }
        }

        std::shared_ptr<WindowState> EmptyState()
        {
            return std::make_shared<WindowState>();
        }
    }

    Window Window::New(const WindowSettings& settings)
    {
        std::shared_ptr<WindowState> state = std::make_shared<WindowState>();
        Result<> opened = state->Open(settings);
        if (!opened)
        {
            state->ErrorText = opened.Error();
            return Window(state);
        }

        ApplyIcon(*state);
        state->Platform->SetBackground(state->Background);
        if (settings.Fullscreen)
        {
            state->SetFullscreen(true);
        }
        if (settings.Maximized)
        {
            state->Platform->SetMaximized(true);
        }
        if (settings.Visible)
        {
            state->Platform->SetVisible(true);
        }
        return Window(state);
    }

    void Window::RunOneFrame()
    {
        internal::HandlePlatformMessages();
        internal::RunFramesOfOpenWindows();

        // Pace the loop: a renderer waiting for the screen does it on its own;
        // otherwise wait for the screen here, or for messages when nothing shows.
        bool anyShown = false;
        bool rendererPaces = false;
        bool waitForScreen = false;
        for (const std::shared_ptr<WindowState>& window : internal::OpenWindows())
        {
            if (window->Platform->IsVisible() && !window->Platform->IsMinimized())
            {
                anyShown = true;
                rendererPaces = rendererPaces || window->SurfaceClaimed;
                waitForScreen = waitForScreen || window->VerticalSyncEnabled;
            }
        }
        if (internal::OpenWindows().empty())
        {
            return;
        }
        if (!anyShown)
        {
            internal::WaitForMessages(1.0f / 60.0f);
        }
        else if (waitForScreen && !rendererPaces)
        {
            internal::WaitForVerticalBlank();
        }
    }

    Window::Window() : Window(EmptyState())
    {
    }

    Window::Window(const Window& other) : Window(other.State)
    {
    }

    Window& Window::operator=(const Window& other)
    {
        if (this != &other)
        {
            State = other.State;
            RebindProperties();
        }
        return *this;
    }

    Window::~Window() = default;

    Window::Window(std::shared_ptr<internal::WindowState> state)
        : Title(state.get(),
              [](const void* owner) { return StateOf(owner).Title; },
              [](void* owner, const std::string& value) {
                  WindowState& state = StateOf(owner);
                  state.Title = value;
                  if (state.Platform)
                  {
                      state.Platform->SetTitle(value);
                  }
              }),
          Icon(state.get(),
              [](const void* owner) { return StateOf(owner).IconPath; },
              [](void* owner, const std::string& value) {
                  WindowState& state = StateOf(owner);
                  state.IconPath = value;
                  ApplyIcon(state);
              }),
          Width(state.get(),
              [](const void* owner) { return CurrentSize(StateOf(owner)).X; },
              [](void* owner, const float& value) {
                  WindowState& state = StateOf(owner);
                  SetSize(state, { value, CurrentSize(state).Y });
              }),
          Height(state.get(),
              [](const void* owner) { return CurrentSize(StateOf(owner)).Y; },
              [](void* owner, const float& value) {
                  WindowState& state = StateOf(owner);
                  SetSize(state, { CurrentSize(state).X, value });
              }),
          Size(state.get(),
              [](const void* owner) { return CurrentSize(StateOf(owner)); },
              [](void* owner, const Vector2& value) { SetSize(StateOf(owner), value); }),
          MinimumWidth(state.get(),
              [](const void* owner) { return StateOf(owner).MinimumSize.X; },
              [](void* owner, const float& value) {
                  WindowState& state = StateOf(owner);
                  SetMinimum(state, { value, state.MinimumSize.Y });
              }),
          MinimumHeight(state.get(),
              [](const void* owner) { return StateOf(owner).MinimumSize.Y; },
              [](void* owner, const float& value) {
                  WindowState& state = StateOf(owner);
                  SetMinimum(state, { state.MinimumSize.X, value });
              }),
          Position(state.get(),
              [](const void* owner) {
                  const WindowState& state = StateOf(owner);
                  return state.Platform ? state.Platform->Position() : Vector2 {};
              },
              [](void* owner, const Vector2& value) {
                  WindowState& state = StateOf(owner);
                  if (state.Platform)
                  {
                      state.Platform->SetPosition(value);
                  }
              }),
          Resizable(state.get(),
              [](const void* owner) { return StateOf(owner).Resizable; },
              [](void* owner, const bool& value) {
                  WindowState& state = StateOf(owner);
                  state.Resizable = value;
                  if (state.Platform)
                  {
                      state.Platform->SetResizable(value);
                  }
              }),
          Maximized(state.get(),
              [](const void* owner) {
                  const WindowState& state = StateOf(owner);
                  return state.Platform && state.Platform->IsMaximized();
              },
              [](void* owner, const bool& value) {
                  WindowState& state = StateOf(owner);
                  if (state.Platform)
                  {
                      state.Platform->SetMaximized(value);
                  }
              }),
          Minimized(state.get(),
              [](const void* owner) {
                  const WindowState& state = StateOf(owner);
                  return state.Platform && state.Platform->IsMinimized();
              },
              [](void* owner, const bool& value) {
                  WindowState& state = StateOf(owner);
                  if (state.Platform)
                  {
                      state.Platform->SetMinimized(value);
                  }
              }),
          Fullscreen(state.get(),
              [](const void* owner) {
                  const WindowState& state = StateOf(owner);
                  return state.Platform && state.Platform->IsFullscreen();
              },
              [](void* owner, const bool& value) { StateOf(owner).SetFullscreen(value); }),
          AlwaysOnTop(state.get(),
              [](const void* owner) { return StateOf(owner).AlwaysOnTop; },
              [](void* owner, const bool& value) {
                  WindowState& state = StateOf(owner);
                  state.AlwaysOnTop = value;
                  if (state.Platform)
                  {
                      state.Platform->SetAlwaysOnTop(value);
                  }
              }),
          Visible(state.get(),
              [](const void* owner) {
                  const WindowState& state = StateOf(owner);
                  return state.Platform && state.Platform->IsVisible();
              },
              [](void* owner, const bool& value) {
                  WindowState& state = StateOf(owner);
                  if (state.Platform)
                  {
                      state.Platform->SetVisible(value);
                  }
              }),
          VerticalSync(state.get(),
              [](const void* owner) { return StateOf(owner).VerticalSyncEnabled; },
              [](void* owner, const bool& value) { StateOf(owner).VerticalSyncEnabled = value; }),
          Background(state.get(),
              [](const void* owner) { return StateOf(owner).Background; },
              [](void* owner, const Color& value) { StateOf(owner).SetBackground(value); }),
          Cursor(state.get(),
              [](const void* owner) { return StateOf(owner).CurrentCursor; },
              [](void* owner, const easyforge::Cursor& value) { StateOf(owner).SetCursor(value); }),
          MouseLocked(state.get(),
              [](const void* owner) { return StateOf(owner).MouseLocked; },
              [](void* owner, const bool& value) {
                  WindowState& state = StateOf(owner);
                  state.MouseLocked = value;
                  if (state.Platform)
                  {
                      state.Platform->SetMouseLocked(value);
                  }
              }),
          Content(state.get(),
              [](const void* owner) { return StateOf(owner).ContentView; },
              [](void* owner, const std::shared_ptr<View>& value) { StateOf(owner).SetContent(value); }),
          TitleBar(state.get(),
              [](const void* owner) { return StateOf(owner).TitleBarView; },
              [](void* owner, const std::shared_ptr<View>& value) { StateOf(owner).SetTitleBar(value); }),
          OnFrame(state.get(),
              [](const void* owner) { return StateOf(owner).FrameCallback; },
              [](void* owner, const std::function<void(float)>& value) { StateOf(owner).FrameCallback = value; }),
          OnEvent(state.get(),
              [](const void* owner) { return StateOf(owner).EventCallback; },
              [](void* owner, const std::function<void(const Event&)>& value) {
                  StateOf(owner).EventCallback = value;
              }),
          OnCloseRequested(state.get(),
              [](const void* owner) { return StateOf(owner).CloseCallback; },
              [](void* owner, const std::function<bool()>& value) { StateOf(owner).CloseCallback = value; }),
          State(std::move(state))
    {
    }

    void Window::RebindProperties()
    {
        void* owner = State.get();
        Title.Rebind(owner);
        Icon.Rebind(owner);
        Width.Rebind(owner);
        Height.Rebind(owner);
        Size.Rebind(owner);
        MinimumWidth.Rebind(owner);
        MinimumHeight.Rebind(owner);
        Position.Rebind(owner);
        Resizable.Rebind(owner);
        Maximized.Rebind(owner);
        Minimized.Rebind(owner);
        Fullscreen.Rebind(owner);
        AlwaysOnTop.Rebind(owner);
        Visible.Rebind(owner);
        VerticalSync.Rebind(owner);
        Background.Rebind(owner);
        Cursor.Rebind(owner);
        MouseLocked.Rebind(owner);
        Content.Rebind(owner);
        TitleBar.Rebind(owner);
        OnFrame.Rebind(owner);
        OnEvent.Rebind(owner);
        OnCloseRequested.Rebind(owner);
    }

    Window::operator bool() const
    {
        return State->Made;
    }

    const std::string& Window::Error() const
    {
        return State->ErrorText;
    }

    void Window::Run() const
    {
        std::shared_ptr<WindowState> state = State;
        while (state->IsOpen())
        {
            RunOneFrame();
        }
    }

    bool Window::IsOpen() const
    {
        return State->IsOpen();
    }

    void Window::Close() const
    {
        State->Close();
    }

    void Window::Focus() const
    {
        if (State->Platform)
        {
            State->Platform->Focus();
        }
    }

    bool Window::IsFocused() const
    {
        return State->IsFocused();
    }

    bool Window::IsTransparent() const
    {
        return State->IsTransparent();
    }

    float Window::Scale() const
    {
        return State->Scale();
    }

    Vector2 Window::PixelSize() const
    {
        return State->PixelSize();
    }

    easyforge::Monitor Window::Monitor() const
    {
        return State->Platform ? State->Platform->CurrentMonitor() : easyforge::Monitor::Primary();
    }

    ColorScheme Window::SystemColorScheme() const
    {
        return State->SystemColorScheme();
    }

    std::string Window::ClipboardText() const
    {
        return State->ClipboardText();
    }

    void Window::SetClipboardText(std::string_view text) const
    {
        State->SetClipboardText(text);
    }

    void Window::SetTextInput(bool enabled, Rectangle caret) const
    {
        State->SetTextInput(enabled, caret);
    }

    void Window::SetIcon(const ImageData& image) const
    {
        if (State->Platform && image)
        {
            State->Platform->SetIcon(image);
        }
    }

    void Window::SetCursorImage(const ImageData& image, Vector2 hotSpot) const
    {
        if (State->Platform && image)
        {
            State->Platform->SetCursorImage(image, hotSpot);
        }
    }

    Surface Window::NativeSurface() const
    {
        return State->NativeSurface();
    }

    std::shared_ptr<Host> Window::AsHost() const
    {
        return State;
    }

    std::vector<Monitor> Monitor::All()
    {
        return internal::PlatformMonitors();
    }

    Monitor Monitor::Primary()
    {
        std::vector<easyforge::Monitor> monitors = All();
        return monitors.empty() ? easyforge::Monitor {} : monitors.front();
    }
}
