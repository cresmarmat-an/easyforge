#pragma once

// What each platform folder provides to the platform-neutral window code.

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/assets/ImageData.h>
#include <easyforge/core/Host.h>
#include <easyforge/core/Result.h>
#include <easyforge/window/Monitor.h>
#include <easyforge/window/Window.h>

namespace easyforge::internal
{
    class WindowState;

    // One window as the platform sees it. The platform keeps the truth about
    // anything the person can change directly, such as size and maximizing.
    class PlatformWindow
    {
    public:
        virtual Surface NativeSurface() const = 0;

        virtual void SetTitle(const std::string& title) = 0;

        // An empty image goes back to the icon built into the program.
        virtual void SetIcon(const ImageData& image) = 0;

        virtual float Scale() const = 0;
        virtual Vector2 ContentSize() const = 0;
        virtual Vector2 PixelSize() const = 0;
        virtual void SetContentSize(Vector2 points) = 0;
        virtual void SetMinimumSize(Vector2 points) = 0;

        virtual Vector2 Position() const = 0;
        virtual void SetPosition(Vector2 pixels) = 0;

        virtual bool IsMaximized() const = 0;
        virtual void SetMaximized(bool maximized) = 0;
        virtual bool IsMinimized() const = 0;
        virtual void SetMinimized(bool minimized) = 0;
        virtual bool IsFullscreen() const = 0;
        virtual void SetFullscreen(bool fullscreen) = 0;
        virtual bool IsVisible() const = 0;
        virtual void SetVisible(bool visible) = 0;
        virtual void SetResizable(bool resizable) = 0;
        virtual void SetAlwaysOnTop(bool alwaysOnTop) = 0;

        // Painted where nothing else draws, while no renderer has claimed the surface.
        virtual void SetBackground(Color color) = 0;
        virtual void SurfaceClaimed(bool claimed) = 0;

        virtual void SetCursor(Cursor cursor) = 0;
        virtual void SetCursorImage(const ImageData& image, Vector2 hotSpot) = 0;
        virtual void SetMouseLocked(bool locked) = 0;

        // Hides or shows the system's title bar.
        virtual void SetCustomTitleBar(bool custom) = 0;

        virtual void SetTextInput(bool enabled, Rectangle caret) = 0;

        virtual bool IsFocused() const = 0;
        virtual void Focus() = 0;

        virtual easyforge::Monitor CurrentMonitor() const = 0;

        // Destroys the platform's window and frees this object, at once or, when
        // the platform is still inside one of its messages, once it returns. The
        // window reports nothing more after this.
        virtual void Destroy() = 0;

    protected:
        virtual ~PlatformWindow() = default;
    };

    struct DestroyPlatformWindow
    {
        void operator()(PlatformWindow* window) const { window->Destroy(); }
    };

    using PlatformWindowPointer = std::unique_ptr<PlatformWindow, DestroyPlatformWindow>;

    // Makes the platform's window for `owner`, which the platform reports events
    // to. The window starts hidden.
    Result<PlatformWindowPointer> CreatePlatformWindow(WindowState& owner, const WindowSettings& settings);

    // Handles everything the system has sent, without waiting.
    void HandlePlatformMessages();

    // Waits for the screen's next refresh.
    void WaitForVerticalBlank();

    // Waits until the system sends something or `seconds` pass.
    void WaitForMessages(float seconds);

    ColorScheme PlatformColorScheme();

    std::string PlatformClipboardText();
    void SetPlatformClipboardText(std::string_view text);

    std::vector<Monitor> PlatformMonitors();
}
