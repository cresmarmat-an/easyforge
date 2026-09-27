#include <algorithm>
#include <map>

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
        for (HMONITOR handle : handles)
        {
            monitors.push_back(Describe(handle, names));
        }
        std::stable_partition(monitors.begin(), monitors.end(), [](const Monitor& monitor) { return monitor.IsPrimary; });
        return monitors;
    }

    void HandlePlatformMessages()
    {
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

    void WaitForVerticalBlank()
    {
        if (FAILED(DwmFlush()))
        {
            WaitForMessages(1.0f / 60.0f);
        }
    }

    void WaitForMessages(float seconds)
    {
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
