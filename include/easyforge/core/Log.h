#pragma once

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <utility>

namespace easyforge
{
    enum class LogLevel
    {
        Detail,
        Information,
        Warning,
        Error,
    };

    // Messages below this level are dropped. The default is Information, which
    // hides Detail messages.
    void SetLogLevel(LogLevel minimum);
    LogLevel CurrentLogLevel();

    // Writes a message. By default it goes to standard error and, on Windows,
    // to the debugger's output window.
    void Log(std::string_view message);
    void Log(LogLevel level, std::string_view message);

    // Formats the message with std::format first: Log("Loaded {} files", count).
    // A message below the current level is dropped without being formatted.
    template <typename First, typename... Rest>
    void Log(std::format_string<First, Rest...> format, First&& first, Rest&&... rest)
    {
        if (LogLevel::Information < CurrentLogLevel())
        {
            return;
        }
        Log(LogLevel::Information,
            std::string_view(std::format(format, std::forward<First>(first), std::forward<Rest>(rest)...)));
    }

    template <typename First, typename... Rest>
    void Log(LogLevel level, std::format_string<First, Rest...> format, First&& first, Rest&&... rest)
    {
        if (level < CurrentLogLevel())
        {
            return;
        }
        Log(level, std::string_view(std::format(format, std::forward<First>(first), std::forward<Rest>(rest)...)));
    }

    using LogHandler = std::function<void(LogLevel level, std::string_view message)>;

    // Sends every message to `handler` instead, for example to show them in the
    // program's own console. Pass nullptr to go back to the default. Messages
    // arrive one at a time, even when several threads log at once.
    void SetLogHandler(LogHandler handler);

    // The level's name in lowercase: "detail", "information", "warning", or "error".
    std::string_view LogLevelName(LogLevel level);
}
