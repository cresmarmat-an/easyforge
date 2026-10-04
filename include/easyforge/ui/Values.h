#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <string_view>

#include <easyforge/core/Color.h>
#include <easyforge/core/Property.h>
#include <easyforge/core/Scalar.h>
#include <easyforge/core/Vector.h>

namespace easyforge::ui
{
    // No limit, for the largest size an element may take.
    inline constexpr float Unlimited = std::numeric_limits<float>::infinity();

    // A width or a height: points, a share of the parent, the space left over,
    // or as small as the content allows.
    //
    //     .Width = 200               // points
    //     .Width = ui::Percent(50)   // half of the parent, inside its padding
    //     .Width = ui::Fill          // the space the parent has left
    //     .Width = ui::Fit           // as small as the content allows
    //
    // A size left out is the element's own: Fit for most, and the size its kind
    // says otherwise, such as 200 points for a TextField.
    struct Size
    {
        enum class Kind : std::uint8_t
        {
            Default,
            Fit,
            Points,
            Percent,
            Fill,
        };

        Kind Type = Kind::Default;
        float Amount = 0.0f;

        constexpr Size() = default;
        constexpr Size(float points) : Type(Kind::Points), Amount(points) {}
        constexpr Size(Kind type, float amount) : Type(type), Amount(amount) {}

        bool operator==(const Size&) const = default;
    };

    // The space the parent has left. In a row or a column, the space the other
    // children leave is shared equally by the children that fill; across it, a
    // child that fills is as wide or tall as the row or column.
    inline constexpr Size Fill { Size::Kind::Fill, 1.0f };

    // As small as the content allows.
    inline constexpr Size Fit { Size::Kind::Fit, 0.0f };

    // A share of the parent's size inside its padding: Percent(50) is half.
    constexpr Size Percent(float percent)
    {
        return { Size::Kind::Percent, percent };
    }

    // Space on each side in points: padding inside an element, or a margin
    // around it.
    //
    //     .Padding = 16              // every side
    //     .Padding = { 16, 8 }       // left and right, then top and bottom
    //     .Padding = { 4, 8, 4, 0 }  // left, top, right, bottom
    struct Insets
    {
        float Left = 0.0f;
        float Top = 0.0f;
        float Right = 0.0f;
        float Bottom = 0.0f;

        constexpr Insets() = default;
        constexpr Insets(float all) : Left(all), Top(all), Right(all), Bottom(all) {}
        constexpr Insets(float horizontal, float vertical)
            : Left(horizontal), Top(vertical), Right(horizontal), Bottom(vertical)
        {
        }
        constexpr Insets(float left, float top, float right, float bottom) : Left(left), Top(top), Right(right), Bottom(bottom)
        {
        }

        constexpr float Horizontal() const { return Left + Right; }
        constexpr float Vertical() const { return Top + Bottom; }

        bool operator==(const Insets&) const = default;
    };

    // Where children sit across a row or a column, or inside a stack.
    enum class Alignment
    {
        Start,
        Center,
        End,

        // As wide as a column, or as tall as a row, for children that fit their
        // content. The default.
        Stretch,
    };

    // How a row or a column spreads its children along itself when nothing fills
    // the space.
    enum class Distribution
    {
        Start,
        Center,
        End,

        // The first child at the start, the last at the end, the rest evenly between.
        SpaceBetween,

        // Equal space around each child, so the ends get half as much as between.
        SpaceAround,

        // Equal space between the children and at both ends.
        SpaceEvenly,
    };

    // Where lines of text sit inside an element.
    enum class TextAlignment
    {
        Start,
        Center,
        End,
    };

    // How an animation moves from start to end.
    enum class Easing
    {
        // At the same speed all the way.
        Linear,

        // Starting slowly.
        In,

        // Slowing down at the end. The default: it feels quick and settles gently.
        Out,

        // Starting and ending slowly.
        InOut,

        // Past the end and back, like a spring.
        Spring,
    };

    // How far along an animation is, from 0 to 1, after the easing.
    float Ease(Easing easing, float progress);

    struct AnimationSettings
    {
        // In seconds.
        float Duration = 0.25f;
        ui::Easing Easing = ui::Easing::Out;

        // Seconds to wait before starting.
        float Delay = 0.0f;

        // Called when the animation reaches its end, not when something stops it.
        std::function<void()> OnFinished;
    };

    namespace internal
    {
        // Runs `step` every frame of the element or root the property belongs to,
        // with how far along the animation is, from 0 to 1, until it ends. A new
        // animation of the same property replaces the old one.
        using AnimationStarter = void (*)(const void* owner, std::string_view property,
            std::function<void(void* owner, float progress)> step, const AnimationSettings& settings);
    }

    // A property that can move to a new value over time instead of at once:
    //
    //     status.Opacity.AnimateTo(0.0f, { .Duration = 0.4f, .Easing = ui::Easing::Out });
    //
    // Assigning the property directly stops an animation of it.
    template <typename Value>
    class Animated : public Property<Value>
    {
    public:
        using typename Property<Value>::Reader;
        using typename Property<Value>::Writer;

        Animated(void* owner, Reader reader, Writer writer, std::string_view name, internal::AnimationStarter starter) noexcept
            : Property<Value>(owner, reader, writer), Owner(owner), Reading(reader), Writing(writer), Name(name),
              Starter(starter)
        {
        }

        using Property<Value>::operator=;

        void AnimateTo(const Value& target, const AnimationSettings& settings = {}) const
        {
            Reader reading = Reading;
            Writer writing = Writing;
            Starter(Owner, Name,
                [reading, writing, target, from = std::optional<Value>()](void* owner, float progress) mutable {
                    using easyforge::Lerp;
                    if (!from)
                    {
                        from = reading(owner);
                    }
                    writing(owner, Lerp(*from, target, progress));
                },
                settings);
        }

        // Points the property at a different owner, as Property::Rebind does.
        void Rebind(void* owner) noexcept
        {
            Property<Value>::Rebind(owner);
            Owner = owner;
        }

    private:
        void* Owner;
        Reader Reading;
        Writer Writing;
        std::string_view Name;
        internal::AnimationStarter Starter;
    };
}
