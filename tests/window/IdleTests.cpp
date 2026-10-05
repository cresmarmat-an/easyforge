#include <easyforge/core/Testing.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <thread>
#include <vector>

#include <easyforge/core/Clock.h>

#include "TestWindows.h"

using namespace easyforge;
using testwindows::HandleOf;
using testwindows::NewHidden;

// A window without OnFrame runs frames under Run only while something needs
// them, and otherwise waits without using the processor.

namespace
{
    constexpr float Never = std::numeric_limits<float>::infinity();

    // A view that wants a frame every `Every` seconds, and remembers the
    // seconds each frame was given.
    class TimedView final : public View
    {
    public:
        void Attach(Host&) override {}
        void Detach() override {}
        void Place(easyforge::Rectangle) override {}
        void HandleEvent(Event&) override {}

        void Frame(float deltaSeconds) override
        {
            SinceFrame.Restart();
            Deltas.push_back(deltaSeconds);
            if (OnFrame)
            {
                OnFrame();
            }
        }

        float SecondsUntilNextFrame() const override
        {
            return Every - static_cast<float>(SinceFrame.Seconds());
        }

        float Every = Never;
        Clock SinceFrame;
        std::vector<float> Deltas;
        std::function<void()> OnFrame;
    };

    // Closes the window after `seconds`, so a test whose window waits too long
    // fails instead of hanging. The message goes straight to the window.
    class Deadline
    {
    public:
        Deadline(const Window& window, double seconds)
            : Handle(HandleOf(window)), Thread([this, seconds] {
                  Clock clock;
                  while (!Finished.load())
                  {
                      if (clock.Seconds() > seconds)
                      {
                          Expired.store(true);
                          PostMessageW(Handle, WM_CLOSE, 0, 0);
                          return;
                      }
                      std::this_thread::sleep_for(std::chrono::milliseconds(5));
                  }
              })
        {
        }

        ~Deadline()
        {
            Finished.store(true);
            Thread.join();
        }

        HWND Handle;
        std::atomic<bool> Finished = false;
        std::atomic<bool> Expired = false;
        std::thread Thread;
    };
}

EASYFORGE_TEST(WindowsWithNothingToDoWaitForARequest)
{
    Window window = NewHidden();
    auto view = std::make_shared<TimedView>();
    window.Content = view;
    std::atomic<bool> requested = false;
    view->OnFrame = [&] {
        if (requested.load())
        {
            window.Close();
        }
    };

    // Another thread asks for a frame while the window waits.
    std::thread other([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        requested.store(true);
        window.RequestFrame();
    });
    Clock clock;
    {
        Deadline deadline(window, 5.0);
        window.Run();
        EASYFORGE_EXPECT(!deadline.Expired.load());
    }
    other.join();

    // The first frame, then nothing until the request: not 18 frames a
    // second as a hidden window with OnFrame would run.
    EASYFORGE_EXPECT(clock.Seconds() >= 0.25);
    EASYFORGE_EXPECT(view->Deltas.size() >= 2);
    EASYFORGE_EXPECT(view->Deltas.size() <= 4);

    // The time spent waiting for the request is not counted.
    EASYFORGE_EXPECT(view->Deltas.back() <= 1.0f / 30.0f);
}

EASYFORGE_TEST(ViewsGetFramesWhenTheirTimeComes)
{
    Window window = NewHidden();
    auto view = std::make_shared<TimedView>();
    view->Every = 0.1f;
    window.Content = view;
    Clock clock;
    view->OnFrame = [&] {
        if (clock.Seconds() > 0.55)
        {
            window.Close();
        }
    };
    {
        Deadline deadline(window, 5.0);
        window.Run();
        EASYFORGE_EXPECT(!deadline.Expired.load());
    }

    // About one frame each tenth of a second, with the time between them
    // counted, since the view asked to wait for it.
    EASYFORGE_EXPECT(view->Deltas.size() >= 4);
    EASYFORGE_EXPECT(view->Deltas.size() <= 9);
    for (std::size_t index = 2; index < view->Deltas.size(); ++index)
    {
        EASYFORGE_EXPECT(view->Deltas[index] >= 0.09f);
    }
}

EASYFORGE_TEST(OnFrameKeepsFramesComing)
{
    Window window = NewHidden();
    auto view = std::make_shared<TimedView>();
    window.Content = view;
    Clock clock;
    int frames = 0;
    window.OnFrame = [&](float) {
        ++frames;
        if (clock.Seconds() > 0.3)
        {
            window.Close();
        }
    };
    {
        Deadline deadline(window, 5.0);
        window.Run();
    }
    EASYFORGE_EXPECT(frames >= 10);
}

EASYFORGE_TEST(InputWakesAWaitingWindow)
{
    Window window = NewHidden();
    auto view = std::make_shared<TimedView>();
    window.Content = view;
    std::atomic<bool> pressed = false;
    window.OnEvent = [&](const Event& event) {
        if (event.Type == EventType::KeyPressed)
        {
            pressed.store(true);
        }
    };
    view->OnFrame = [&] {
        if (pressed.load())
        {
            window.Close();
        }
    };
    std::thread other([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        PostMessageW(HandleOf(window), WM_KEYDOWN, 'A', testwindows::KeyParameter(0x1E));
    });
    {
        Deadline deadline(window, 5.0);
        window.Run();
        EASYFORGE_EXPECT(!deadline.Expired.load());
    }
    other.join();
    EASYFORGE_EXPECT(pressed.load());
    EASYFORGE_EXPECT(view->Deltas.size() <= 4);
}

EASYFORGE_TEST(RunOneFrameAlwaysRunsAFrame)
{
    Window window = NewHidden();
    auto view = std::make_shared<TimedView>();
    window.Content = view;
    testwindows::Pump(5);
    EASYFORGE_EXPECT_EQUAL(view->Deltas.size(), std::size_t { 5 });
    window.Close();
}

EASYFORGE_TEST(RunReturnsWhenItsWindowClosesWhileAnotherWaits)
{
    // A second window with nothing to do waits for input; closing the first
    // must still end Run instead of waiting along with it.
    Window other = NewHidden();
    Window window = NewHidden();
    int frames = 0;
    window.OnFrame = [&](float) {
        if (++frames == 3)
        {
            PostMessageW(HandleOf(window), WM_CLOSE, 0, 0);
        }
    };
    {
        // Closing the other window would end a wait that never ends otherwise.
        Deadline deadline(other, 5.0);
        window.Run();
        EASYFORGE_EXPECT(!deadline.Expired.load());
    }
    EASYFORGE_EXPECT(!window.IsOpen());
    EASYFORGE_EXPECT(other.IsOpen());
    other.Close();
}
