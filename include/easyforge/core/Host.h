#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <easyforge/core/Event.h>
#include <easyforge/core/Rectangle.h>
#include <easyforge/core/Vector.h>

namespace easyforge
{
    // What a window gives graphics to draw into: the platform's own handles.
    struct Surface
    {
        enum class Platform
        {
            None,
            Windows,
            X11,
            Wayland,
            MacOS,
            IOS,
            Android,
            Web,
        };

        Platform Kind = Platform::None;

        // Windows: the HWND. X11: the Window. Wayland: the wl_surface. macOS: the
        // NSView. iOS: the UIView. Android: the ANativeWindow. Web: the canvas's
        // CSS selector, as a C string.
        void* Handle = nullptr;

        // Windows: the HINSTANCE. X11: the Display. Wayland: the wl_display.
        // Elsewhere, nothing.
        void* Connection = nullptr;
    };

    enum class Cursor
    {
        Arrow,
        Text,
        Hand,
        Crosshair,
        Move,
        ResizeHorizontal,
        ResizeVertical,

        // From top left to bottom right.
        ResizeDiagonal,

        // From top right to bottom left.
        ResizeAntiDiagonal,

        NotAllowed,
        Wait,

        // Busy, but still accepting input.
        Progress,

        Hidden,
    };

    // How a window is shown.
    enum class WindowMode
    {
        Normal,
        Minimized,
        Maximized,

        // Covering the whole screen, without a frame or title bar.
        Fullscreen,
    };

    // Whether the system is set to light or dark colors.
    enum class ColorScheme
    {
        Light,
        Dark,
    };

    // What a point in a title bar is, so the window can behave like the
    // platform's own title bar there.
    enum class HitArea
    {
        Content,

        // Dragging moves the window, and double-clicking maximizes it.
        Caption,

        MinimizeButton,

        // On Windows 11, hovering shows the snap layouts.
        MaximizeButton,

        CloseButton,
    };

    class Host;

    // Anything that can fill a window: an interface from ui, or your own drawing.
    // A window holds views in its Content and TitleBar slots and calls them for
    // every event and every frame. A view is in one slot of one window at a time.
    class View
    {
    public:
        virtual ~View() = default;

        // Called when the view is put into a host and when it is taken out. The
        // view must not use the host after Detach.
        virtual void Attach(Host& host) = 0;
        virtual void Detach() = 0;

        // The part of the window the view fills, in points from the top left of
        // the window. Called after Attach and whenever the area changes.
        virtual void Place(Rectangle area) = 0;

        // Every event the host receives. Set event.Handled when the view used it.
        virtual void HandleEvent(Event& event) = 0;

        // Once a frame: update and draw.
        virtual void Frame(float deltaSeconds) = 0;

        // How big the view would like to be when given `available` points. A
        // window uses the height of its title bar view and places the content
        // below it.
        virtual Vector2 PreferredSize(Vector2 available) const { return available; }

        // For a title bar: what the point, in points from the top left of the
        // window, is part of.
        virtual HitArea HitTest(Vector2 point) const
        {
            (void)point;
            return HitArea::Content;
        }
    };

    // Something that follows a host's events and frames without being drawn,
    // such as the Controls of the input library.
    class HostListener
    {
    public:
        virtual ~HostListener() = default;

        // Called after the views, so event.Handled says whether one of them used it.
        virtual void HandleEvent(const Event& event) = 0;

        // Called before the window's OnFrame, and after the views have drawn.
        virtual void FrameStarted(float deltaSeconds) = 0;
        virtual void FrameEnded() = 0;
    };

    // The window side of a View: what views and other libraries can ask of the
    // window they work with. easyforge's Window implements it, and so can a
    // window from another library that wants to show easyforge views.
    class Host
    {
    public:
        virtual ~Host() = default;

        virtual Surface NativeSurface() const = 0;

        // The size of the content, in points.
        virtual Vector2 Size() const = 0;

        // The size of the content in pixels, which is what a renderer draws.
        virtual Vector2 PixelSize() const = 0;

        // Pixels per point: 1.0 at 100% scaling, 1.5 at 150%.
        virtual float Scale() const = 0;

        virtual bool IsOpen() const = 0;
        virtual bool IsFocused() const = 0;
        virtual WindowMode Mode() const = 0;

        // True when the window was made see-through, so a renderer has to draw
        // with an alpha channel the system blends with what is behind.
        virtual bool IsTransparent() const = 0;

        // Whether a renderer should wait for the screen before showing each frame.
        virtual bool VerticalSync() const = 0;

        // A renderer claims the surface while it draws into it. The host then stops
        // painting its own background and stops pacing frames, because the
        // renderer's wait for the screen does that. Returns false when another
        // renderer already has it.
        virtual bool ClaimSurface() = 0;
        virtual void ReleaseSurface() = 0;

        virtual void SetCursor(Cursor cursor) = 0;

        // Turns text input on, with any input method window placed next to `caret`
        // (in points), or off. Views that edit text turn it on while they have focus.
        virtual void SetTextInput(bool enabled, Rectangle caret) = 0;

        // The system's light or dark setting. The host sends ColorSchemeChanged
        // when it changes.
        virtual ColorScheme SystemColorScheme() const = 0;

        virtual std::string ClipboardText() const = 0;
        virtual void SetClipboardText(std::string_view text) = 0;

        // The listener must stay alive until it is removed.
        virtual void AddListener(HostListener& listener) = 0;
        virtual void RemoveListener(HostListener& listener) = 0;

        // One object per host that libraries share, found by a name such as
        // "easyforge.graphics.renderer". Starts empty.
        virtual std::shared_ptr<void>& Shared(std::string_view name) = 0;
    };
}
