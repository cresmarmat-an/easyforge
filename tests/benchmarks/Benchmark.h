#pragma once

// A small timer for the benchmarks. Each benchmark sets up its work once, then
// times it with Measure, which runs it a few times to warm up and then the
// given number of times, and prints the median, the fastest, and the slowest.

#include <functional>
#include <string>
#include <string_view>

namespace benchmarks
{
    // Times `work` `runs` times. `prepare`, when given, runs before each run and
    // is not timed.
    void Measure(std::string_view name, int runs, const std::function<void()>& work,
        const std::function<void()>& prepare = {});

    // Prints a line of information next to the timings.
    void Note(std::string_view text);

    struct Registration
    {
        Registration(const char* name, void (*function)());
    };
}

// Defines a benchmark. The program runs every benchmark whose name contains
// the text given on its command line, or all of them.
#define EASYFORGE_BENCHMARK(name)                                                        \
    static void name##Benchmark();                                                       \
    static const ::benchmarks::Registration name##Registration(#name, &name##Benchmark); \
    static void name##Benchmark()
