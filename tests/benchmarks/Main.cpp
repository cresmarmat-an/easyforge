#include "Benchmark.h"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

#include <easyforge/core/Clock.h>

namespace benchmarks
{
    namespace
    {
        struct Entry
        {
            const char* Name;
            void (*Function)();
        };

        std::vector<Entry>& Entries()
        {
            static std::vector<Entry> entries;
            return entries;
        }

        std::string Readable(double seconds)
        {
            char text[32];
            if (seconds >= 1.0)
            {
                std::snprintf(text, sizeof(text), "%.2f s", seconds);
            }
            else if (seconds >= 0.001)
            {
                std::snprintf(text, sizeof(text), "%.3f ms", seconds * 1000.0);
            }
            else
            {
                std::snprintf(text, sizeof(text), "%.2f us", seconds * 1'000'000.0);
            }
            return text;
        }
    }

    Registration::Registration(const char* name, void (*function)())
    {
        Entries().push_back({ name, function });
    }

    void Measure(std::string_view name, int runs, const std::function<void()>& work, const std::function<void()>& prepare)
    {
        int warmUps = std::min(3, std::max(runs / 10, 1));
        for (int run = 0; run < warmUps; ++run)
        {
            if (prepare)
            {
                prepare();
            }
            work();
        }
        std::vector<double> times;
        times.reserve(static_cast<std::size_t>(runs));
        for (int run = 0; run < runs; ++run)
        {
            if (prepare)
            {
                prepare();
            }
            easyforge::Clock clock;
            work();
            times.push_back(clock.Seconds());
        }
        std::sort(times.begin(), times.end());
        std::printf("  %-58.*s %12s   fastest %s, slowest %s, %d runs\n", static_cast<int>(name.size()), name.data(),
            Readable(times[times.size() / 2]).c_str(), Readable(times.front()).c_str(), Readable(times.back()).c_str(),
            runs);
        std::fflush(stdout);
    }

    void Note(std::string_view text)
    {
        std::printf("  %.*s\n", static_cast<int>(text.size()), text.data());
        std::fflush(stdout);
    }
}

int main(int count, char** arguments)
{
    std::string filter = count > 1 ? arguments[1] : "";
#ifndef NDEBUG
    std::printf("This is a debug build: the times below say little. Build in Release to measure.\n\n");
#endif
    for (const benchmarks::Entry& entry : benchmarks::Entries())
    {
        if (!filter.empty() && std::string(entry.Name).find(filter) == std::string::npos)
        {
            continue;
        }
        std::printf("%s\n", entry.Name);
        entry.Function();
        std::printf("\n");
    }
    return 0;
}
