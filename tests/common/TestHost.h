#pragma once

// A host without a window, for libraries that follow one, such as network and
// script. Like a window that waits while nothing changes, it runs a frame only
// when something asks for one, or when a test says so.

#include <algorithm>
#include <atomic>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/core/Host.h>

namespace easyforge::testing
{
    class TestHost final : public Host
    {
    public:
        Surface NativeSurface() const override { return {}; }
        Vector2 Size() const override { return { 800, 600 }; }
        Vector2 PixelSize() const override { return { 800, 600 }; }
        float Scale() const override { return 1.0f; }
        bool IsOpen() const override { return true; }
        bool IsFocused() const override { return true; }
        WindowMode Mode() const override { return WindowMode::Normal; }
        bool IsTransparent() const override { return false; }
        bool VerticalSync() const override { return true; }
        bool ClaimSurface() override { return true; }
        void ReleaseSurface() override {}
        void SetCursor(Cursor) override {}
        void SetTextInput(bool, easyforge::Rectangle) override {}
        ColorScheme SystemColorScheme() const override { return ColorScheme::Light; }
        std::string ClipboardText() const override { return {}; }
        void SetClipboardText(std::string_view) override {}
        void AddListener(HostListener& listener) override { Listeners.push_back(&listener); }
        void RemoveListener(HostListener& listener) override
        {
            Listeners.erase(std::remove(Listeners.begin(), Listeners.end(), &listener), Listeners.end());
        }
        std::shared_ptr<void>& Shared(std::string_view name) override { return SharedObjects[std::string(name)]; }
        void RequestFrame() override
        {
            ++Requests;
            Wanted = true;
        }

        // The soonest any listener wants a frame, in seconds.
        float SecondsUntilWanted() const
        {
            float seconds = Wanted ? 0.0f : std::numeric_limits<float>::infinity();
            for (HostListener* listener : Listeners)
            {
                seconds = std::min(seconds, listener->SecondsUntilNextFrame());
            }
            return seconds;
        }

        void RunFrame(float deltaSeconds)
        {
            Wanted = false;
            std::vector<HostListener*> listeners = Listeners;
            for (HostListener* listener : listeners)
            {
                listener->FrameStarted(deltaSeconds);
            }
            for (HostListener* listener : listeners)
            {
                listener->FrameEnded();
            }
        }

        // Runs a frame when one was asked for, or a listener wants one now.
        void RunFrameIfWanted()
        {
            if (SecondsUntilWanted() <= 0.0f)
            {
                RunFrame(1.0f / 60.0f);
            }
        }

        std::vector<HostListener*> Listeners;
        std::map<std::string, std::shared_ptr<void>> SharedObjects;
        std::atomic<bool> Wanted = false;
        std::atomic<int> Requests = 0;
    };
}
