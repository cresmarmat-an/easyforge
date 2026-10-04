#include <easyforge/core/FrameTimer.h>

#include <algorithm>
#include <cmath>

namespace easyforge
{
    namespace
    {
        // A frame further than this share of a refresh from a whole number of
        // refreshes is taken as measured.
        constexpr double SnapWindow = 0.25;

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

        double total = measuredSeconds + Carried;
        double refreshes = std::round(total / Interval);
        if (refreshes >= 1.0 && refreshes <= MostRefreshes)
        {
            double snapped = refreshes * Interval;
            double leftover = total - snapped;
            if (std::abs(leftover) <= Interval * SnapWindow)
            {
                // Leftovers that keep leaning one way mean the interval is off.
                Interval += leftover / refreshes * FollowShare;
                Interval = std::clamp(Interval, StatedInterval * (1.0 - MostDrift), StatedInterval * (1.0 + MostDrift));
                double released = leftover * ReleaseShare;
                Carried = leftover - released;
                return snapped + released;
            }
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
