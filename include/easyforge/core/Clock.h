#pragma once

#include <chrono>

namespace easyforge
{
    // Measures time passed since it was made or last restarted. It uses the
    // system's steady clock, which never jumps when the date or time is changed.
    class Clock
    {
    public:
        Clock() : Start(std::chrono::steady_clock::now()) {}

        double Seconds() const
        {
            return std::chrono::duration<double>(std::chrono::steady_clock::now() - Start).count();
        }

        // Returns the seconds passed and starts counting from zero again.
        double Restart()
        {
            auto now = std::chrono::steady_clock::now();
            double seconds = std::chrono::duration<double>(now - Start).count();
            Start = now;
            return seconds;
        }

    private:
        std::chrono::steady_clock::time_point Start;
    };
}
