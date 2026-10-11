#include <algorithm>
#include <atomic>
#include <map>
#include <thread>

#include "../Platform.h"
#include "../WindowState.h"
#include "Monitors.h"
#include "Win32.h"

#include <dwmapi.h>
#include <shellscalingapi.h>

namespace easyforge::internal
{
    namespace
    {
        // The thread that handles the windows' messages, which a wake-up is
        // posted to.
        std::atomic<DWORD> WindowThread { 0 };

        // The names screens report, by the GDI device name Windows uses for each
        // desktop area, such as \\.\DISPLAY1.
        std::map<std::wstring, std::string> ScreenNames()
        {
            std::map<std::wstring, std::string> names;
            UINT32 pathCount = 0;
            UINT32 modeCount = 0;
            if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount) != ERROR_SUCCESS)
            {
                return names;
            }
            std::vector<DISPLAYCONFIG_PATH_INFO> paths(pathCount);
            std::vector<DISPLAYCONFIG_MODE_INFO> modes(modeCount);
            if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount, modes.data(),
                    nullptr) != ERROR_SUCCESS)
            {
                return names;
            }
            for (UINT32 index = 0; index < pathCount; ++index)
            {
                const DISPLAYCONFIG_PATH_INFO& path = paths[index];

                DISPLAYCONFIG_SOURCE_DEVICE_NAME source {};
                source.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
                source.header.size = sizeof(source);
                source.header.adapterId = path.sourceInfo.adapterId;
                source.header.id = path.sourceInfo.id;
                if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS)
                {
                    continue;
                }

                DISPLAYCONFIG_TARGET_DEVICE_NAME target {};
                target.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
                target.header.size = sizeof(target);
                target.header.adapterId = path.targetInfo.adapterId;
                target.header.id = path.targetInfo.id;
                if (DisplayConfigGetDeviceInfo(&target.header) != ERROR_SUCCESS || target.monitorFriendlyDeviceName[0] == 0)
                {
                    continue;
                }
                names[source.viewGdiDeviceName] = FromWide(target.monitorFriendlyDeviceName);
            }
            return names;
        }

        Rectangle ToRectangle(const RECT& rect)
        {
            return { static_cast<float>(rect.left), static_cast<float>(rect.top),
                static_cast<float>(rect.right - rect.left), static_cast<float>(rect.bottom - rect.top) };
        }

        Monitor Describe(HMONITOR handle, const std::map<std::wstring, std::string>& names)
        {
            Monitor monitor;
            MONITORINFOEXW information {};
            information.cbSize = sizeof(information);
            if (!GetMonitorInfoW(handle, &information))
            {
                return monitor;
            }
            monitor.Area = ToRectangle(information.rcMonitor);
            monitor.WorkArea = ToRectangle(information.rcWork);
            monitor.IsPrimary = (information.dwFlags & MONITORINFOF_PRIMARY) != 0;

            UINT dpiX = 96;
            UINT dpiY = 96;
            if (SUCCEEDED(GetDpiForMonitor(handle, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
            {
                monitor.Scale = static_cast<float>(dpiX) / 96.0f;
            }

            DEVMODEW mode {};
            mode.dmSize = sizeof(mode);
            if (EnumDisplaySettingsW(information.szDevice, ENUM_CURRENT_SETTINGS, &mode) && mode.dmDisplayFrequency > 1)
            {
                monitor.RefreshRate = static_cast<float>(mode.dmDisplayFrequency);
            }

            auto named = names.find(information.szDevice);
            if (named != names.end())
            {
                monitor.Name = named->second;
            }
            else
            {
                DISPLAY_DEVICEW device {};
                device.cb = sizeof(device);
                monitor.Name = EnumDisplayDevicesW(information.szDevice, 0, &device, 0) ? FromWide(device.DeviceString)
                                                                                        : FromWide(information.szDevice);
            }
            return monitor;
        }

        BOOL CALLBACK AddMonitor(HMONITOR handle, HDC, LPRECT, LPARAM parameter)
        {
            reinterpret_cast<std::vector<HMONITOR>*>(parameter)->push_back(handle);
            return TRUE;
        }

        // Clipboard contents belong to a window, so the clipboard gets one of its own
        // that never shows.
        HWND ClipboardOwner()
        {
            static HWND owner = [] {
                WNDCLASSEXW windowClass {};
                windowClass.cbSize = sizeof(windowClass);
                windowClass.lpfnWndProc = DefWindowProcW;
                windowClass.hInstance = GetModuleHandleW(nullptr);
                windowClass.lpszClassName = L"easyforge.clipboard";
                RegisterClassExW(&windowClass);
                return CreateWindowExW(0, L"easyforge.clipboard", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr,
                    GetModuleHandleW(nullptr), nullptr);
            }();
            return owner;
        }

        // Another program may have the clipboard open for a moment.
        bool OpenClipboardPatiently()
        {
            for (int attempt = 0; attempt < 10; ++attempt)
            {
                if (OpenClipboard(ClipboardOwner()))
                {
                    return true;
                }
                Sleep(2);
            }
            return false;
        }
    }

    float MonitorRefreshRate(HMONITOR handle)
    {
        MONITORINFOEXW information {};
        information.cbSize = sizeof(information);
        DEVMODEW mode {};
        mode.dmSize = sizeof(mode);
        // 0 and 1 mean the hardware's default rate, which the system does not name.
        if (GetMonitorInfoW(handle, &information) && EnumDisplaySettingsW(information.szDevice, ENUM_CURRENT_SETTINGS, &mode) &&
            mode.dmDisplayFrequency > 1)
        {
            return static_cast<float>(mode.dmDisplayFrequency);
        }
        return 0.0f;
    }

    Monitor DescribeMonitor(HMONITOR handle)
    {
        return Describe(handle, ScreenNames());
    }

    std::vector<Monitor> PlatformMonitors()
    {
        std::vector<HMONITOR> handles;
        EnumDisplayMonitors(nullptr, nullptr, AddMonitor, reinterpret_cast<LPARAM>(&handles));
        std::map<std::wstring, std::string> names = ScreenNames();
        std::vector<Monitor> monitors;
        monitors.reserve(handles.size());
        for (HMONITOR handle : handles)
        {
            monitors.push_back(Describe(handle, names));
        }
        std::stable_partition(monitors.begin(), monitors.end(), [](const Monitor& monitor) { return monitor.IsPrimary; });
        return monitors;
    }

    void HandlePlatformMessages()
    {
        WindowThread.store(GetCurrentThreadId());
        MSG message;
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            if (message.message == WM_QUIT)
            {
                // Something asked the program to end: close every window, so Run returns.
                std::vector<std::shared_ptr<WindowState>> windows = OpenWindows();
                for (const std::shared_ptr<WindowState>& window : windows)
                {
                    window->Close();
                }
                continue;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }

    namespace
    {
        // DwmFlush waits for the compositor's next pass, which does not come while
        // the screen sleeps, a remote session is cut off, or nothing is composed;
        // it would hold the window thread until then. It runs on a thread of its
        // own instead, and the window thread waits for it only so long.
        class BlankWaiter
        {
        public:
            BlankWaiter()
                : Wanted(CreateEventW(nullptr, FALSE, FALSE, nullptr)),
                  Passed(CreateEventW(nullptr, FALSE, FALSE, nullptr)),
                  Finished(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
                  Worker([this] { Work(); })
            {
            }

            ~BlankWaiter()
            {
                Stopping = true;
                SetEvent(Wanted);
                // A thread still inside DwmFlush is let go rather than waited for.
                if (WaitForSingleObject(Finished, 100) == WAIT_OBJECT_0)
                {
                    Worker.join();
                    CloseHandle(Wanted);
                    CloseHandle(Passed);
                    CloseHandle(Finished);
                }
                else
                {
                    Worker.detach();
                }
            }

            BlankWaiter(const BlankWaiter&) = delete;
            BlankWaiter& operator=(const BlankWaiter&) = delete;

            // Whether the compositor's pass came within the time.
            bool Wait(DWORD milliseconds)
            {
                // A pass that came after an earlier wait gave up is not this one.
                ResetEvent(Passed);
                SetEvent(Wanted);
                return WaitForSingleObject(Passed, milliseconds) == WAIT_OBJECT_0;
            }

        private:
            void Work()
            {
                while (true)
                {
                    WaitForSingleObject(Wanted, INFINITE);
                    if (Stopping)
                    {
                        SetEvent(Finished);
                        return;
                    }
                    if (FAILED(DwmFlush()))
                    {
                        Sleep(1);
                    }
                    SetEvent(Passed);
                }
            }

            HANDLE Wanted;
            HANDLE Passed;
            HANDLE Finished;
            std::atomic<bool> Stopping = false;
            std::thread Worker;
        };
    }

    void WaitForVerticalBlank()
    {
        static BlankWaiter waiter;
        // A tenth of a second covers a screen refreshing ten times a second; past
        // that the compositor has stopped, and frames go on without it.
        if (!waiter.Wait(100))
        {
            WaitForMessages(1.0f / 60.0f);
        }
    }

    void WaitForMessages(float seconds)
    {
        WindowThread.store(GetCurrentThreadId());
        if (!(seconds < 86'400.0f))
        {
            MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            return;
        }
        // A plain timeout rounds up to the system's timer tick, which is often
        // 15.6 milliseconds, so ask for a timer that keeps finer time.
        static HANDLE timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        LARGE_INTEGER due {};
        due.QuadPart = -static_cast<LONGLONG>(Max(seconds, 0.0f) * 10'000'000.0f);
        if (timer && SetWaitableTimer(timer, &due, 0, nullptr, nullptr, FALSE))
        {
            MsgWaitForMultipleObjectsEx(1, &timer, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            return;
        }
        DWORD milliseconds = static_cast<DWORD>(Max(seconds, 0.0f) * 1000.0f);
        MsgWaitForMultipleObjectsEx(0, nullptr, milliseconds, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
    }

    // A thread message: it reaches the thread even while every window is busy,
    // and nothing handles it, so it only ends the wait.
    void WakeWindowThread()
    {
        if (DWORD thread = WindowThread.load())
        {
            PostThreadMessageW(thread, WM_NULL, 0, 0);
        }
    }

    ColorScheme PlatformColorScheme()
    {
        DWORD light = 1;
        DWORD size = sizeof(light);
        LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
            L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
        return status == ERROR_SUCCESS && light == 0 ? ColorScheme::Dark : ColorScheme::Light;
    }

    std::string PlatformClipboardText()
    {
        if (!IsClipboardFormatAvailable(CF_UNICODETEXT) || !OpenClipboardPatiently())
        {
            return {};
        }
        std::string text;
        if (HANDLE data = GetClipboardData(CF_UNICODETEXT))
        {
            if (const auto* characters = static_cast<const wchar_t*>(GlobalLock(data)))
            {
                std::size_t capacity = GlobalSize(data) / sizeof(wchar_t);
                text = FromWide(std::wstring_view(characters, wcsnlen(characters, capacity)));
                GlobalUnlock(data);
            }
        }
        CloseClipboard();
        return text;
    }

    void SetPlatformClipboardText(std::string_view text)
    {
        std::wstring wide = ToWide(text);
        if (!OpenClipboardPatiently())
        {
            return;
        }
        EmptyClipboard();
        if (HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, (wide.size() + 1) * sizeof(wchar_t)))
        {
            auto* characters = static_cast<wchar_t*>(GlobalLock(data));
            std::copy(wide.begin(), wide.end(), characters);
            characters[wide.size()] = L'\0';
            GlobalUnlock(data);
            if (!SetClipboardData(CF_UNICODETEXT, data))
            {
                GlobalFree(data);
            }
        }
        CloseClipboard();
    }
}
