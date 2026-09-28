#include <easyforge/graphics/Model.h>

#include <easyforge/core/Log.h>

#include "Resources.h"

namespace easyforge
{
    Model Model::Load(std::string_view path)
    {
        return FromData(ModelData::Load(path));
    }

    Model Model::FromData(ModelData data)
    {
        auto state = std::make_shared<internal::ModelState>();
        state->ErrorText = data.Error();
        for (const MaterialData& material : data.Materials)
        {
            Texture texture;
            if (!material.BaseColorTexture.empty())
            {
                texture = Texture::Load(material.BaseColorTexture, { .Smooth = true, .Repeat = true });
                if (!texture)
                {
                    Log(LogLevel::Warning, "The model's material {} is drawn without its texture: {}", material.Name,
                        texture.Error());
                }
            }
            state->MaterialTextures.push_back(texture);
        }
        state->Data = std::move(data);
        return Model(state);
    }

    Model::Model() : Shared(std::make_shared<internal::ModelState>())
    {
    }

    Model::Model(std::shared_ptr<internal::ModelState> state) : Shared(std::move(state))
    {
    }

    Model::operator bool() const
    {
        return static_cast<bool>(Shared->Data);
    }

    const std::string& Model::Error() const
    {
        return Shared->ErrorText;
    }

    BoundingBox Model::Bounds() const
    {
        return Shared->Data.Bounds();
    }

    const ModelData& Model::Data() const
    {
        return Shared->Data;
    }
}
