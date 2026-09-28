#include <easyforge/graphics/Shader.h>

#include <cstring>

#include <easyforge/assets/Files.h>
#include <easyforge/core/Log.h>

#include "Resources.h"

namespace easyforge
{
    ShaderValue::ShaderValue(std::string name, float value) : Name(std::move(name)), Numbers { value }, Count(1)
    {
    }

    ShaderValue::ShaderValue(std::string name, Vector2 value) : Name(std::move(name)), Numbers { value.X, value.Y }, Count(2)
    {
    }

    ShaderValue::ShaderValue(std::string name, Vector3 value)
        : Name(std::move(name)), Numbers { value.X, value.Y, value.Z }, Count(3)
    {
    }

    ShaderValue::ShaderValue(std::string name, Vector4 value)
        : Name(std::move(name)), Numbers { value.X, value.Y, value.Z, value.W }, Count(4)
    {
    }

    ShaderValue::ShaderValue(std::string name, Color value)
        : Name(std::move(name)), Numbers { value.Red, value.Green, value.Blue, value.Alpha }, Count(4)
    {
    }

    namespace internal
    {
        gpu::Pipeline* ShaderState::On(const std::shared_ptr<gpu::Device>& device)
        {
            if (!Made || Refused)
            {
                return nullptr;
            }
            DeviceCopy<gpu::Pipeline>& copy = Pipelines[device->Identifier()];
            if (copy.Object)
            {
                return copy.Object.get();
            }
            Result<std::unique_ptr<gpu::Pipeline>> pipeline = device->CreatePipeline({
                .VertexShader = Compiled.Code,
                .PixelShader = Compiled.Code,
                .TriangleStrip = true,
                .Blend = gpu::Blending::Premultiplied,
                .Name = Name,
            });
            if (!pipeline)
            {
                Refused = true;
                Log(LogLevel::Error, "The shader {} could not be used: {}", Name, pipeline.Error());
                return nullptr;
            }
            copy.Device = device;
            copy.Object = std::move(pipeline).Get();
            return copy.Object.get();
        }

        std::vector<std::uint8_t> ShaderState::Constants(const ShaderFrameConstants& frame,
            const std::vector<ShaderValue>& values) const
        {
            std::vector<std::uint8_t> bytes(sizeof(ShaderFrameConstants) + Compiled.Values.size() * 16, 0);
            std::memcpy(bytes.data(), &frame, sizeof(frame));
            for (std::size_t index = 0; index < Compiled.Values.size(); ++index)
            {
                const ShaderValueSlot& slot = Compiled.Values[index];
                std::array<float, 4> numbers = slot.Default;
                for (const ShaderValue& value : values)
                {
                    if (value.Name != slot.Name)
                    {
                        continue;
                    }
                    if (value.Count != ComponentCount(slot.Type) && !(value.Count == 1 && slot.Type == ShaderType::Boolean))
                    {
                        Log(LogLevel::Warning, "The shader {} expects {} to be a {}; the value given was left out", Name,
                            slot.Name, Spelling(slot.Type));
                        continue;
                    }
                    std::copy(value.Numbers, value.Numbers + 4, numbers.begin());
                }
                std::memcpy(bytes.data() + sizeof(ShaderFrameConstants) + index * 16, numbers.data(), 16);
            }
            return bytes;
        }
    }

    Shader Shader::Load(std::string_view path)
    {
        Result<std::vector<std::uint8_t>> bytes = Files::Read(path);
        if (!bytes)
        {
            auto state = std::make_shared<internal::ShaderState>();
            state->Name = std::string(path);
            state->ErrorText = bytes.Error();
            return Shader(state);
        }
        std::string source(bytes->begin(), bytes->end());
        return FromText(source, path);
    }

    Shader Shader::FromText(std::string_view source, std::string_view name)
    {
        auto state = std::make_shared<internal::ShaderState>();
        state->Name = std::string(name);
        Result<internal::CompiledShader> compiled = internal::CompileShader(source, name);
        if (!compiled)
        {
            state->ErrorText = compiled.Error();
            return Shader(state);
        }
        state->Compiled = std::move(compiled).Get();
        state->Made = true;
        return Shader(state);
    }

    Shader::Shader() : Shared(std::make_shared<internal::ShaderState>())
    {
    }

    Shader::Shader(std::shared_ptr<internal::ShaderState> state) : Shared(std::move(state))
    {
    }

    Shader::operator bool() const
    {
        return Shared->Made;
    }

    const std::string& Shader::Error() const
    {
        return Shared->ErrorText;
    }

    std::vector<std::string> Shader::ValueNames() const
    {
        std::vector<std::string> names;
        for (const internal::ShaderValueSlot& slot : Shared->Compiled.Values)
        {
            names.push_back(slot.Name);
        }
        return names;
    }

    const std::string& Shader::GeneratedCode() const
    {
        return Shared->Compiled.Code;
    }
}
