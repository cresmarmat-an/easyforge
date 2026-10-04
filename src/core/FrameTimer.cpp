#include <easyforge/core/FrameTimer.h>

#include <algorithm>
#include <cmath>

namespace easyforge
{
    namespace
    {
        // How close to the next whole refresh a frame must be to count as
        // reaching it, as a share of a refresh.
        constexpr double SnapWindow = 0.25;

        // The most time carried from one frame to the next, in refreshes. More
        // than that, and the frames are out of step with the screen.
        constexpr double MostCarried = 1.0;

        // The most refreshes a single frame is snapped to; longer frames are
        // stalls and are taken as measured.
        constexpr double MostRefreshes = 8.0;

        // The share of what was rounded off that each frame lets through, and how
        // strongly a steady rounding error moves the followed interval. Together
        // they settle within a few seconds and keep the output within a fraction
        // of a millisecond of even.
        constexpr double ReleaseShare = 0.05;
        constexpr double FollowShare = 0.002;

        // How far the followed interval may drift from the stated one, since a
        // screen's stated rate is rounded to whole hertz.
        constexpr double MostDrift = 0.05;
    }

    void FrameTimer::SetRefreshRate(float hertz)
    {
        if (!(hertz > 0.0f) || !std::isfinite(hertz))
        {
            StatedInterval = 0.0;
            Interval = 0.0;
            Carried = 0.0;
            return;
        }
        double stated = 1.0 / static_cast<double>(hertz);
        // The same screen keeps what was learned about its true rate; a
        // different one starts over.
        if (StatedInterval == 0.0 || std::abs(stated - StatedInterval) > StatedInterval * 0.01)
        {
            StatedInterval = stated;
            Interval = stated;
            Carried = 0.0;
            ShortFrames = 0;
        }
    }

    double FrameTimer::Next(double measuredSeconds)
    {
        if (!(measuredSeconds >= 0.0) || !std::isfinite(measuredSeconds))
        {
            measuredSeconds = 0.0;
        }
        if (Interval <= 0.0)
        {
            return measuredSeconds;
        }

        // Frames that keep coming faster than the screen refreshes are not
        // being shown in step with it, whatever was said.
        ShortFrames = measuredSeconds < Interval * (1.0 - SnapWindow) ? ShortFrames + 1 : 0;
        if (ShortFrames >= 3)
        {
            double total = std::max(measuredSeconds + Carried, 0.0);
            Carried = 0.0;
            return total;
        }

        // An extra frame within one refresh, such as one the system asks for
        // while a window is resized, is reported as measured.
        double total = measuredSeconds + Carried;
        if (total < Interval * SnapWindow)
        {
            Carried = std::min(total, 0.0);
            return std::max(total, 0.0);
        }

        // A frame counts as the whole refreshes it covers, rounding up only
        // when close to the next one. A frame that started late and the early
        // one after it are then one refresh each, as the screen showed them,
        // with the difference carried between them.
        double refreshes = std::max(1.0, std::floor(total / Interval + SnapWindow));
        double leftover = total - refreshes * Interval;
        if (refreshes <= MostRefreshes && std::abs(leftover) <= Interval * MostCarried)
        {
            // Small leftovers that keep leaning one way mean the interval is off.
            if (std::abs(leftover) <= Interval * SnapWindow)
            {
                Interval += leftover / refreshes * FollowShare;
                Interval = std::clamp(Interval, StatedInterval * (1.0 - MostDrift), StatedInterval * (1.0 + MostDrift));
            }
            double released = leftover * ReleaseShare;
            Carried = leftover - released;
            return refreshes * Interval + released;
        }

        // Out of step with the screen: report everything measured, with what
        // was carried, so the total stays true. Time is never reported as
        // negative; a deficit waits for the next frame.
        if (total < 0.0)
        {
            Carried = total;
            return 0.0;
        }
        Carried = 0.0;
        return total;
    }
}
