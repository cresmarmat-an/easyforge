#pragma once

#include <memory>
#include <string>
#include <string_view>

#include <easyforge/assets/ImageData.h>

namespace easyforge
{
    namespace internal
    {
        class TextureState;
    }

    struct TextureSettings
    {
        // Blends between pixels when drawn larger or smaller. Turn it off for
        // pixel art, to keep hard edges.
        bool Smooth = true;

        // Repeats the image outside its edges, for tiling in 3D.
        bool Repeat = false;
    };

    // An image ready to draw. It is sent to the GPU the first time something
    // draws it, so textures can be loaded anywhere, before any renderer exists.
    //
    //     Texture logo = Texture::Load("logo.png");
    //     canvas.Image(logo, { .Position = { 20, 20 } });
    //
    // Texture is a handle: copies refer to the same texture.
    class Texture
    {
    public:
        static Texture Load(std::string_view path, const TextureSettings& settings = {});
        static Texture FromImage(ImageData image, const TextureSettings& settings = {});

        // No texture. Tests as false.
        Texture();

        // True when the texture has pixels.
        explicit operator bool() const;

        // Why loading failed, or empty.
        const std::string& Error() const;

        int Width() const;
        int Height() const;
        TextureSettings Settings() const;

        // Replaces the pixels, and the size if it differs. Everything drawing the
        // texture shows the new pixels from the next frame.
        void Update(ImageData image) const;

        // The shared state; used by the graphics library itself.
        const std::shared_ptr<internal::TextureState>& State() const { return Shared; }

    private:
        explicit Texture(std::shared_ptr<internal::TextureState> state);

        std::shared_ptr<internal::TextureState> Shared;
    };
}
