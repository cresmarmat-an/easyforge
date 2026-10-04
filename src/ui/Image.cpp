#include <easyforge/ui/Elements.h>

#include <easyforge/core/Log.h>

#include "Behavior.h"
#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        class ImageBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Image::KindName; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Fit")
                {
                    return DataValue("contain");
                }
                if (property == "Tint")
                {
                    return DataValue(Color::White);
                }
                if (property == "Slice")
                {
                    return DataValue(0.0);
                }
                if (property == "Source")
                {
                    return DataValue("");
                }
                return Behavior::Default(property);
            }

            Vector2 MeasureContent(ElementState& element, Context&, const Constraint& constraint) override
            {
                easyforge::Texture texture = element.Object<easyforge::Texture>("Texture");
                if (!texture)
                {
                    return {};
                }
                Vector2 natural { static_cast<float>(texture.Width()), static_cast<float>(texture.Height()) };
                // With one side given, the other keeps the picture's shape.
                if (constraint.ExactWidth && !constraint.ExactHeight && natural.X > 0.0f)
                {
                    return { constraint.Available.X, constraint.Available.X * natural.Y / natural.X };
                }
                if (constraint.ExactHeight && !constraint.ExactWidth && natural.Y > 0.0f)
                {
                    return { constraint.Available.Y * natural.X / natural.Y, constraint.Available.Y };
                }
                return natural;
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                easyforge::Texture texture = element.Object<easyforge::Texture>("Texture");
                const Style& style = element.CurrentStyle();
                Rectangle content { element.Frame.X + style.Padding.Left, element.Frame.Y + style.Padding.Top,
                    element.Frame.Width - style.Padding.Horizontal(), element.Frame.Height - style.Padding.Vertical() };
                Color tint = element.Get("Tint").AsColor();
                if (!element.IsEnabled())
                {
                    tint = tint.WithAlpha(tint.Alpha * 0.4f);
                }
                DrawPicture(context.Canvas, texture, content, FromData<ImageFit>(element.Get("Fit")),
                    element.Get("Slice").As<float>(), tint, style.CornerRadius.value_or(0.0f));
            }
        };

        void LoadPicture(ElementState& element, std::string_view path)
        {
            element.Set("Source", DataValue(path));
            if (path.empty())
            {
                element.ClearObject("Texture");
            }
            else
            {
                easyforge::Texture texture = easyforge::Texture::Load(path);
                if (!texture)
                {
                    Log(LogLevel::Warning, "ui::Image: {}", texture.Error());
                }
                element.SetObject("Texture", texture);
            }
            element.Changed(true);
        }

        void ApplyImageSettings(ElementState& element, const ImageSettings& settings)
        {
            const ImageSettings defaults {};
            ApplyCommonSettings(element, settings);
            Choose(element, "Fit", settings.Fit, defaults.Fit);
            Choose(element, "Tint", settings.Tint, defaults.Tint);
            Choose(element, "Slice", settings.Slice, defaults.Slice);
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Image::Image(std::string_view path, const ImageSettings& settings) : Image(MakeElement(std::make_unique<ImageBehavior>()))
    {
        ApplyImageSettings(*State, settings);
        if (!path.empty())
        {
            LoadPicture(*State, path);
        }
    }

    Image::Image(const easyforge::Texture& texture, const ImageSettings& settings)
        : Image(MakeElement(std::make_unique<ImageBehavior>()))
    {
        ApplyImageSettings(*State, settings);
        State->SetObject("Texture", texture);
    }

    Image::Image(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)),
          Source(
              &State,
              [](const void* owner) -> std::string {
                  ElementState* element = StateOf(owner);
                  return element ? element->Get("Source").AsText() : std::string();
              },
              [](void* owner, const std::string& path) {
                  if (ElementState* element = StateOf(owner))
                  {
                      LoadPicture(*element, path);
                  }
              }),
          Texture(Kept<easyforge::Texture, "Texture", true>(State)), Fit(Stored<ImageFit, "Fit">(State)),
          Tint(Stored<easyforge::Color, "Tint">(State)), Slice(Stored<float, "Slice">(State))
    {
    }

    Image::Image(const Image& other) : Image(other.State)
    {
    }

    Image& Image::operator=(const Image& other)
    {
        State = other.State;
        return *this;
    }
}
