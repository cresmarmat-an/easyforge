#include <array>
#include <cmath>
#include <cstdlib>
#include <format>

#include "../Platform.h"
#include "../WindowState.h"
#include "Icons.h"
#include "Keyboard.h"
#include "Monitors.h"
#include "Win32.h"

#include <dwmapi.h>
#include <imm.h>
#include <shellapi.h>
#include <shellscalingapi.h>

namespace easyforge::internal
{
    namespace
    {
        constexpr wchar_t ClassName[] = L"easyforge.window";

        // Runs frames while Windows holds the thread in its own loop, such as while
        // the person drags an edge of the window.
        constexpr UINT_PTR ModalFrameTimer = 1;

        LRESULT CALLBACK WindowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam);

        void PrepareProcess()
        {
            static bool prepared = [] {
                // Sizes in points only work when Windows tells each window its real
                // scale. This fails harmlessly when the program already chose.
                SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

                WNDCLASSEXW windowClass {};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.style = CS_HREDRAW | CS_VREDRAW;
                windowClass.lpfnWndProc = WindowProcedure;
                windowClass.hInstance = GetModuleHandleW(nullptr);
                windowClass.lpszClassName = ClassName;
                return RegisterClassExW(&windowClass) != 0;
            }();
            (void)prepared;
        }

        LPCWSTR StandardCursor(Cursor cursor)
        {
            switch (cursor)
            {
            case Cursor::Arrow: return IDC_ARROW;
            case Cursor::Text: return IDC_IBEAM;
            case Cursor::Hand: return IDC_HAND;
            case Cursor::Crosshair: return IDC_CROSS;
            case Cursor::Move: return IDC_SIZEALL;
            case Cursor::ResizeHorizontal: return IDC_SIZEWE;
            case Cursor::ResizeVertical: return IDC_SIZENS;
            case Cursor::ResizeDiagonal: return IDC_SIZENWSE;
            case Cursor::ResizeAntiDiagonal: return IDC_SIZENESW;
            case Cursor::NotAllowed: return IDC_NO;
            case Cursor::Wait: return IDC_WAIT;
            case Cursor::Progress: return IDC_APPSTARTING;
            case Cursor::Hidden: return nullptr;
            }
            return IDC_ARROW;
        }

        // Mouse positions come as two signed 16-bit numbers in one parameter.
        POINT PointFrom(LPARAM lParam)
        {
            return { static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam)) };
        }

        KeyModifiers CurrentModifiers()
        {
            KeyModifiers modifiers;
            modifiers.Shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            modifiers.Control = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            modifiers.Alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            modifiers.Meta = ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) != 0;
            return modifiers;
        }

        void AppendUtf8(std::string& text, char32_t codePoint)
        {
            if (codePoint < 0x80)
            {
                text += static_cast<char>(codePoint);
            }
            else if (codePoint < 0x800)
            {
                text += static_cast<char>(0xC0 | (codePoint >> 6));
                text += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
            else if (codePoint < 0x10000)
            {
                text += static_cast<char>(0xE0 | (codePoint >> 12));
                text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                text += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
            else
            {
                text += static_cast<char>(0xF0 | (codePoint >> 18));
                text += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
                text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
                text += static_cast<char>(0x80 | (codePoint & 0x3F));
            }
        }

        COLORREF ToColorReference(Color color)
        {
            auto byte = [](float component) { return static_cast<BYTE>(std::lround(Clamp(component, 0.0f, 1.0f) * 255.0f)); };
            return RGB(byte(color.Red), byte(color.Green), byte(color.Blue));
        }
    }

    class Win32Window final : public PlatformWindow
    {
    public:
        explicit Win32Window(WindowState& owner) : Owner(owner) {}

        Result<> Create(const WindowSettings& settings);

        LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

        // Surface
        Surface NativeSurface() const override
        {
            return { Surface::Platform::Windows, Handle, GetModuleHandleW(nullptr) };
        }

        void SetTitle(const std::string& title) override { SetWindowTextW(Handle, ToWide(title).c_str()); }
        void SetIcon(const ImageData& image) override;

        float Scale() const override { return static_cast<float>(Dpi) / 96.0f; }
        Vector2 ContentSize() const override { return PixelSize() / Scale(); }
        Vector2 PixelSize() const override;
        void SetContentSize(Vector2 points) override;
        void SetMinimumSize(Vector2 points) override { MinimumSize = points; }

        Vector2 Position() const override;
        void SetPosition(Vector2 pixels) override;

        bool IsMaximized() const override { return IsZoomed(Handle) || (!IsVisible() && MaximizeWhenShown); }
        void SetMaximized(bool maximized) override;
        bool IsMinimized() const override { return IsIconic(Handle) != FALSE; }
        void SetMinimized(bool minimized) override;
        bool IsFullscreen() const override { return Fullscreen; }
        void SetFullscreen(bool fullscreen) override;
        bool IsVisible() const override { return IsWindowVisible(Handle) != FALSE; }
        void SetVisible(bool visible) override;
        void SetResizable(bool resizable) override;
        void SetAlwaysOnTop(bool alwaysOnTop) override;

        void SetBackground(Color color) override;
        void SurfaceClaimed(bool claimed) override;

        void SetCursor(Cursor cursor) override;
        void SetCursorImage(const ImageData& image, Vector2 hotSpot) override;
        void SetMouseLocked(bool locked) override;
        void SetCustomTitleBar(bool custom) override;
        void SetTextInput(bool enabled, Rectangle caret) override;

        bool IsFocused() const override { return Focused; }
        void Focus() override;

        easyforge::Monitor CurrentMonitor() const override
        {
            return DescribeMonitor(MonitorFromWindow(Handle, MONITOR_DEFAULTTONEAREST));
        }

        void Destroy() override;

        HWND Handle = nullptr;
        UINT Dpi = 96;
        int MessageDepth = 0;
        bool DestroyRequested = false;
        bool InsideDestroy = false;

    private:
        // Anything Windows says after Destroy is ignored, since the owner may be gone.
        void Report(Event& event)
        {
            if (Ready && !DestroyRequested)
            {
                Owner.Report(event);
            }
        }

        void Report(EventType type)
        {
            Event event;
            event.Type = type;
            Report(event);
        }

        DWORD Style() const;
        DWORD ExtendedStyle() const;
        void ApplyStyle();

        // How far the edges of the whole window are from the content, in pixels,
        // at the given DPI.
        RECT FrameInsets(UINT dpi) const;
        int ResizeBorderHeight() const;

        Vector2 ToPoints(POINT pixels) const
        {
            return { static_cast<float>(pixels.x) / Scale(), static_cast<float>(pixels.y) / Scale() };
        }

        POINT ScreenToContent(LPARAM lParam) const
        {
            POINT point = PointFrom(lParam);
            ScreenToClient(Handle, &point);
            return point;
        }

        void BuildIcons();
        void ApplyCursor();
        void ShowChosenCursor();
        bool CursorInsideContent() const;
        void ApplyMouseLock();
        void ApplyDarkTitleBar();
        void RunModalFrames();

        LRESULT HitTest(LPARAM lParam);
        void HandleKey(UINT message, WPARAM wParam, LPARAM lParam);
        void HandleCharacter(WPARAM wParam);
        void HandleMouseMove(POINT point);
        void HandleButton(MouseButton button, bool pressed, POINT point);
        void HandleMouseLeft();
        void ReleaseTitleBarButton(POINT point);
        void HandleRawInput(LPARAM lParam);
        void HandleComposition(LPARAM lParam);
        void HandleDroppedFiles(HDROP drop);
        bool HandlePointer(UINT message, WPARAM wParam, LPARAM lParam);
        void ReleaseHeldKeys();
        void ReportKey(Key key, bool pressed);

        WindowState& Owner;
        bool Ready = false;

        bool Resizable = true;
        bool AlwaysOnTop = false;
        bool Transparent = false;
        bool CustomTitleBar = false;
        bool Fullscreen = false;
        WINDOWPLACEMENT PlacementBeforeFullscreen { sizeof(WINDOWPLACEMENT) };
        bool MaximizeWhenShown = false;
        Vector2 MinimumSize;
        bool InModalLoop = false;
        bool Focused = false;

        Color Background;
        HBRUSH BackgroundBrush = nullptr;
        bool SurfaceIsClaimed = false;

        ImageData IconImage;
        HICON SmallIcon = nullptr;
        HICON BigIcon = nullptr;

        Cursor StandardCursorShape = Cursor::Arrow;
        HCURSOR CustomCursor = nullptr;
        bool UsingCustomCursor = false;

        bool MouseLocked = false;
        bool MouseInside = false;
        bool TrackingContent = false;
        bool TrackingFrame = false;
        bool HasMousePosition = false;
        POINT MousePosition {};
        bool HasRawPosition = false;
        POINT RawPosition {};
        int ButtonsHeld = 0;

        // The custom title bar button the left button went down on, while it is held.
        HitArea PressedTitleBarButton = HitArea::Content;
        bool TitleBarButtonHeld = false;

        MouseButton LastClickButton = MouseButton::Left;
        DWORD LastClickTime = 0;
        POINT LastClickPoint {};
        int ClickCount = 0;

        std::array<bool, static_cast<std::size_t>(Key::Count)> KeysHeld {};
        wchar_t HighSurrogate = 0;

        bool TextInputEnabled = false;
        Rectangle Caret;
    };

    namespace
    {
        LRESULT CALLBACK WindowProcedure(HWND handle, UINT message, WPARAM wParam, LPARAM lParam)
        {
            if (message == WM_NCCREATE)
            {
                auto* creating = static_cast<Win32Window*>(reinterpret_cast<CREATESTRUCTW*>(lParam)->lpCreateParams);
                creating->Handle = handle;
                creating->Dpi = GetDpiForWindow(handle);
                SetWindowLongPtrW(handle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creating));
            }
            auto* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(handle, GWLP_USERDATA));
            if (!window)
            {
                return DefWindowProcW(handle, message, wParam, lParam);
            }

            ++window->MessageDepth;
            LRESULT result = window->HandleMessage(message, wParam, lParam);
            --window->MessageDepth;

            // A window closed while handling a message is freed once Windows is done with it.
            if (window->DestroyRequested && window->MessageDepth == 0 && !window->InsideDestroy)
            {
                delete window;
            }
            return result;
        }
    }

    Result<PlatformWindowPointer> CreatePlatformWindow(WindowState& owner, const WindowSettings& settings)
    {
        auto* window = new Win32Window(owner);
        PlatformWindowPointer pointer(window);
        Result<> created = window->Create(settings);
        if (!created)
        {
            return Failure(created.Error());
        }
        return pointer;
    }

    Result<> Win32Window::Create(const WindowSettings& settings)
    {
        PrepareProcess();
        Resizable = settings.Resizable;
        AlwaysOnTop = settings.AlwaysOnTop;
        Transparent = settings.Transparent;
        MinimumSize = { Max(settings.MinimumWidth, 0.0f), Max(settings.MinimumHeight, 0.0f) };

        HWND created = CreateWindowExW(ExtendedStyle(), ClassName, ToWide(settings.Title).c_str(), Style(),
            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, nullptr, nullptr, GetModuleHandleW(nullptr),
            this);
        if (!created)
        {
            return Failure(std::format("Windows could not create the window (error {})", GetLastError()));
        }

        // Size and place the window on the screen it will show on, at that
        // screen's scale.
        POINT anchor { 0, 0 };
        if (settings.Position)
        {
            anchor = { static_cast<LONG>(std::lround(settings.Position->X)), static_cast<LONG>(std::lround(settings.Position->Y)) };
        }
        HMONITOR monitor = MonitorFromPoint(anchor, settings.Position ? MONITOR_DEFAULTTONEAREST : MONITOR_DEFAULTTOPRIMARY);
        for (int attempt = 0; attempt < 2; ++attempt)
        {
            UINT dpiX = Dpi;
            UINT dpiY = Dpi;
            GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            float scale = static_cast<float>(dpiX) / 96.0f;
            RECT insets = FrameInsets(dpiX);
            int width = static_cast<int>(std::lround(Max(settings.Width, 1.0f) * scale)) + insets.left + insets.right;
            int height = static_cast<int>(std::lround(Max(settings.Height, 1.0f) * scale)) + insets.top + insets.bottom;

            // A window larger than the screen would reach under the taskbar or off
            // the edge; start it at the most that fits.
            MONITORINFO information { sizeof(MONITORINFO) };
            GetMonitorInfoW(monitor, &information);
            const RECT& work = information.rcWork;
            width = Min(width, static_cast<int>(work.right - work.left));
            height = Min(height, static_cast<int>(work.bottom - work.top));

            int left = work.left + ((work.right - work.left) - width) / 2;
            int top = work.top + ((work.bottom - work.top) - height) / 2;
            if (settings.Position)
            {
                left = anchor.x - insets.left;
                top = anchor.y - insets.top;
            }
            SetWindowPos(Handle, nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);

            // Moving to another screen can change the scale; size again for it.
            UINT now = GetDpiForWindow(Handle);
            if (now == dpiX)
            {
                break;
            }
            Dpi = now;
        }
        Dpi = GetDpiForWindow(Handle);

        if (AlwaysOnTop)
        {
            SetWindowPos(Handle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        DragAcceptFiles(Handle, TRUE);
        ImmAssociateContextEx(Handle, nullptr, 0);
        ApplyDarkTitleBar();
        Ready = true;
        return {};
    }

    void Win32Window::Destroy()
    {
        DestroyRequested = true;
        InsideDestroy = true;
        if (Handle)
        {
            if (MouseLocked)
            {
                ClipCursor(nullptr);
            }
            DestroyWindow(Handle);
        }
        InsideDestroy = false;

        if (SmallIcon)
        {
            DestroyIcon(SmallIcon);
        }
        if (BigIcon)
        {
            DestroyIcon(BigIcon);
        }
        if (CustomCursor)
        {
            DestroyCursor(CustomCursor);
        }
        if (BackgroundBrush)
        {
            DeleteObject(BackgroundBrush);
        }
        SmallIcon = BigIcon = nullptr;
        CustomCursor = nullptr;
        BackgroundBrush = nullptr;

        if (MessageDepth == 0)
        {
            delete this;
        }
    }

    DWORD Win32Window::Style() const
    {
        if (Fullscreen)
        {
            return WS_POPUP | WS_CLIPCHILDREN;
        }
        DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
        if (Resizable)
        {
            style |= WS_THICKFRAME | WS_MAXIMIZEBOX;
        }
        return style;
    }

    DWORD Win32Window::ExtendedStyle() const
    {
        return WS_EX_APPWINDOW | (Transparent ? WS_EX_NOREDIRECTIONBITMAP : 0);
    }

    void Win32Window::ApplyStyle()
    {
        LONG_PTR kept = GetWindowLongPtrW(Handle, GWL_STYLE) & (WS_VISIBLE | WS_MAXIMIZE | WS_MINIMIZE);
        SetWindowLongPtrW(Handle, GWL_STYLE, static_cast<LONG_PTR>(Style()) | kept);
        SetWindowPos(Handle, nullptr, 0, 0, 0, 0,
            SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    }

    RECT Win32Window::FrameInsets(UINT dpi) const
    {
        RECT rect {};
        if (!Fullscreen)
        {
            AdjustWindowRectExForDpi(&rect, Style(), FALSE, ExtendedStyle(), dpi);
        }
        RECT insets { -rect.left, -rect.top, rect.right, rect.bottom };
        if (CustomTitleBar && !Fullscreen)
        {
            insets.top = 0;
        }
        return insets;
    }

    int Win32Window::ResizeBorderHeight() const
    {
        return GetSystemMetricsForDpi(SM_CYFRAME, Dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
    }

    Vector2 Win32Window::PixelSize() const
    {
        RECT client {};
        GetClientRect(Handle, &client);
        return { static_cast<float>(client.right - client.left), static_cast<float>(client.bottom - client.top) };
    }

    void Win32Window::SetContentSize(Vector2 points)
    {
        RECT insets = FrameInsets(Dpi);
        int width = static_cast<int>(std::lround(points.X * Scale())) + insets.left + insets.right;
        int height = static_cast<int>(std::lround(points.Y * Scale())) + insets.top + insets.bottom;
        // A full screen, maximized, or minimized window keeps its size; change the
        // size it goes back to.
        if (Fullscreen)
        {
            RECT& normal = PlacementBeforeFullscreen.rcNormalPosition;
            normal.right = normal.left + width;
            normal.bottom = normal.top + height;
            return;
        }
        if (IsZoomed(Handle) || IsIconic(Handle))
        {
            WINDOWPLACEMENT placement { sizeof(WINDOWPLACEMENT) };
            GetWindowPlacement(Handle, &placement);
            placement.rcNormalPosition.right = placement.rcNormalPosition.left + width;
            placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + height;
            SetWindowPlacement(Handle, &placement);
            return;
        }
        SetWindowPos(Handle, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    Vector2 Win32Window::Position() const
    {
        POINT corner { 0, 0 };
        ClientToScreen(Handle, &corner);
        return { static_cast<float>(corner.x), static_cast<float>(corner.y) };
    }

    void Win32Window::SetPosition(Vector2 pixels)
    {
        if (Fullscreen)
        {
            return;
        }
        RECT window {};
        GetWindowRect(Handle, &window);
        POINT corner { 0, 0 };
        ClientToScreen(Handle, &corner);
        int left = static_cast<int>(std::lround(pixels.X)) - (corner.x - window.left);
        int top = static_cast<int>(std::lround(pixels.Y)) - (corner.y - window.top);
        SetWindowPos(Handle, nullptr, left, top, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    }

    void Win32Window::SetMaximized(bool maximized)
    {
        if (!IsVisible())
        {
            MaximizeWhenShown = maximized;
            return;
        }
        if (maximized && !IsZoomed(Handle))
        {
            ShowWindow(Handle, SW_MAXIMIZE);
        }
        else if (!maximized && IsZoomed(Handle))
        {
            ShowWindow(Handle, SW_RESTORE);
        }
    }

    void Win32Window::SetMinimized(bool minimized)
    {
        if (minimized && !IsIconic(Handle) && IsVisible())
        {
            ShowWindow(Handle, SW_MINIMIZE);
        }
        else if (!minimized && IsIconic(Handle))
        {
            ShowWindow(Handle, SW_RESTORE);
        }
    }

    void Win32Window::SetFullscreen(bool fullscreen)
    {
        if (fullscreen == Fullscreen)
        {
            return;
        }
        if (fullscreen)
        {
            GetWindowPlacement(Handle, &PlacementBeforeFullscreen);
            MONITORINFO information { sizeof(MONITORINFO) };
            GetMonitorInfoW(MonitorFromWindow(Handle, MONITOR_DEFAULTTONEAREST), &information);
            Fullscreen = true;
            LONG_PTR kept = GetWindowLongPtrW(Handle, GWL_STYLE) & WS_VISIBLE;
            SetWindowLongPtrW(Handle, GWL_STYLE, static_cast<LONG_PTR>(Style()) | kept);
            const RECT& area = information.rcMonitor;
            SetWindowPos(Handle, nullptr, area.left, area.top, area.right - area.left, area.bottom - area.top,
                SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        }
        else
        {
            Fullscreen = false;
            LONG_PTR kept = GetWindowLongPtrW(Handle, GWL_STYLE) & WS_VISIBLE;
            SetWindowLongPtrW(Handle, GWL_STYLE, static_cast<LONG_PTR>(Style()) | kept);
            if (!IsVisible())
            {
                PlacementBeforeFullscreen.showCmd = SW_HIDE;
            }
            SetWindowPlacement(Handle, &PlacementBeforeFullscreen);
            SetWindowPos(Handle, nullptr, 0, 0, 0, 0,
                SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOACTIVATE);
        }
    }

    void Win32Window::SetVisible(bool visible)
    {
        if (visible == IsVisible())
        {
            return;
        }
        if (visible)
        {
            ShowWindow(Handle, MaximizeWhenShown && !Fullscreen ? SW_SHOWMAXIMIZED : SW_SHOW);
            MaximizeWhenShown = false;
        }
        else
        {
            MaximizeWhenShown = IsZoomed(Handle) != FALSE;
            ShowWindow(Handle, SW_HIDE);
        }
    }

    void Win32Window::SetResizable(bool resizable)
    {
        if (resizable == Resizable)
        {
            return;
        }
        Vector2 size = PixelSize();
        Resizable = resizable;
        if (Fullscreen)
        {
            return;
        }
        ApplyStyle();
        // The frame's thickness changes with it; keep the content's size.
        if (!IsZoomed(Handle))
        {
            RECT insets = FrameInsets(Dpi);
            SetWindowPos(Handle, nullptr, 0, 0, static_cast<int>(size.X) + insets.left + insets.right,
                static_cast<int>(size.Y) + insets.top + insets.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    void Win32Window::SetAlwaysOnTop(bool alwaysOnTop)
    {
        AlwaysOnTop = alwaysOnTop;
        SetWindowPos(Handle, alwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }

    void Win32Window::SetBackground(Color color)
    {
        Background = color;
        if (BackgroundBrush)
        {
            DeleteObject(BackgroundBrush);
        }
        BackgroundBrush = CreateSolidBrush(ToColorReference(color));
        InvalidateRect(Handle, nullptr, FALSE);
    }

    void Win32Window::SurfaceClaimed(bool claimed)
    {
        SurfaceIsClaimed = claimed;
        InvalidateRect(Handle, nullptr, FALSE);
    }

    void Win32Window::SetIcon(const ImageData& image)
    {
        IconImage = image;
        BuildIcons();
    }

    void Win32Window::BuildIcons()
    {
        int smallSize = GetSystemMetricsForDpi(SM_CXSMICON, Dpi);
        int bigSize = GetSystemMetricsForDpi(SM_CXICON, Dpi);
        HICON smallIcon = IconImage ? CreateIconFromImage(IconImage, smallSize) : LoadProgramIcon(smallSize);
        HICON bigIcon = IconImage ? CreateIconFromImage(IconImage, bigSize) : LoadProgramIcon(bigSize);
        SendMessageW(Handle, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(smallIcon));
        SendMessageW(Handle, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(bigIcon));
        if (SmallIcon)
        {
            DestroyIcon(SmallIcon);
        }
        if (BigIcon)
        {
            DestroyIcon(BigIcon);
        }
        SmallIcon = smallIcon;
        BigIcon = bigIcon;
    }

    void Win32Window::SetCursor(Cursor cursor)
    {
        StandardCursorShape = cursor;
        UsingCustomCursor = false;
        ApplyCursor();
    }

    void Win32Window::SetCursorImage(const ImageData& image, Vector2 hotSpot)
    {
        HCURSOR created = CreateCursorFromImage(image, static_cast<int>(hotSpot.X), static_cast<int>(hotSpot.Y));
        if (!created)
        {
            return;
        }
        UsingCustomCursor = true;
        HCURSOR old = CustomCursor;
        CustomCursor = created;
        ApplyCursor();
        if (old)
        {
            DestroyCursor(old);
        }
    }

    bool Win32Window::CursorInsideContent() const
    {
        POINT cursor {};
        if (!GetCursorPos(&cursor) || WindowFromPoint(cursor) != Handle)
        {
            return false;
        }
        POINT content = cursor;
        ScreenToClient(Handle, &content);
        RECT client {};
        GetClientRect(Handle, &client);
        return PtInRect(&client, content) != FALSE;
    }

    void Win32Window::ApplyCursor()
    {
        if (!CursorInsideContent())
        {
            return;
        }
        POINT cursor {};
        GetCursorPos(&cursor);
        if (SendMessageW(Handle, WM_NCHITTEST, 0, MAKELPARAM(cursor.x, cursor.y)) == HTCLIENT)
        {
            ShowChosenCursor();
        }
    }

    void Win32Window::ShowChosenCursor()
    {
        if (MouseLocked)
        {
            ::SetCursor(nullptr);
        }
        else if (UsingCustomCursor)
        {
            ::SetCursor(CustomCursor);
        }
        else
        {
            LPCWSTR shape = StandardCursor(StandardCursorShape);
            ::SetCursor(shape ? LoadCursorW(nullptr, shape) : nullptr);
        }
    }

    void Win32Window::SetMouseLocked(bool locked)
    {
        if (locked == MouseLocked)
        {
            return;
        }
        MouseLocked = locked;
        RAWINPUTDEVICE device {};
        device.usUsagePage = 0x01;
        device.usUsage = 0x02;
        device.dwFlags = locked ? 0 : RIDEV_REMOVE;
        device.hwndTarget = locked ? Handle : nullptr;
        RegisterRawInputDevices(&device, 1, sizeof(device));
        HasRawPosition = false;
        ApplyMouseLock();
        ApplyCursor();
    }

    void Win32Window::ApplyMouseLock()
    {
        if (!MouseLocked || !Focused)
        {
            ClipCursor(nullptr);
            return;
        }
        // Hold the hidden cursor at the middle of the content, so clicks never
        // land on the frame or another window.
        RECT client {};
        GetClientRect(Handle, &client);
        POINT middle { (client.right - client.left) / 2, (client.bottom - client.top) / 2 };
        ClientToScreen(Handle, &middle);
        RECT area { middle.x, middle.y, middle.x + 1, middle.y + 1 };
        SetCursorPos(middle.x, middle.y);
        ClipCursor(&area);
    }

    void Win32Window::SetCustomTitleBar(bool custom)
    {
        if (custom == CustomTitleBar)
        {
            return;
        }
        Vector2 size = PixelSize();
        CustomTitleBar = custom;
        ApplyStyle();
        // The system's title bar takes space the content now has, or the other way
        // around; keep the content's size.
        if (!Fullscreen && !IsZoomed(Handle) && !IsIconic(Handle))
        {
            RECT insets = FrameInsets(Dpi);
            SetWindowPos(Handle, nullptr, 0, 0, static_cast<int>(size.X) + insets.left + insets.right,
                static_cast<int>(size.Y) + insets.top + insets.bottom, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
        }
    }

    void Win32Window::SetTextInput(bool enabled, Rectangle caret)
    {
        TextInputEnabled = enabled;
        Caret = caret;
        if (!enabled)
        {
            ImmAssociateContextEx(Handle, nullptr, 0);
            return;
        }
        ImmAssociateContextEx(Handle, nullptr, IACE_DEFAULT);
        if (HIMC context = ImmGetContext(Handle))
        {
            float scale = Scale();
            POINT position { static_cast<LONG>(std::lround(caret.X * scale)), static_cast<LONG>(std::lround(caret.Y * scale)) };
            RECT area { position.x, position.y, static_cast<LONG>(std::lround(caret.Right() * scale)),
                static_cast<LONG>(std::lround(caret.Bottom() * scale)) };

            COMPOSITIONFORM composition {};
            composition.dwStyle = CFS_POINT;
            composition.ptCurrentPos = position;
            ImmSetCompositionWindow(context, &composition);

            CANDIDATEFORM candidates {};
            candidates.dwIndex = 0;
            candidates.dwStyle = CFS_EXCLUDE;
            candidates.ptCurrentPos = { position.x, area.bottom };
            candidates.rcArea = area;
            ImmSetCandidateWindow(context, &candidates);
            ImmReleaseContext(Handle, context);
        }
    }

    void Win32Window::Focus()
    {
        if (IsIconic(Handle))
        {
            ShowWindow(Handle, SW_RESTORE);
        }
        SetForegroundWindow(Handle);
    }

    void Win32Window::ApplyDarkTitleBar()
    {
        BOOL dark = PlatformColorScheme() == ColorScheme::Dark ? TRUE : FALSE;
        // The attribute's number changed in Windows 10 version 2004.
        if (FAILED(DwmSetWindowAttribute(Handle, 20, &dark, sizeof(dark))))
        {
            DwmSetWindowAttribute(Handle, 19, &dark, sizeof(dark));
        }
    }

    void Win32Window::RunModalFrames()
    {
        if (!DestroyRequested)
        {
            RunFramesOfOpenWindows();
        }
    }

    LRESULT Win32Window::HitTest(LPARAM lParam)
    {
        LRESULT hit = DefWindowProcW(Handle, WM_NCHITTEST, 0, lParam);
        if (!CustomTitleBar || Fullscreen || DestroyRequested)
        {
            return hit;
        }

        // The left, right, and bottom edges are still the system's frame. The top
        // edge is inside the content, so resizing from it is decided here.
        POINT point = ScreenToContent(lParam);
        bool resizable = Resizable && !IsZoomed(Handle);
        bool atTop = resizable && point.y < ResizeBorderHeight();
        if (hit == HTLEFT && atTop)
        {
            return HTTOPLEFT;
        }
        if (hit == HTRIGHT && atTop)
        {
            return HTTOPRIGHT;
        }
        if (hit != HTCLIENT)
        {
            return hit;
        }
        if (atTop)
        {
            RECT client {};
            GetClientRect(Handle, &client);
            int corner = GetSystemMetricsForDpi(SM_CXFRAME, Dpi) + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, Dpi);
            if (point.x < corner)
            {
                return HTTOPLEFT;
            }
            if (point.x >= client.right - corner)
            {
                return HTTOPRIGHT;
            }
            return HTTOP;
        }

        switch (Owner.TitleBarHitTest(ToPoints(point)))
        {
        case HitArea::Caption: return HTCAPTION;
        case HitArea::MinimizeButton: return HTMINBUTTON;
        case HitArea::MaximizeButton: return HTMAXBUTTON;
        case HitArea::CloseButton: return HTCLOSE;
        case HitArea::Content: break;
        }
        return HTCLIENT;
    }

    void Win32Window::ReportKey(Key key, bool pressed)
    {
        std::size_t index = static_cast<std::size_t>(key);
        Event event;
        event.Type = pressed ? EventType::KeyPressed : EventType::KeyReleased;
        event.Key = key;
        event.Repeat = pressed && KeysHeld[index];
        event.Modifiers = CurrentModifiers();
        KeysHeld[index] = pressed;
        Report(event);
    }

    void Win32Window::HandleKey(UINT message, WPARAM wParam, LPARAM lParam)
    {
        bool pressed = message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
        unsigned scanCode = (static_cast<unsigned>(lParam) >> 16) & 0xFF;
        bool extended = ((static_cast<unsigned>(lParam) >> 24) & 1) != 0;
        auto virtualKey = static_cast<unsigned>(wParam);

        // AltGr sends a left Control press first, with the same time as the right
        // Alt press that follows it. That Control press is not a real key.
        if (virtualKey == VK_CONTROL && !extended)
        {
            MSG next {};
            if (PeekMessageW(&next, Handle, 0, 0, PM_NOREMOVE) && next.message == message && next.wParam == VK_MENU &&
                ((static_cast<unsigned>(next.lParam) >> 24) & 1) != 0 && next.time == static_cast<DWORD>(GetMessageTime()))
            {
                return;
            }
        }

        Key key = KeyFromScanCode(scanCode, extended, virtualKey);
        if (key == Key::Unknown)
        {
            return;
        }

        // Windows sends Print Screen's release but not its press.
        if (key == Key::PrintScreen && !pressed && !KeysHeld[static_cast<std::size_t>(key)])
        {
            ReportKey(key, true);
        }

        ReportKey(key, pressed);

        // With both Shift keys held, Windows reports only the second release.
        if (!pressed && (key == Key::LeftShift || key == Key::RightShift))
        {
            Key other = key == Key::LeftShift ? Key::RightShift : Key::LeftShift;
            int otherVirtualKey = other == Key::LeftShift ? VK_LSHIFT : VK_RSHIFT;
            if (KeysHeld[static_cast<std::size_t>(other)] && (GetKeyState(otherVirtualKey) & 0x8000) == 0)
            {
                ReportKey(other, false);
            }
        }
    }

    void Win32Window::ReleaseHeldKeys()
    {
        for (std::size_t index = 0; index < KeysHeld.size(); ++index)
        {
            if (KeysHeld[index])
            {
                ReportKey(static_cast<Key>(index), false);
            }
        }
    }

    void Win32Window::HandleCharacter(WPARAM wParam)
    {
        auto unit = static_cast<wchar_t>(wParam);
        char32_t codePoint = unit;
        if (unit >= 0xD800 && unit <= 0xDBFF)
        {
            HighSurrogate = unit;
            return;
        }
        if (unit >= 0xDC00 && unit <= 0xDFFF)
        {
            if (HighSurrogate == 0)
            {
                return;
            }
            codePoint = 0x10000 + ((static_cast<char32_t>(HighSurrogate) - 0xD800) << 10) + (unit - 0xDC00);
        }
        HighSurrogate = 0;

        // Control characters come as key presses instead.
        if (codePoint < 0x20 || codePoint == 0x7F)
        {
            return;
        }
        Event event;
        event.Type = EventType::TextEntered;
        AppendUtf8(event.Text, codePoint);
        Report(event);
    }

    void Win32Window::HandleMouseMove(POINT point)
    {
        if (!MouseInside)
        {
            MouseInside = true;
            HasMousePosition = false;
            Report(EventType::MouseEntered);
        }
        if (MouseLocked)
        {
            return;
        }
        if (HasMousePosition && point.x == MousePosition.x && point.y == MousePosition.y)
        {
            return;
        }
        Event event;
        event.Type = EventType::MouseMoved;
        event.Position = ToPoints(point);
        event.Movement = HasMousePosition ? event.Position - ToPoints(MousePosition) : Vector2 {};
        event.Modifiers = CurrentModifiers();
        MousePosition = point;
        HasMousePosition = true;
        Report(event);
    }

    void Win32Window::HandleButton(MouseButton button, bool pressed, POINT point)
    {
        int bit = 1 << static_cast<int>(button);
        Event event;
        event.Type = pressed ? EventType::MouseButtonPressed : EventType::MouseButtonReleased;
        event.Button = button;
        event.Position = ToPoints(point);
        event.Modifiers = CurrentModifiers();

        if (pressed)
        {
            DWORD now = static_cast<DWORD>(GetMessageTime());
            bool nearby = std::abs(point.x - LastClickPoint.x) <= GetSystemMetricsForDpi(SM_CXDOUBLECLK, Dpi) / 2 &&
                          std::abs(point.y - LastClickPoint.y) <= GetSystemMetricsForDpi(SM_CYDOUBLECLK, Dpi) / 2;
            bool soon = now - LastClickTime <= GetDoubleClickTime();
            ClickCount = button == LastClickButton && nearby && soon && ClickCount > 0 ? ClickCount + 1 : 1;
            LastClickButton = button;
            LastClickTime = now;
            LastClickPoint = point;

            if (ButtonsHeld == 0)
            {
                SetCapture(Handle);
            }
            ButtonsHeld |= bit;
        }
        else
        {
            if ((ButtonsHeld & bit) == 0)
            {
                return;
            }
            ButtonsHeld &= ~bit;
            if (ButtonsHeld == 0)
            {
                ReleaseCapture();
            }
        }
        event.ClickCount = pressed ? ClickCount : Max(ClickCount, 1);
        Report(event);
    }

    void Win32Window::HandleMouseLeft()
    {
        // Moving between the content and a custom title bar's buttons, which
        // Windows counts as frame, is not leaving the window.
        if (CursorInsideContent() || ButtonsHeld != 0)
        {
            POINT cursor {};
            GetCursorPos(&cursor);
            LRESULT hit = SendMessageW(Handle, WM_NCHITTEST, 0, MAKELPARAM(cursor.x, cursor.y));
            TRACKMOUSEEVENT track { sizeof(TRACKMOUSEEVENT) };
            track.dwFlags = TME_LEAVE | (hit == HTCLIENT ? 0 : TME_NONCLIENT);
            track.hwndTrack = Handle;
            TrackMouseEvent(&track);
            TrackingContent = hit == HTCLIENT;
            TrackingFrame = !TrackingContent;
            return;
        }
        if (TitleBarButtonHeld)
        {
            POINT cursor {};
            GetCursorPos(&cursor);
            ScreenToClient(Handle, &cursor);
            ReleaseTitleBarButton(cursor);
        }
        if (MouseInside)
        {
            MouseInside = false;
            HasMousePosition = false;
            Report(EventType::MouseLeft);
        }
    }

    void Win32Window::ReleaseTitleBarButton(POINT point)
    {
        if (!TitleBarButtonHeld)
        {
            return;
        }
        TitleBarButtonHeld = false;
        PressedTitleBarButton = HitArea::Content;
        Event event;
        event.Type = EventType::MouseButtonReleased;
        event.Button = MouseButton::Left;
        event.Position = ToPoints(point);
        event.Modifiers = CurrentModifiers();
        Report(event);
    }

    void Win32Window::HandleRawInput(LPARAM lParam)
    {
        RAWINPUT input {};
        UINT size = sizeof(input);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT, &input, &size, sizeof(RAWINPUTHEADER)) ==
                static_cast<UINT>(-1) ||
            input.header.dwType != RIM_TYPEMOUSE || !MouseLocked || !Focused)
        {
            return;
        }
        const RAWMOUSE& mouse = input.data.mouse;
        POINT movement {};
        if (mouse.usFlags & MOUSE_MOVE_ABSOLUTE)
        {
            // Remote desktops and tablets send positions instead of movement.
            bool virtualDesktop = (mouse.usFlags & MOUSE_VIRTUAL_DESKTOP) != 0;
            int width = GetSystemMetrics(virtualDesktop ? SM_CXVIRTUALSCREEN : SM_CXSCREEN);
            int height = GetSystemMetrics(virtualDesktop ? SM_CYVIRTUALSCREEN : SM_CYSCREEN);
            POINT position { static_cast<LONG>(mouse.lLastX / 65535.0 * width), static_cast<LONG>(mouse.lLastY / 65535.0 * height) };
            if (HasRawPosition)
            {
                movement = { position.x - RawPosition.x, position.y - RawPosition.y };
            }
            RawPosition = position;
            HasRawPosition = true;
        }
        else
        {
            movement = { mouse.lLastX, mouse.lLastY };
        }
        if (movement.x == 0 && movement.y == 0)
        {
            return;
        }
        RECT client {};
        GetClientRect(Handle, &client);
        Event event;
        event.Type = EventType::MouseMoved;
        event.Position = ToPoints({ client.right / 2, client.bottom / 2 });
        event.Movement = { static_cast<float>(movement.x), static_cast<float>(movement.y) };
        event.Modifiers = CurrentModifiers();
        Report(event);
    }

    void Win32Window::HandleComposition(LPARAM lParam)
    {
        HIMC context = ImmGetContext(Handle);
        if (!context)
        {
            return;
        }
        auto read = [&](DWORD kind) {
            LONG bytes = ImmGetCompositionStringW(context, kind, nullptr, 0);
            if (bytes <= 0)
            {
                return std::string();
            }
            std::wstring text(static_cast<std::size_t>(bytes) / sizeof(wchar_t), L'\0');
            ImmGetCompositionStringW(context, kind, text.data(), static_cast<DWORD>(bytes));
            return FromWide(text);
        };
        std::string result = (lParam & GCS_RESULTSTR) ? read(GCS_RESULTSTR) : std::string();
        std::string composing = (lParam & GCS_COMPSTR) ? read(GCS_COMPSTR) : std::string();
        ImmReleaseContext(Handle, context);

        if (!result.empty())
        {
            Event entered;
            entered.Type = EventType::TextEntered;
            entered.Text = result;
            Report(entered);
        }
        Event composition;
        composition.Type = EventType::TextComposition;
        composition.Text = composing;
        Report(composition);
    }

    void Win32Window::HandleDroppedFiles(HDROP drop)
    {
        Event event;
        event.Type = EventType::FilesDropped;
        UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
        for (UINT index = 0; index < count; ++index)
        {
            UINT length = DragQueryFileW(drop, index, nullptr, 0);
            std::wstring path(length + 1, L'\0');
            DragQueryFileW(drop, index, path.data(), length + 1);
            path.resize(length);
            event.Files.push_back(FromWide(path));
        }
        POINT point {};
        DragQueryPoint(drop, &point);
        event.Position = ToPoints(point);
        DragFinish(drop);
        Report(event);
    }

    bool Win32Window::HandlePointer(UINT message, WPARAM wParam, LPARAM lParam)
    {
        UINT32 pointer = GET_POINTERID_WPARAM(wParam);
        POINTER_INPUT_TYPE type = PT_POINTER;
        if (!GetPointerType(pointer, &type) || type != PT_TOUCH)
        {
            return false;
        }
        Event event;
        event.Touch = static_cast<int>(pointer);
        event.Position = ToPoints(ScreenToContent(lParam));
        switch (message)
        {
        case WM_POINTERDOWN: event.Type = EventType::TouchBegan; break;
        case WM_POINTERUPDATE: event.Type = EventType::TouchMoved; break;
        case WM_POINTERUP:
            event.Type = IS_POINTER_CANCELED_WPARAM(wParam) ? EventType::TouchCancelled : EventType::TouchEnded;
            break;
        default: event.Type = EventType::TouchCancelled; break;
        }
        Report(event);
        return true;
    }

    LRESULT Win32Window::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam)
    {
        switch (message)
        {
        case WM_NCCALCSIZE:
            if (wParam && CustomTitleBar && !Fullscreen)
            {
                // The system's frame without its title bar: the content starts at
                // the top edge of the window.
                auto* sizes = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                LONG top = sizes->rgrc[0].top;
                LRESULT result = DefWindowProcW(Handle, message, wParam, lParam);
                if (result != 0)
                {
                    return result;
                }
                sizes->rgrc[0].top = top;
                // A maximized window reaches past the screen by its frame's thickness.
                if (IsZoomed(Handle))
                {
                    sizes->rgrc[0].top += ResizeBorderHeight();
                }
                return 0;
            }
            break;

        case WM_NCHITTEST:
            return HitTest(lParam);

        case WM_NCMOUSEMOVE:
            if (CustomTitleBar)
            {
                POINT point = ScreenToContent(lParam);
                if (!TrackingFrame)
                {
                    TRACKMOUSEEVENT track { sizeof(TRACKMOUSEEVENT) };
                    track.dwFlags = TME_LEAVE | TME_NONCLIENT;
                    track.hwndTrack = Handle;
                    TrackMouseEvent(&track);
                    TrackingFrame = true;
                    TrackingContent = false;
                }
                RECT client {};
                GetClientRect(Handle, &client);
                if (PtInRect(&client, point))
                {
                    HandleMouseMove(point);
                }
            }
            break;

        case WM_NCLBUTTONDOWN:
        case WM_NCLBUTTONDBLCLK:
            // Buttons of a custom title bar: tell the view, and act on release.
            // Windows would otherwise draw its own old buttons over the view.
            if (CustomTitleBar && (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE))
            {
                PressedTitleBarButton = wParam == HTMINBUTTON   ? HitArea::MinimizeButton
                                        : wParam == HTMAXBUTTON ? HitArea::MaximizeButton
                                                                : HitArea::CloseButton;
                TitleBarButtonHeld = true;
                Event event;
                event.Type = EventType::MouseButtonPressed;
                event.Button = MouseButton::Left;
                event.Position = ToPoints(ScreenToContent(lParam));
                event.Modifiers = CurrentModifiers();
                Report(event);
                return 0;
            }
            break;

        case WM_NCLBUTTONUP:
            if (CustomTitleBar && (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE))
            {
                HitArea released = wParam == HTMINBUTTON   ? HitArea::MinimizeButton
                                   : wParam == HTMAXBUTTON ? HitArea::MaximizeButton
                                                           : HitArea::CloseButton;
                bool clicked = TitleBarButtonHeld && released == PressedTitleBarButton;
                ReleaseTitleBarButton(ScreenToContent(lParam));
                if (clicked && !DestroyRequested)
                {
                    WPARAM command = released == HitArea::MinimizeButton ? SC_MINIMIZE
                                     : released == HitArea::CloseButton  ? SC_CLOSE
                                     : IsZoomed(Handle)                  ? SC_RESTORE
                                                                         : SC_MAXIMIZE;
                    PostMessageW(Handle, WM_SYSCOMMAND, command, 0);
                }
                return 0;
            }
            break;

        case WM_NCMOUSELEAVE:
            TrackingFrame = false;
            HandleMouseLeft();
            return 0;

        case WM_MOUSELEAVE:
            TrackingContent = false;
            HandleMouseLeft();
            return 0;

        case WM_MOUSEMOVE:
        {
            if (!TrackingContent)
            {
                TRACKMOUSEEVENT track { sizeof(TRACKMOUSEEVENT) };
                track.dwFlags = TME_LEAVE;
                track.hwndTrack = Handle;
                TrackMouseEvent(&track);
                TrackingContent = true;
                TrackingFrame = false;
            }
            // Leaving a title bar button while holding it cancels the click.
            PressedTitleBarButton = HitArea::Content;
            HandleMouseMove(PointFrom(lParam));
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
            if (message == WM_LBUTTONUP && TitleBarButtonHeld)
            {
                ReleaseTitleBarButton(PointFrom(lParam));
                return 0;
            }
            HandleButton(MouseButton::Left, message == WM_LBUTTONDOWN, PointFrom(lParam));
            return 0;
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            HandleButton(MouseButton::Right, message == WM_RBUTTONDOWN, PointFrom(lParam));
            return 0;
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
            HandleButton(MouseButton::Middle, message == WM_MBUTTONDOWN, PointFrom(lParam));
            return 0;
        case WM_XBUTTONDOWN:
        case WM_XBUTTONUP:
            HandleButton(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? MouseButton::Back : MouseButton::Forward,
                message == WM_XBUTTONDOWN, PointFrom(lParam));
            return TRUE;

        case WM_CAPTURECHANGED:
            // Another window took the mouse, such as a menu: nothing is held any more.
            if (reinterpret_cast<HWND>(lParam) != Handle && ButtonsHeld != 0)
            {
                POINT point {};
                GetCursorPos(&point);
                ScreenToClient(Handle, &point);
                for (int button = 0; button < static_cast<int>(MouseButton::Count); ++button)
                {
                    if (ButtonsHeld & (1 << button))
                    {
                        ButtonsHeld &= ~(1 << button);
                        Event event;
                        event.Type = EventType::MouseButtonReleased;
                        event.Button = static_cast<MouseButton>(button);
                        event.Position = ToPoints(point);
                        Report(event);
                    }
                }
            }
            return 0;

        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
        {
            float amount = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / WHEEL_DELTA;
            Event event;
            event.Type = EventType::MouseWheel;
            event.Wheel = message == WM_MOUSEWHEEL ? Vector2 { 0.0f, amount } : Vector2 { amount, 0.0f };
            event.Position = ToPoints(ScreenToContent(lParam));
            event.Modifiers = CurrentModifiers();
            Report(event);
            return 0;
        }

        case WM_INPUT:
            HandleRawInput(lParam);
            break;

        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
        case WM_POINTERCAPTURECHANGED:
            if (HandlePointer(message, wParam, lParam))
            {
                return 0;
            }
            break;

        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
            HandleKey(message, wParam, lParam);
            // Alt+F4 and the other system keys still do what they normally do.
            if (message == WM_SYSKEYDOWN || message == WM_SYSKEYUP)
            {
                break;
            }
            return 0;

        case WM_CHAR:
            HandleCharacter(wParam);
            return 0;

        case WM_SYSCHAR:
            // Only Alt+Space, which opens the window menu, needs the system;
            // anything else would beep.
            if (wParam == L' ')
            {
                break;
            }
            return 0;

        case WM_UNICHAR:
            if (wParam == UNICODE_NOCHAR)
            {
                return TRUE;
            }
            {
                Event event;
                event.Type = EventType::TextEntered;
                AppendUtf8(event.Text, static_cast<char32_t>(wParam));
                Report(event);
            }
            return 0;

        case WM_IME_COMPOSITION:
            HandleComposition(lParam);
            return 0;

        case WM_IME_STARTCOMPOSITION:
            SetTextInput(TextInputEnabled, Caret);
            break;

        case WM_IME_ENDCOMPOSITION:
        {
            Event event;
            event.Type = EventType::TextComposition;
            Report(event);
            break;
        }

        case WM_SYSCOMMAND:
            // Pressing Alt alone or F10 would enter the menu bar the window does
            // not have, and swallow the next key.
            if ((wParam & 0xFFF0) == SC_KEYMENU && lParam == 0)
            {
                return 0;
            }
            break;

        case WM_SETCURSOR:
            if (LOWORD(lParam) == HTCLIENT)
            {
                ShowChosenCursor();
                return TRUE;
            }
            break;

        case WM_SETFOCUS:
            Focused = true;
            ApplyMouseLock();
            Report(EventType::FocusGained);
            return 0;

        case WM_KILLFOCUS:
            Focused = false;
            ApplyMouseLock();
            ReleaseHeldKeys();
            HighSurrogate = 0;
            Report(EventType::FocusLost);
            return 0;

        case WM_SIZE:
        {
            if (wParam == SIZE_MINIMIZED)
            {
                Report(EventType::Minimized);
                return 0;
            }
            Event resized;
            resized.Type = EventType::Resized;
            resized.Size = ToPoints({ LOWORD(lParam), HIWORD(lParam) });
            Report(resized);
            if (wParam == SIZE_MAXIMIZED)
            {
                Report(EventType::Maximized);
            }
            else if (wParam == SIZE_RESTORED)
            {
                Report(EventType::Restored);
            }
            ApplyMouseLock();
            // Keep drawing while the person drags the edge, so the content follows.
            if (InModalLoop)
            {
                RunModalFrames();
            }
            return 0;
        }

        case WM_MOVE:
            ApplyMouseLock();
            return 0;

        case WM_GETMINMAXINFO:
        {
            auto* limits = reinterpret_cast<MINMAXINFO*>(lParam);
            RECT insets = FrameInsets(Dpi);
            LONG width = static_cast<LONG>(std::lround(MinimumSize.X * Scale())) + insets.left + insets.right;
            LONG height = static_cast<LONG>(std::lround(MinimumSize.Y * Scale())) + insets.top + insets.bottom;
            limits->ptMinTrackSize.x = Max(limits->ptMinTrackSize.x, width);
            limits->ptMinTrackSize.y = Max(limits->ptMinTrackSize.y, height);
            return 0;
        }

        case WM_GETDPISCALEDSIZE:
        {
            // Keep the content the same number of points on the new screen.
            UINT dpi = static_cast<UINT>(wParam);
            Vector2 points = ContentSize();
            float scale = static_cast<float>(dpi) / 96.0f;
            RECT insets = FrameInsets(dpi);
            auto* size = reinterpret_cast<SIZE*>(lParam);
            size->cx = static_cast<LONG>(std::lround(points.X * scale)) + insets.left + insets.right;
            size->cy = static_cast<LONG>(std::lround(points.Y * scale)) + insets.top + insets.bottom;
            return TRUE;
        }

        case WM_DPICHANGED:
        {
            Dpi = HIWORD(wParam);
            const auto* suggested = reinterpret_cast<const RECT*>(lParam);
            SetWindowPos(Handle, nullptr, suggested->left, suggested->top, suggested->right - suggested->left,
                suggested->bottom - suggested->top, SWP_NOZORDER | SWP_NOACTIVATE);
            BuildIcons();
            Event event;
            event.Type = EventType::ScaleChanged;
            event.Scale = Scale();
            Report(event);
            return 0;
        }

        case WM_ENTERSIZEMOVE:
        case WM_ENTERMENULOOP:
            InModalLoop = true;
            SetTimer(Handle, ModalFrameTimer, USER_TIMER_MINIMUM, nullptr);
            return 0;

        case WM_EXITSIZEMOVE:
        case WM_EXITMENULOOP:
            InModalLoop = false;
            KillTimer(Handle, ModalFrameTimer);
            return 0;

        case WM_TIMER:
            if (wParam == ModalFrameTimer)
            {
                RunModalFrames();
                return 0;
            }
            break;

        case WM_ERASEBKGND:
            return 1;

        case WM_PAINT:
        {
            PAINTSTRUCT paint {};
            HDC context = BeginPaint(Handle, &paint);
            if (!SurfaceIsClaimed && !Transparent && BackgroundBrush)
            {
                FillRect(context, &paint.rcPaint, BackgroundBrush);
            }
            EndPaint(Handle, &paint);
            return 0;
        }

        case WM_DROPFILES:
            HandleDroppedFiles(reinterpret_cast<HDROP>(wParam));
            return 0;

        case WM_SETTINGCHANGE:
            if (lParam && std::wstring_view(reinterpret_cast<const wchar_t*>(lParam)) == L"ImmersiveColorSet")
            {
                ApplyDarkTitleBar();
                Report(EventType::ColorSchemeChanged);
            }
            break;

        case WM_CLOSE:
            if (Ready && !DestroyRequested)
            {
                Owner.ReportCloseRequest();
            }
            return 0;

        case WM_NCDESTROY:
            SetWindowLongPtrW(Handle, GWLP_USERDATA, 0);
            Handle = nullptr;
            return 0;

        default:
            break;
        }
        return DefWindowProcW(Handle, message, wParam, lParam);
    }
}
