// easyforge-shader checks shaders written in the easyforge shader language. The
// easyforge_add_shaders CMake function runs it when a program is built, so a
// shader with a problem stops the build with the file, line, and column.
//
//     easyforge-shader ripple.shader glow.shader
//     easyforge-shader --code ripple.shader        prints the code the GPU gets

#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <easyforge/graphics.h>

using namespace easyforge;

int main(int argumentCount, char** arguments)
{
    std::vector<std::string> files;
    bool printCode = false;
    for (int index = 1; index < argumentCount; ++index)
    {
        std::string_view argument = arguments[index];
        if (argument == "--code")
        {
            printCode = true;
        }
        else
        {
            files.emplace_back(argument);
        }
    }
    if (files.empty())
    {
        std::fputs("usage: easyforge-shader [--code] <file.shader>...\n", stderr);
        return 2;
    }

    // The GPU's own compiler reports through the log; anything it refuses is a failure.
    std::vector<std::string> refused;
    SetLogHandler([&refused](LogLevel level, std::string_view message) {
        if (level == LogLevel::Error)
        {
            refused.emplace_back(message);
        }
    });

    Renderer renderer = Renderer::New({ .Adapter = GraphicsAdapter::Software, .Width = 4, .Height = 4 });
    int failures = 0;
    for (const std::string& file : files)
    {
        Shader shader = Shader::Load(file);
        if (!shader)
        {
            std::fprintf(stderr, "%s\n", shader.Error().c_str());
            ++failures;
            continue;
        }
        if (renderer)
        {
            refused.clear();
            Canvas canvas = renderer.BeginFrame();
            canvas.Shaded(shader, { 0, 0, 4, 4 });
            renderer.EndFrame();
            for (const std::string& message : refused)
            {
                std::fprintf(stderr, "%s: %s\n", file.c_str(), message.c_str());
            }
            failures += refused.empty() ? 0 : 1;
        }
        if (printCode)
        {
            std::fputs(shader.GeneratedCode().c_str(), stdout);
        }
    }
    return failures == 0 ? 0 : 1;
}
