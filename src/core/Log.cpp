#include <easyforge/core/Log.h>

#include <atomic>
#include <cstdio>
#include <mutex>

#include "DebuggerOutput.h"

namespace easyforge
{
    namespace
    {
        struct LogState
        {
            // Recursive so that a handler which logs does not lock itself out.
            std::recursive_mutex Mutex;
            LogHandler Handler;
            std::atomic<LogLevel> Minimum = LogLevel::Information;
        };

        LogState& State()
        {
            static LogState state;
            return state;
        }

        void WriteToStandardOutputs(LogLevel level, std::string_view message)
        {
            std::string line = std::format("[{}] {}\n", LogLevelName(level), message);
            std::fwrite(line.data(), 1, line.size(), stderr);
            std::fflush(stderr);
            internal::WriteToDebugger(line);
        }
    }

    void Log(std::string_view message)
    {
        Log(LogLevel::Information, message);
    }

    void Log(LogLevel level, std::string_view message)
    {
        LogState& state = State();
        if (level < state.Minimum.load(std::memory_order_relaxed))
        {
            return;
        }

        std::lock_guard lock(state.Mutex);
        if (state.Handler)
        {
            state.Handler(level, message);
        }
        else
        {
            WriteToStandardOutputs(level, message);
        }
    }

    void SetLogHandler(LogHandler handler)
    {
        LogState& state = State();
        std::lock_guard lock(state.Mutex);
        state.Handler = std::move(handler);
    }

    void SetLogLevel(LogLevel minimum)
    {
        State().Minimum.store(minimum, std::memory_order_relaxed);
    }

    LogLevel CurrentLogLevel()
    {
        return State().Minimum.load(std::memory_order_relaxed);
    }

    std::string_view LogLevelName(LogLevel level)
    {
        switch (level)
        {
        case LogLevel::Detail:
            return "detail";
        case LogLevel::Information:
            return "information";
        case LogLevel::Warning:
            return "warning";
        case LogLevel::Error:
            return "error";
        }
        return "unknown";
    }
}
