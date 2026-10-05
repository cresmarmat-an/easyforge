#include <easyforge/core/Testing.h>
#include <easyforge/graphics.h>

#include "gpu/Gpu.h"

using namespace easyforge;

// When the GPU is reset, as when its driver is updated or it stops answering,
// a renderer starts again on a new device by itself, and textures, fonts,
// models, and shaders are sent to the new device the next time they are drawn.

namespace
{
    ImageData Checkered()
    {
        ImageData image(4, 4);
        for (int y = 0; y < 4; ++y)
        {
            for (int x = 0; x < 4; ++x)
            {
                std::size_t at = image.Stride() * static_cast<std::size_t>(y) + static_cast<std::size_t>(x) * 4;
                std::uint8_t level = (x + y) % 2 == 0 ? 255 : 30;
                image.Pixels[at] = level;
                image.Pixels[at + 1] = 80;
                image.Pixels[at + 2] = static_cast<std::uint8_t>(255 - level);
                image.Pixels[at + 3] = 255;
            }
        }
        return image;
    }
}

EASYFORGE_TEST(RenderersStartAgainAfterTheDeviceIsLost)
{
    Renderer renderer = Renderer::New({ .Adapter = GraphicsAdapter::Software, .Width = 64, .Height = 48 });
    EASYFORGE_REQUIRE(renderer);
    Texture texture = Texture::FromImage(Checkered(), { .Smooth = false });
    auto draw = [&] {
        Canvas canvas = renderer.BeginFrame(Color::Hex("#102030"));
        canvas.Rectangle({ .Position = { 4, 4 }, .Size = { 20, 12 }, .Color = Color::Hex("#F2A65A"), .CornerRadius = 3 });
        canvas.Image(texture, { .Position = { 32, 8 }, .Size = { 16, 16 } });
        renderer.EndFrame();
        return renderer.Capture();
    };
    ImageData before = draw();
    EASYFORGE_REQUIRE(before);

    Result<std::shared_ptr<internal::gpu::Device>> device = internal::gpu::SharedDevice(GraphicsAdapter::Software);
    EASYFORGE_REQUIRE(device);
    (*device)->SimulateLoss();
    EASYFORGE_REQUIRE((*device)->IsLost());

    ImageData after = draw();
    EASYFORGE_REQUIRE(after);
    EASYFORGE_EXPECT(renderer);
    EASYFORGE_EXPECT(after.Pixels == before.Pixels);

    // A new device was made, and the lost one is not handed out again.
    Result<std::shared_ptr<internal::gpu::Device>> again = internal::gpu::SharedDevice(GraphicsAdapter::Software);
    EASYFORGE_REQUIRE(again);
    EASYFORGE_EXPECT(!(*again)->IsLost());
    EASYFORGE_EXPECT((*again)->Identifier() != (*device)->Identifier());
}
