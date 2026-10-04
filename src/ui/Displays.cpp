#include <easyforge/ui/Displays.h>

#include <easyforge/core/Log.h>

#include "Behavior.h"
#include "Containers.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui
{
    Transition Transition::Cut()
    {
        return {};
    }

    Transition Transition::Fade(float seconds)
    {
        Transition transition;
        transition.Type = Kind::Fade;
        transition.Seconds = seconds;
        return transition;
    }

    Transition Transition::Slide(float seconds, SlideFrom from)
    {
        Transition transition;
        transition.Type = Kind::Slide;
        transition.Seconds = seconds;
        transition.From = from;
        return transition;
    }

    Transition Transition::Scale(float seconds)
    {
        Transition transition;
        transition.Type = Kind::Scale;
        transition.Seconds = seconds;
        return transition;
    }

    Transition Transition::Shader(const easyforge::Shader& shader, float seconds)
    {
        Transition transition;
        transition.Type = Kind::Shader;
        transition.Seconds = seconds;
        transition.Effect = shader;
        return transition;
    }

    Transition Transition::Reversed() const
    {
        Transition reversed = *this;
        switch (From)
        {
        case SlideFrom::Right: reversed.From = SlideFrom::Left; break;
        case SlideFrom::Left: reversed.From = SlideFrom::Right; break;
        case SlideFrom::Bottom: reversed.From = SlideFrom::Top; break;
        case SlideFrom::Top: reversed.From = SlideFrom::Bottom; break;
        }
        return reversed;
    }
}

namespace easyforge::ui::internal
{
    namespace
    {
        void Call(ElementState* element, std::string_view callback)
        {
            if (!element)
            {
                return;
            }
            std::function<void()> function = element->Object<std::function<void()>>(callback);
            if (function)
            {
                std::shared_ptr<ElementState> kept = element->shared_from_this();
                function();
            }
        }

        class DisplayBehavior final : public LineBehavior
        {
        public:
            DisplayBehavior() : LineBehavior(Display::KindName, false) {}
        };

        class DisplaysBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Displays::KindName; }
            bool HoldsChildren() const override { return true; }

            DataValue Default(std::string_view property) const override
            {
                if (property == "Width" || property == "Height")
                {
                    return DataValue("fill");
                }
                if (property == "Current")
                {
                    return DataValue("");
                }
                return Behavior::Default(property);
            }

            ElementState* Named(ElementState& element, std::string_view name) const
            {
                for (const std::shared_ptr<ElementState>& child : element.Children)
                {
                    if (child->Row.Name() == name)
                    {
                        return child.get();
                    }
                }
                return nullptr;
            }

            ElementState* Shown(ElementState& element) const
            {
                std::string current = element.Get("Current").AsText();
                ElementState* shown = Named(element, current);
                if (!shown && current.empty() && !element.Children.empty())
                {
                    shown = element.Children.front().get();
                }
                return shown;
            }

            void VisibleChildren(ElementState& element, std::vector<ElementState*>& into) override
            {
                ElementState* shown = Shown(element);
                ElementState* leaving = Leaving.lock().get();
                if (Active && leaving && leaving != shown && leaving->Parent == &element)
                {
                    into.push_back(leaving);
                }
                if (shown && shown->CurrentStyle().Visible)
                {
                    into.push_back(shown);
                }
            }

            void InteractiveChildren(ElementState& element, std::vector<ElementState*>& into) override
            {
                ElementState* shown = Shown(element);
                if (shown && shown->CurrentStyle().Visible)
                {
                    into.push_back(shown);
                }
            }

            bool ClipsChildren(ElementState&) const override
            {
                return Active && (Moving.Type == Transition::Kind::Slide || Moving.Type == Transition::Kind::Scale);
            }

            void Update(ElementState&, Context& context) override
            {
                if (!Active)
                {
                    return;
                }
                Progress += Moving.Seconds > 0.0f ? context.DeltaSeconds / Moving.Seconds : 1.0f;
                if (Progress >= 1.0f)
                {
                    Progress = 1.0f;
                    Active = false;
                    std::shared_ptr<ElementState> leaving = Leaving.lock();
                    Leaving.reset();
                    Call(leaving.get(), "OnHidden");
                }
            }

            bool DrawChildren(ElementState& element, DrawContext& context) override
            {
                std::shared_ptr<ElementState> leaving = Leaving.lock();
                ElementState* shown = Shown(element);
                if (!Active || !leaving || !shown || leaving.get() == shown)
                {
                    return false;
                }
                const Canvas& canvas = context.Canvas;
                float amount = Ease(Moving.Easing, Progress);
                Rectangle area = element.Frame;
                switch (Moving.Type)
                {
                case Transition::Kind::Cut: return false;
                case Transition::Kind::Fade:
                    // Each fades into the other, so displays without a background
                    // do not show through each other at full strength.
                    canvas.BeginLayer(area);
                    DrawElement(*leaving, context);
                    canvas.EndLayer({ .Opacity = 1.0f - amount });
                    canvas.BeginLayer(area);
                    DrawElement(*shown, context);
                    canvas.EndLayer({ .Opacity = amount });
                    return true;
                case Transition::Kind::Slide:
                {
                    Vector2 away;
                    switch (Moving.From)
                    {
                    case SlideFrom::Right: away = { -area.Width, 0.0f }; break;
                    case SlideFrom::Left: away = { area.Width, 0.0f }; break;
                    case SlideFrom::Bottom: away = { 0.0f, -area.Height }; break;
                    case SlideFrom::Top: away = { 0.0f, area.Height }; break;
                    }
                    canvas.PushTransform(away * amount);
                    DrawElement(*leaving, context);
                    canvas.PopTransform();
                    canvas.PushTransform(-away * (1.0f - amount));
                    DrawElement(*shown, context);
                    canvas.PopTransform();
                    return true;
                }
                case Transition::Kind::Scale:
                {
                    Vector2 center = area.Center();
                    float outgoing = 1.0f - 0.06f * amount;
                    canvas.PushTransform(center * (1.0f - outgoing), outgoing);
                    canvas.BeginLayer(area);
                    DrawElement(*leaving, context);
                    canvas.EndLayer({ .Opacity = 1.0f - amount });
                    canvas.PopTransform();
                    float incoming = 1.06f - 0.06f * amount;
                    canvas.PushTransform(center * (1.0f - incoming), incoming);
                    canvas.BeginLayer(area);
                    DrawElement(*shown, context);
                    canvas.EndLayer({ .Opacity = amount });
                    canvas.PopTransform();
                    return true;
                }
                case Transition::Kind::Shader:
                    DrawElement(*leaving, context);
                    canvas.BeginLayer(area);
                    DrawElement(*shown, context);
                    if (Moving.Effect)
                    {
                        canvas.EndLayer({ .Shader = Moving.Effect, .Values = { { "Progress", amount } } });
                    }
                    else
                    {
                        canvas.EndLayer({ .Opacity = amount });
                    }
                    return true;
                }
                return false;
            }

            bool Show(ElementState& element, std::string_view name, const ui::Transition& transition, bool remember)
            {
                ElementState* found = Named(element, name);
                if (!found)
                {
                    Log(LogLevel::Warning, "Displays::Show: there is no display named {}", name);
                    return false;
                }
                ElementState* shown = Shown(element);
                if (found == shown)
                {
                    return true;
                }
                // Held for the whole change: the callbacks below may remove either.
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                std::shared_ptr<ElementState> nextKept = found->shared_from_this();
                std::shared_ptr<ElementState> previousKept = shown ? shown->shared_from_this() : nullptr;
                ElementState* next = nextKept.get();
                ElementState* previous = previousKept.get();
                if (remember && previous)
                {
                    History.push_back({ previous->Row.Name(), transition });
                }
                if (RootState* root = element.Root())
                {
                    if (ElementState* focused = root->FocusedElement(); focused && previous && IsInside(focused, previous))
                    {
                        root->SetFocus(nullptr, false);
                    }
                }
                // A transition still running ends where it was going.
                if (std::shared_ptr<ElementState> stillLeaving = Leaving.lock(); Active && stillLeaving)
                {
                    Call(stillLeaving.get(), "OnHidden");
                }
                element.Set("Current", DataValue(name));
                Moving = transition;
                Progress = 0.0f;
                Active = transition.Type != Transition::Kind::Cut && transition.Seconds > 0.0f && previous;
                Leaving = previous ? previous->weak_from_this() : std::weak_ptr<ElementState>();
                Call(next, "OnShown");
                std::function<void(const std::string&)> changed =
                    element.Object<std::function<void(const std::string&)>>("OnChange");
                if (changed)
                {
                    changed(std::string(name));
                }
                if (!Active)
                {
                    Leaving.reset();
                    Call(previous, "OnHidden");
                }
                return true;
            }

            bool Back(ElementState& element)
            {
                if (History.empty())
                {
                    return false;
                }
                auto [name, transition] = History.back();
                History.pop_back();
                return Show(element, name, transition.Reversed(), false);
            }

            std::vector<std::pair<std::string, ui::Transition>> History;
            std::weak_ptr<ElementState> Leaving;
            ui::Transition Moving;
            float Progress = 1.0f;
            bool Active = false;
        };

        DisplaysBehavior* DisplaysOf(const std::shared_ptr<ElementState>& state)
        {
            return state ? dynamic_cast<DisplaysBehavior*>(state->Kind.get()) : nullptr;
        }
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Display::Display(std::string name, const ContainerSettings& settings)
        : Display(MakeElement(std::make_unique<DisplayBehavior>()))
    {
        ApplyContainerSettings(*State, settings);
        if (!name.empty())
        {
            State->Row.Rename(name);
        }
    }

    Display::Display(std::shared_ptr<internal::ElementState> state)
        : Container(std::move(state)), OnShown(Kept<std::function<void()>, "OnShown">(State)),
          OnHidden(Kept<std::function<void()>, "OnHidden">(State))
    {
    }

    Display::Display(const Display& other) : Display(other.State)
    {
    }

    Display& Display::operator=(const Display& other)
    {
        State = other.State;
        return *this;
    }

    Displays::Displays(const DisplaysSettings& settings) : Displays(MakeElement(std::make_unique<DisplaysBehavior>()))
    {
        ApplyCommonSettings(*State, settings);
        State->SetObject("Transition", settings.Transition);
        if (settings.OnChange)
        {
            State->SetObject("OnChange", settings.OnChange);
        }
        AddChildren(*State, settings.Children);
        std::string start = settings.Start;
        if (start.empty() && !State->Children.empty())
        {
            start = State->Children.front()->Row.Name();
        }
        State->Set("Current", DataValue(start));
        if (!settings.Start.empty() && !DisplaysOf(State)->Named(*State, settings.Start))
        {
            Log(LogLevel::Warning, "ui::Displays: there is no display named {} to start with", settings.Start);
        }
    }

    Displays::Displays(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)),
          Current(
              &State,
              [](const void* owner) -> std::string {
                  ElementState* element = StateOf(owner);
                  return element ? element->Get("Current").AsText() : std::string();
              },
              [](void* owner, const std::string& name) {
                  std::shared_ptr<ElementState>& element = SharedStateOf(owner);
                  if (DisplaysBehavior* displays = DisplaysOf(element))
                  {
                      displays->Show(*element, name, element->Object<ui::Transition>("Transition"), true);
                  }
              }),
          Transition(Kept<ui::Transition, "Transition">(State)),
          OnChange(Kept<std::function<void(const std::string&)>, "OnChange">(State))
    {
    }

    Displays::Displays(const Displays& other) : Displays(other.State)
    {
    }

    Displays& Displays::operator=(const Displays& other)
    {
        State = other.State;
        return *this;
    }

    bool Displays::Show(std::string_view name) const
    {
        DisplaysBehavior* displays = DisplaysOf(State);
        return displays && displays->Show(*State, name, State->Object<ui::Transition>("Transition"), true);
    }

    bool Displays::Show(std::string_view name, const ui::Transition& transition) const
    {
        DisplaysBehavior* displays = DisplaysOf(State);
        return displays && displays->Show(*State, name, transition, true);
    }

    bool Displays::Back() const
    {
        DisplaysBehavior* displays = DisplaysOf(State);
        return displays && displays->Back(*State);
    }

    bool Displays::CanGoBack() const
    {
        DisplaysBehavior* displays = DisplaysOf(State);
        return displays && !displays->History.empty();
    }
}
