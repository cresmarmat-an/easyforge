#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>

#include "Damage.h"

using namespace easyforge;
using namespace easyforge::testing;

// Damaged shaders are checked and turned into the GPU's language without
// crashing or hanging; a damaged shader that still passes the check must give
// code too.

namespace
{
    constexpr const char* Everything = R"(-- every kind of shader statement
value Speed: number = 1
value Tint: color = #FFFFFF
value Center: vector2 = vector2(0.5, 0.5)

function Ripple(at: vector2, time: number) returns number then
    variable total = 0
    for index in 1 to 3 then
        total = total + Sine(Length(at - Center) * 20 * index - time * Speed) / index
    end
    if total > 1 then
        return 1
    else if total < -1 then
        return -1
    end
    return total
end

function Pixel(input: PixelInput) returns color then
    constant wave = Ripple(input.Coordinates, input.Time)
    constant moved = input.Coordinates + vector2(0, wave * 0.01)
    return Sample(input.Content, moved) * Tint
end
)";

    std::string ReadFile(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::ostringstream text;
        text << file.rdbuf();
        return text.str();
    }
}

EASYFORGE_TEST(DamagedShadersNeverCrash)
{
    std::filesystem::path folder = std::filesystem::path(EASYFORGE_TEST_SHADERS);
    std::uint64_t seed = 500;
    for (const std::string& source : { std::string(Everything), ReadFile(folder / "ripple.shader"), ReadFile(folder / "broken.shader") })
    {
        Damage(
            source,
            [](const std::string& damaged) {
                Shader shader = Shader::FromText(damaged, "damaged.shader");
                if (shader)
                {
                    EASYFORGE_EXPECT(!shader.GeneratedCode().empty());
                }
            },
            seed++, 400);
    }
}
