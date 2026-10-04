#pragma once

namespace easyforge
{
    // Turns the measured time between frames into steady steps.
    //
    // When frames are shown in step with the screen, the screen shows them at
    // even intervals, but the time measured between them wobbles by a few
    // milliseconds. Animating by the measured time makes motion uneven. A
    // FrameTimer gives the whole number of screen refreshes each frame took
    // instead, carrying what it rounded off into later frames, so no time is
    // lost or added over many frames. A real hitch still shows as two or more
    // refreshes. It follows the screen's true rate, so a 59.94 hertz screen
    // stays exact over hours.
    //
    //     FrameTimer timer;
    //     timer.SetRefreshRate(60.0f);
    //     float deltaSeconds = static_cast<float>(timer.Next(clock.Restart()));
    //
    // Window does this for every frame it runs, so OnFrame already gets steady
    // times. Use it in your own loop when you run frames yourself.
    class FrameTimer
    {
    public:
        // The rate the screen refreshes at, in hertz, or 0 when frames are not
        // tied to the screen, which turns the smoothing off.
        void SetRefreshRate(float hertz);

        // The seconds measured since the last frame in, the seconds to use out.
        double Next(double measuredSeconds);

        // The screen's refresh interval as followed so far, in seconds, or 0 when
        // the smoothing is off.
        double RefreshInterval() const { return Interval; }

    private:
        double StatedInterval = 0.0;
        double Interval = 0.0;
        double Carried = 0.0;
    };
}
