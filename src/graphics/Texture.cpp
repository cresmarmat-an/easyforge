#include <easyforge/graphics/Texture.h>

#include "Resources.h"

namespace easyforge
{
    Texture Texture::Load(std::string_view path, const TextureSettings& settings)
    {
        return FromImage(ImageData::Load(path), settings);
    }

    Texture Texture::FromImage(ImageData image, const TextureSettings& settings)
    {
        auto state = std::make_shared<internal::TextureState>();
        state->ErrorText = image.Error();
        state->Image = std::move(image);
        state->Settings = settings;
        return Texture(state);
    }

    Texture::Texture() : Shared(std::make_shared<internal::TextureState>())
    {
    }

    Texture::Texture(std::shared_ptr<internal::TextureState> state) : Shared(std::move(state))
    {
    }

    Texture::operator bool() const
    {
        return static_cast<bool>(Shared->Image);
    }

    const std::string& Texture::Error() const
    {
        return Shared->ErrorText;
    }

    int Texture::Width() const
    {
        return Shared->Image.Width;
    }

    int Texture::Height() const
    {
        return Shared->Image.Height;
    }

    TextureSettings Texture::Settings() const
    {
        return Shared->Settings;
    }

    void Texture::Update(ImageData image) const
    {
        if (!image)
        {
            return;
        }
        Shared->Image = std::move(image);
        Shared->ErrorText.clear();
        ++Shared->Version;
    }
}
