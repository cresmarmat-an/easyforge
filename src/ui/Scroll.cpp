#include <easyforge/ui/Layout.h>

#include <cmath>

#include "Behavior.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        // Points one notch of the wheel moves.
        constexpr float WheelStep = 48.0f;

        class ScrollBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Scroll::KindName; }
            bool HoldsChildren() const override { return true; }
            bool ClipsChildren(ElementState&) const override { return true; }
            bool TakesPointer(ElementState&) const override { return true; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Direction")
                {
                    return DataValue("vertical");
                }
                if (property == "Position")
                {
                    return DataValue(0.0);
                }
                return Behavior::Default(property);
            }

            bool Horizontal(ElementState& element) const
            {
                return FromData<ScrollDirection>(element.Get("Direction")) == ScrollDirection::Horizontal;
            }

            float Farthest() const { return Max(ContentLength - ViewLength, 0.0f); }

            // A point on the bar belongs to the scroll, even over a child.
            bool ClaimsPoint(ElementState& element, Vector2 point) const override { return OnBar(element, point); }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint) override
            {
                return MeasureLine(element, context, constraint, Horizontal(element));
            }

            void Arrange(ElementState& element, Context& context, Rectangle content) override
            {
                bool horizontal = Horizontal(element);
                ViewLength = horizontal ? content.Width : content.Height;

                // Percent sizes along the scroll are shares of the view.
                Constraint constraint;
                constraint.Available = content.Size();
                constraint.ExactWidth = !horizontal;
                constraint.ExactHeight = horizontal;
                Vector2 length = MeasureLine(element, context, constraint, horizontal);
                ContentLength = horizontal ? length.X : length.Y;
                Area = content;
                Laid = true;

                // A position asked for before this layout is kept to the new limits.
                Target = Clamp(Target, 0.0f, Farthest());
                float position = Clamp(element.Get("Position").As<float>(), 0.0f, Farthest());
                Rectangle whole = horizontal ? Rectangle { content.X, content.Y, Max(ContentLength, content.Width), content.Height }
                                             : Rectangle { content.X, content.Y, content.Width, Max(ContentLength, content.Height) };
                ArrangeLine(element, context, whole, horizontal, -position, ViewLength);

                // An element asked to be shown is placed now.
                if (std::shared_ptr<ElementState> target = Revealing.lock())
                {
                    Revealing.reset();
                    if (IsInside(target.get(), &element))
                    {
                        float start = horizontal ? target->Frame.X - Area.X : target->Frame.Y - Area.Y;
                        float size = horizontal ? target->Frame.Width : target->Frame.Height;
                        if (start < 0.0f)
                        {
                            Target = Clamp(position + start, 0.0f, Farthest());
                        }
                        else if (start + size > ViewLength)
                        {
                            Target = Clamp(position + start + size - ViewLength, 0.0f, Farthest());
                        }
                        Velocity = 0.0f;
                    }
                }
            }

            void MoveTo(ElementState& element, float position)
            {
                position = Clamp(position, 0.0f, Farthest());
                Written = position;
                if (element.Get("Position").As<float>() != position)
                {
                    element.Set("Position", DataValue(position));
                }
            }

            void Update(ElementState& element, Context& context) override
            {
                float position = element.Get("Position").As<float>();
                if (position != Written)
                {
                    // Something else set the position; it wins.
                    Target = position;
                    Written = position;
                }

                // Until the first layout the limits are unknown, so nothing moves.
                if (!Laid)
                {
                    return;
                }
                float seconds = context.DeltaSeconds;
                if (!Dragging && Velocity != 0.0f)
                {
                    Target = Clamp(Target + Velocity * seconds, 0.0f, Farthest());
                    Velocity *= std::exp(-4.0f * seconds);
                    if (std::abs(Velocity) < 5.0f || Target <= 0.0f || Target >= Farthest())
                    {
                        Velocity = 0.0f;
                    }
                }
                // The target may lie past content added since the last layout, so
                // it is not cut to the old limits; the next layout does that.
                float goal = Clamp(Target, 0.0f, Farthest());
                if (position != goal)
                {
                    float next = position + (goal - position) * Min(seconds * 18.0f, 1.0f);
                    if (std::abs(goal - next) < 0.5f || Dragging)
                    {
                        next = goal;
                    }
                    MoveTo(element, next);
                    Shown = 1.0f;
                }
                Shown = Max(Shown - seconds * 1.5f, 0.0f);
            }

            // The bar's thumb: where it starts along the view, and how long it is.
            void Thumb(float& start, float& length) const
            {
                length = Max(ViewLength * ViewLength / Max(ContentLength, 1.0f), 24.0f);
                float room = Max(ViewLength - length, 0.0f);
                start = Farthest() > 0.0f ? room * Clamp(Written / Farthest(), 0.0f, 1.0f) : 0.0f;
            }

            bool OnBar(ElementState& element, Vector2 point) const
            {
                if (Farthest() <= 0.0f)
                {
                    return false;
                }
                return Horizontal(element) ? point.Y >= element.Frame.Bottom() - 12.0f : point.X >= element.Frame.Right() - 12.0f;
            }

            void DrawOver(ElementState& element, DrawContext& context) override
            {
                if (Farthest() <= 0.0f)
                {
                    return;
                }
                float visible = Max(Max(Shown, element.HoverAmount), DraggingBar ? 1.0f : 0.0f);
                if (visible <= 0.01f)
                {
                    return;
                }
                float start = 0.0f;
                float length = 0.0f;
                Thumb(start, length);
                float thickness = DraggingBar || BarHovered ? 8.0f : 4.0f;
                Color color = context.Theme->MutedText.WithAlpha(0.6f * visible);
                const Rectangle& frame = element.Frame;
                if (Horizontal(element))
                {
                    context.Canvas.Rectangle({ .Position = { Area.X + start, frame.Bottom() - thickness - 2.0f },
                        .Size = { length, thickness }, .Color = color, .CornerRadius = thickness * 0.5f });
                }
                else
                {
                    context.Canvas.Rectangle({ .Position = { frame.Right() - thickness - 2.0f, Area.Y + start },
                        .Size = { thickness, length }, .Color = color, .CornerRadius = thickness * 0.5f });
                }
            }

            bool Wheel(ElementState& element, Context&, Vector2 amount) override
            {
                float delta = Horizontal(element) ? amount.X - amount.Y : -amount.Y;
                if (delta == 0.0f || Farthest() <= 0.0f)
                {
                    return false;
                }
                float before = Target;
                Target = Clamp(Target + delta * WheelStep, 0.0f, Farthest());
                Velocity = 0.0f;
                Shown = 1.0f;
                return Target != before || (delta < 0.0f ? before > 0.0f : before < Farthest());
            }

            float Along(ElementState& element, Vector2 point) const { return Horizontal(element) ? point.X : point.Y; }

            void PointerPressed(ElementState& element, Context& context, const Pointer& pointer) override
            {
                Velocity = 0.0f;
                if (OnBar(element, pointer.Position))
                {
                    float start = 0.0f;
                    float length = 0.0f;
                    Thumb(start, length);
                    float thumbStart = (Horizontal(element) ? Area.X : Area.Y) + start;
                    float along = Along(element, pointer.Position);
                    DraggingBar = true;
                    Grip = along >= thumbStart && along <= thumbStart + length ? along - thumbStart : length * 0.5f;
                    FollowBar(element, along);
                }
                else if (pointer.Touch)
                {
                    Dragging = true;
                    Last = Along(element, pointer.Position);
                    LastTime = context.Time;
                }
            }

            void FollowBar(ElementState& element, float along)
            {
                float start = 0.0f;
                float length = 0.0f;
                Thumb(start, length);
                float room = Max(ViewLength - length, 1.0f);
                float fraction = Clamp((along - Grip - (Horizontal(element) ? Area.X : Area.Y)) / room, 0.0f, 1.0f);
                Target = fraction * Farthest();
                MoveTo(element, Target);
            }

            void PointerMoved(ElementState& element, Context& context, const Pointer& pointer) override
            {
                BarHovered = OnBar(element, pointer.Position);
                if (!element.Pressed)
                {
                    return;
                }
                float along = Along(element, pointer.Position);
                if (DraggingBar)
                {
                    FollowBar(element, along);
                }
                else if (Dragging)
                {
                    // The speed for a fling, smoothed over the last few moves.
                    float moved = along - Last;
                    float seconds = Max(context.Time - LastTime, 1.0f / 120.0f);
                    Velocity = Velocity * 0.6f - moved / seconds * 0.4f;
                    Last = along;
                    LastTime = context.Time;
                    Target = Clamp(Target - moved, 0.0f, Farthest());
                    MoveTo(element, Target);
                    Shown = 1.0f;
                }
            }

            void PointerReleased(ElementState&, Context& context, const Pointer&, bool) override
            {
                // A finger that rested before lifting does not fling.
                if (Dragging && context.Time - LastTime > 0.1f)
                {
                    Velocity = 0.0f;
                }
                DraggingBar = false;
                Dragging = false;
            }

            bool KeyPressed(ElementState& element, Context&, const Event& event) override
            {
                switch (event.Key)
                {
                case Key::PageDown: Target += ViewLength * 0.9f; break;
                case Key::PageUp: Target -= ViewLength * 0.9f; break;
                case Key::Home:
                    if (!event.Modifiers.Control)
                    {
                        return false;
                    }
                    Target = 0.0f;
                    break;
                case Key::End:
                    if (!event.Modifiers.Control)
                    {
                        return false;
                    }
                    Target = Farthest();
                    break;
                default: return false;
                }
                (void)element;
                Target = Clamp(Target, 0.0f, Farthest());
                Shown = 1.0f;
                return true;
            }

            float ContentLength = 0.0f;
            float ViewLength = 0.0f;
            Rectangle Area;
            float Target = 0.0f;
            float Written = 0.0f;
            float Velocity = 0.0f;
            float Shown = 0.0f;
            bool Dragging = false;
            bool DraggingBar = false;
            bool BarHovered = false;
            float Grip = 0.0f;
            float Last = 0.0f;
            float LastTime = 0.0f;
            bool Laid = false;
            std::weak_ptr<ElementState> Revealing;
        };

        ScrollBehavior* ScrollOf(const std::shared_ptr<ElementState>& state)
        {
            return state ? dynamic_cast<ScrollBehavior*>(state->Kind.get()) : nullptr;
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Scroll::Scroll(const ScrollSettings& settings) : Scroll(MakeElement(std::make_unique<ScrollBehavior>()))
    {
        const ScrollSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "Gap", settings.Gap, defaults.Gap);
        Choose(*State, "Alignment", settings.Alignment, defaults.Alignment);
        Choose(*State, "Direction", settings.Direction, defaults.Direction);
        AddChildren(*State, settings.Children);
    }

    Scroll::Scroll(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)), Gap(Stored<float, "Gap">(State)), Alignment(Stored<ui::Alignment, "Alignment">(State)),
          Direction(Stored<ScrollDirection, "Direction">(State)), Position(Stored<float, "Position">(State))
    {
    }

    Scroll::Scroll(const Scroll& other) : Scroll(other.State)
    {
    }

    Scroll& Scroll::operator=(const Scroll& other)
    {
        State = other.State;
        return *this;
    }

    void Scroll::ScrollTo(float position) const
    {
        if (ScrollBehavior* scroll = ScrollOf(State))
        {
            // Kept as asked: content added just before may not be laid out yet.
            scroll->Target = Max(position, 0.0f);
            scroll->Velocity = 0.0f;
        }
    }

    void Scroll::ScrollIntoView(const Element& element) const
    {
        ScrollBehavior* scroll = ScrollOf(State);
        const std::shared_ptr<ElementState>& target = StateOfHandle(element);
        if (!scroll || !target || !IsInside(target.get(), State.get()))
        {
            return;
        }
        // The element may have been added just now, so it is placed first: the
        // next layout scrolls to it.
        scroll->Revealing = target;
        State->Changed(true);
    }
}
