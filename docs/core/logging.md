# Logging

```cpp
#include <easyforge/core.h>

using namespace easyforge;

Log("starting");
Log("loaded {} files from {}", count, folder);
Log(LogLevel::Warning, "{} is larger than 4096 pixels", name);
Log(LogLevel::Error, result.Error());
```

By default each message goes to standard error as a line like
`[warning] icon.png is larger than 4096 pixels`. On Windows it also goes to the
debugger's output window when a debugger is attached, which is where messages
from a program without a console can be read.

## Levels

| Level | For |
|---|---|
| `LogLevel::Detail` | Step-by-step detail while tracking down a problem |
| `LogLevel::Information` | What the program is doing. `Log(message)` uses this level |
| `LogLevel::Warning` | Something unexpected that the program recovered from |
| `LogLevel::Error` | Something that failed |

Messages below the current level are dropped before they are formatted, so a
`Detail` message costs almost nothing while it is hidden.

```cpp
SetLogLevel(LogLevel::Detail);          // show everything
SetLogLevel(LogLevel::Warning);         // only warnings and errors
LogLevel level = CurrentLogLevel();     // Information unless changed
std::string_view name = LogLevelName(LogLevel::Warning);   // "warning"
```

## Formatting

With arguments after the message, `Log` formats with `std::format`, so every
format `std::format` accepts works, including easyforge's vectors and colors.
The format string is checked when the program is compiled.

Without arguments the message is printed as it is, braces included:
`Log("{}")` prints `{}`. Use `Log("{}", text)` to print text that might contain
braces from outside the program.

## Sending messages somewhere else

```cpp
SetLogHandler([](LogLevel level, std::string_view message) {
    console.Add(std::format("{}: {}", LogLevelName(level), message));
});

SetLogHandler(nullptr);     // back to standard error and the debugger
```

A handler replaces the default output for the whole program. Messages reach it
one at a time, even when several threads log at once. It may call `Log` itself
without locking up. Keep it quick: other threads wait while it runs.

## Limitations

- There is one handler for the whole program. To send messages to several
  places, have the handler do so.
- There is no built-in file output; write one with a handler.
- Messages carry no timestamp or thread name. A handler can add them.
