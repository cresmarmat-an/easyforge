#pragma once

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <easyforge/core/Clock.h>

#include "Platform.h"

namespace easyforge::internal
{
    // Everything a Window handle refers to. The platform window reports events
    // here, and this class passes them on to the views, listeners, and callbacks.
    class WindowState final : public Host, public std::enable_shared_from_this<WindowState>
    {
    public:
        ~WindowState() override;

        // Opens the platform window and adds it to the open windows.
        Result<> Open(const WindowSettings& settings);

        // Detaches the views and destroys the platform window.
        void Close();

        // Host
        Surface NativeSurface() const override;
        Vector2 Size() const override;
        Vector2 PixelSize() const override;
        float Scale() const override;
        bool IsOpen() const override { return Platform != nullptr; }
        bool IsFocused() const override;
        WindowMode Mode() const override;
        bool IsTransparent() const override { return Transparent; }
        bool VerticalSync() const override { return VerticalSyncEnabled; }
        bool ClaimSurface() override;
        void ReleaseSurface() override;
        void SetCursor(Cursor cursor) override;
        void SetTextInput(bool enabled, Rectangle caret) override;
        ColorScheme SystemColorScheme() const override;
        std::string ClipboardText() const override;
        void SetClipboardText(std::string_view text) override;
        void AddListener(HostListener& listener) override;
        void RemoveListener(HostListener& listener) override;
        std::shared_ptr<void>& Shared(std::string_view name) override;
        void RequestPlacement() override { PlacementWanted = true; }

        // Called by the platform window.
        void Report(Event& event);
        void ReportCloseRequest();
        HitArea TitleBarHitTest(Vector2 point) const;
        bool HasCustomTitleBar() const { return TitleBarView != nullptr; }

        // Runs one frame, unless one is already running.
        void RunFrame();

        void SetContent(std::shared_ptr<View> view);
        void SetTitleBar(std::shared_ptr<View> view);
        void SetBackground(std::optional<Color> color);
        void SetFullscreen(bool fullscreen);

        std::string ErrorText;
        bool Made = false;
        bool Transparent = false;
        bool VerticalSyncEnabled = true;
        bool SurfaceClaimed = false;
        bool MouseLocked = false;
        std::string Title;
        std::string IconPath;
        Vector2 MinimumSize;
        bool Resizable = true;
        bool AlwaysOnTop = false;
        Cursor CurrentCursor = Cursor::Arrow;

        // Empty while the background follows the system's light or dark setting.
        std::optional<Color> ChosenBackground;
        Color Background;

        std::shared_ptr<View> ContentView;
        std::shared_ptr<View> TitleBarView;

        // How tall the title bar was placed, in points.
        float TitleBarHeight = 0.0f;

        std::function<void(float)> FrameCallback;
        std::function<void(const Event&)> EventCallback;
        std::function<bool()> CloseCallback;

        // Null once the window is closed.
        PlatformWindowPointer Platform;

    private:
        bool TitleBarShown() const;
        void PlaceViews();
        void RemoveFinishedListeners();

        // Removed listeners become null until no event or frame is running.
        std::vector<HostListener*> Listeners;
        int Dispatching = 0;

        std::map<std::string, std::shared_ptr<void>, std::less<>> SharedObjects;
        Clock FrameClock;
        bool InFrame = false;
        bool PlacementWanted = false;
    };

    // Every open window, in the order they were opened.
    std::vector<std::shared_ptr<WindowState>>& OpenWindows();

    // Runs one frame of every open window, without handling system messages. The
    // platform calls it while the system holds the thread, such as while the
    // person drags a window's edge.
    void RunFramesOfOpenWindows();

    // The background for the system's current light or dark setting.
    Color DefaultBackground(ColorScheme scheme);
}
