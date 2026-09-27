#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include <easyforge/assets/ImageData.h>
#include <easyforge/core/Color.h>
#include <easyforge/core/Event.h>
#include <easyforge/core/Host.h>
#include <easyforge/core/Property.h>
#include <easyforge/core/Rectangle.h>
#include <easyforge/core/Vector.h>
#include <easyforge/window/Monitor.h>

namespace easyforge
{
    namespace internal
    {
        class WindowState;
    }

    // What a window starts with. Every one of these can be changed later through
    // the property of the same name, except Transparent.
    //
    // Sizes are in points: one pixel at 100% display scaling, 1.5 pixels at 150%.
    // A window 1280 points wide looks the same size on every screen.
    struct WindowSettings
    {
        std::string Title = "easyforge";

        // An image file for the window's icon, shown in its title bar and the
        // taskbar. Empty uses the icon built into the program, if it has one.
        std::string Icon;

        // The size of the content, not counting the system's frame.
        float Width = 1280.0f;
        float Height = 720.0f;

        float MinimumWidth = 0.0f;
        float MinimumHeight = 0.0f;

        // Where the top left of the content goes, in desktop pixels. Empty centers
        // the window on the primary screen. Write it as `Vector2 { 100, 100 }`.
        std::optional<Vector2> Position;

        bool Resizable = true;
        bool Maximized = false;

        // Covers the whole screen the window is on, without a frame.
        bool Fullscreen = false;

        bool AlwaysOnTop = false;
        bool Visible = true;

        // The window is see-through wherever its content is drawn with alpha
        // below 1. Only chosen here; it cannot be changed later.
        bool Transparent = false;

        // Whether frames wait for the screen, so they come at its refresh rate.
        bool VerticalSync = true;

        // The color the window shows where nothing is drawn. Empty follows the
        // system's light or dark setting.
        std::optional<Color> Background;
    };

    // A window on the desktop, or the whole screen on phones and the web.
    //
    //     Window window = Window::New({ .Title = "Notes", .Width = 1280, .Height = 720 });
    //     window.OnFrame = [](float deltaSeconds) { /* every frame */ };
    //     window.Run();
    //
    // A Window is a handle: copies refer to the same window. An open window stays
    // open until it is closed, even when no handle refers to it any more.
    //
    // Windows belong to the thread that made them. Make and use them on one
    // thread, normally the main one.
    class Window
    {
    public:
        using Settings = WindowSettings;

        // Opens a window. If that fails, the result tests as false and Error()
        // says why.
        static Window New(const WindowSettings& settings = {});

        // Handles what the system sent and runs one frame of every open window.
        // For programs that run their own loop instead of calling Run.
        static void RunOneFrame();

        // Not a window. Tests as false.
        Window();

        Window(const Window& other);
        Window& operator=(const Window& other);
        ~Window();

        // True when the window was made, even after it closes.
        explicit operator bool() const;

        // Why making the window failed, or empty.
        const std::string& Error() const;

        // Runs frames until this window closes. Every other open window keeps
        // running too.
        void Run() const;

        bool IsOpen() const;

        // Closes the window without calling OnCloseRequested.
        void Close() const;

        // Brings the window to the front and gives it the keyboard.
        void Focus() const;

        bool IsFocused() const;

        // True for windows made with Transparent.
        bool IsTransparent() const;

        // Pixels per point on the screen the window is on.
        float Scale() const;

        // The size of the content in pixels.
        Vector2 PixelSize() const;

        // The screen that holds most of the window.
        easyforge::Monitor Monitor() const;

        easyforge::ColorScheme SystemColorScheme() const;

        std::string ClipboardText() const;
        void SetClipboardText(std::string_view text) const;

        // Turns the system's text input on, with any input method window placed
        // next to `caret` (in points from the top left of the window), or off. It
        // starts off, so an input method for Chinese, Japanese, or Korean does
        // not open during a game. Typed characters arrive as TextEntered either
        // way; interfaces from ui turn this on while a text field has focus.
        void SetTextInput(bool enabled, Rectangle caret = {}) const;

        // Sets the icon from an image in memory instead of a file.
        void SetIcon(const ImageData& image) const;

        // Shows the image as the mouse cursor, with `hotSpot` (in pixels of the
        // image) as the point that clicks. Assigning Cursor again goes back to a
        // standard cursor.
        void SetCursorImage(const ImageData& image, Vector2 hotSpot) const;

        Surface NativeSurface() const;

        // The window as a Host, for libraries that work with any window, such as
        // graphics and input: `Renderer::New(window)`.
        std::shared_ptr<Host> AsHost() const;
        operator std::shared_ptr<Host>() const { return AsHost(); }

        Property<std::string> Title;

        // An image file, as in WindowSettings.
        Property<std::string> Icon;

        // The content's size in points. Size sets both at once.
        Property<float> Width;
        Property<float> Height;
        Property<Vector2> Size;

        Property<float> MinimumWidth;
        Property<float> MinimumHeight;

        // The top left of the content, in desktop pixels.
        Property<Vector2> Position;

        Property<bool> Resizable;
        Property<bool> Maximized;
        Property<bool> Minimized;
        Property<bool> Fullscreen;
        Property<bool> AlwaysOnTop;
        Property<bool> Visible;
        Property<bool> VerticalSync;
        Property<Color> Background;

        Property<easyforge::Cursor> Cursor;

        // Hides the cursor and keeps it in the window. MouseMoved events then
        // report Movement from the mouse itself, without the system's pointer
        // speed or the edges of the screen, which is what a camera controlled by
        // the mouse needs.
        Property<bool> MouseLocked;

        // What the window shows, such as an interface from ui.
        Property<std::shared_ptr<View>> Content;

        // A title bar drawn in place of the system's. While one is set, the
        // system's title bar is hidden and this view's HitTest tells the window
        // where to drag, and where its buttons are.
        Property<std::shared_ptr<View>> TitleBar;

        // Called once a frame with the seconds since the last one.
        Property<std::function<void(float)>> OnFrame;

        // Called for every event, after the views and listeners.
        Property<std::function<void(const Event&)>> OnEvent;

        // Called when the person tries to close the window. Return false to keep
        // it open. When not set, the window closes.
        Property<std::function<bool()>> OnCloseRequested;

    private:
        explicit Window(std::shared_ptr<internal::WindowState> state);

        void RebindProperties();

        std::shared_ptr<internal::WindowState> State;
    };
}
