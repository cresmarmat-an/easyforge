// easyforge-script runs scripts in the easyforge language, checks them, or opens
// a prompt to try the language out.
//
//     easyforge-script greet.script          runs the file
//     easyforge-script check greet.script    checks it, including its types, without running it
//     easyforge-script check --names a.script   reports names nothing defines too
//     easyforge-script                       opens a prompt

#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <easyforge/core/language/Syntax.h>
#include <easyforge/script.h>

using namespace easyforge;

namespace
{
    int RunFile(const std::string& path)
    {
        ScriptEngine scripts = ScriptEngine::New();
        Result<ScriptValue> ran = scripts.RunFile(path);
        if (!ran)
        {
            std::fprintf(stderr, "%s\n", ran.Error().c_str());
            return 1;
        }

        // Functions started with spawn carry on, a frame at a time, until they end.
        using Clock = std::chrono::steady_clock;
        Clock::time_point last = Clock::now();
        while (scripts.RunningTasks() > 0)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            Clock::time_point now = Clock::now();
            Result<> updated = scripts.Update(std::chrono::duration<float>(now - last).count());
            last = now;
            if (!updated)
            {
                std::fprintf(stderr, "%s\n", updated.Error().c_str());
                return 1;
            }
        }
        return 0;
    }

    int Check(const std::vector<std::string>& paths, bool unknownNames)
    {
        ScriptEngine scripts = ScriptEngine::New();
        int failures = 0;
        for (const std::string& path : paths)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file)
            {
                std::fprintf(stderr, "cannot read %s\n", path.c_str());
                ++failures;
                continue;
            }
            std::ostringstream contents;
            contents << file.rdbuf();
            std::vector<std::string> problems = scripts.Check(contents.str(), path, unknownNames);
            for (const std::string& problem : problems)
            {
                std::fprintf(stderr, "%s\n", problem.c_str());
            }
            failures += problems.empty() ? 0 : 1;
        }
        return failures == 0 ? 0 : 1;
    }

    // True when the text stops in the middle of a block, so the prompt waits for
    // more lines.
    bool Unfinished(const std::string& text)
    {
        for (const language::Problem& problem : language::Parse(text).Problems)
        {
            if (problem.Message.find("the end of the file") != std::string::npos)
            {
                return true;
            }
        }
        return false;
    }

    int Prompt()
    {
        std::puts("The easyforge script prompt. Type a line, or a whole function; end the input to leave.");
        ScriptEngine scripts = ScriptEngine::New();
        std::string pending;
        std::string line;
        while (true)
        {
            std::fputs(pending.empty() ? "> " : "... ", stdout);
            std::fflush(stdout);
            if (!std::getline(std::cin, line))
            {
                std::puts("");
                return 0;
            }
            // Some shells start piped input with a byte order mark.
            if (line.starts_with("\xEF\xBB\xBF"))
            {
                line.erase(0, 3);
            }
            pending += line + "\n";
            if (Unfinished(pending))
            {
                continue;
            }
            // A lone value, such as 1 + 2 or Twice(21), is shown.
            language::SyntaxTree tree = language::Parse(pending);
            bool lone = tree.Statements.size() == 1 && tree.Statements[0]->Kind == language::StatementKind::Expression;
            Result<ScriptValue> ran = scripts.Run(lone ? "return " + pending : pending, "prompt");
            if (!ran && ran.Error().find("this does nothing") != std::string::npos)
            {
                ran = scripts.Run("return " + pending, "prompt");
            }
            pending.clear();
            if (!ran)
            {
                std::printf("%s\n", ran.Error().c_str());
            }
            else if (!ran->IsNothing())
            {
                std::printf("%s\n", ran->AsText().c_str());
            }
            scripts.Update(0.0f);
        }
    }
}

int main(int argumentCount, char** arguments)
{
    if (argumentCount <= 1)
    {
        return Prompt();
    }
    std::string_view first = arguments[1];
    if (first == "check")
    {
        // The names a program defines are not known here, so they are only
        // reported when asked for.
        std::vector<std::string> paths(arguments + 2, arguments + argumentCount);
        bool unknownNames = !paths.empty() && paths.front() == "--names";
        if (unknownNames)
        {
            paths.erase(paths.begin());
        }
        if (paths.empty())
        {
            std::fputs("usage: easyforge-script check [--names] <file.script>...\n", stderr);
            return 2;
        }
        return Check(paths, unknownNames);
    }
    if (first == "--help" || argumentCount > 2)
    {
        std::fputs("usage: easyforge-script <file.script>        runs it\n"
                   "       easyforge-script check [--names] <file.script>  checks it without running it\n"
                   "       easyforge-script                      opens a prompt\n",
            stderr);
        return first == "--help" ? 0 : 2;
    }
    return RunFile(std::string(first));
}