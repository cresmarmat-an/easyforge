#include <easyforge/core/Color.h>

#include <cmath>
#include <format>

namespace easyforge
{
    namespace
    {
        int ToByte(float component)
        {
            return static_cast<int>(Clamp(component, 0.0f, 1.0f) * 255.0f + 0.5f);
        }

        float ToLinearComponent(float value)
        {
            return value <= 0.04045f ? value / 12.92f : std::pow((value + 0.055f) / 1.055f, 2.4f);
        }

        float FromLinearComponent(float value)
        {
            return value <= 0.0031308f ? value * 12.92f : 1.055f * std::pow(value, 1.0f / 2.4f) - 0.055f;
        }
    }

    std::string Color::ToHex() const
    {
        int alpha = ToByte(Alpha);
        if (alpha == 255)
        {
            return std::format("#{:02X}{:02X}{:02X}", ToByte(Red), ToByte(Green), ToByte(Blue));
        }
        return std::format("#{:02X}{:02X}{:02X}{:02X}", ToByte(Red), ToByte(Green), ToByte(Blue), alpha);
    }

    Color Color::ToLinear() const
    {
        return { ToLinearComponent(Red), ToLinearComponent(Green), ToLinearComponent(Blue), Alpha };
    }

    Color Color::FromLinear(Color linear)
    {
        return {
            FromLinearComponent(linear.Red),
            FromLinearComponent(linear.Green),
            FromLinearComponent(linear.Blue),
            linear.Alpha,
        };
    }
}
