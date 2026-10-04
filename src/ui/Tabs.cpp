#include <easyforge/ui/Displays.h>

#include <easyforge/core/Log.h>

#include "Behavior.h"
#include "Drawing.h"
#include "Properties.h"
#include "RootState.h"

namespace easyforge::ui::internal
{
    namespace
    {
        // Space at each side of a tab's text.
        constexpr float TabPadding = 14.0f;

        void Call(const std::shared_ptr<ElementState>& element, std::string_view callback)
        {
            if (!element)
            {
                return;
            }
            std::function<void()> function = element->Object<std::function<void()>>(callback);
            if (function)
            {
                function();
            }
        }

        class TabsBehavior final : public Behavior
        {
        public:
            std::string_view Name() const override { return Tabs::KindName; }
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

            // The pages that have tabs: the visible children.
            static std::vector<ElementState*> Pages(ElementState& element)
            {
                std::vector<ElementState*> pages;
                for (const std::shared_ptr<ElementState>& child : element.Children)
                {
                    if (child->CurrentStyle().Visible)
                    {
                        pages.push_back(child.get());
                    }
                }
                return pages;
            }

            // The page shown: the one named Current, or the first.
            static ElementState* Shown(ElementState& element)
            {
                std::vector<ElementState*> pages = Pages(element);
                std::string current = element.Get("Current").AsText();
                for (ElementState* page : pages)
                {
                    if (page->Row.Name() == current)
                    {
                        return page;
                    }
                }
                return pages.empty() ? nullptr : pages.front();
            }

            static float StripHeight(ElementState& element, const Context& context)
            {
                easyforge::Font font = FontOf(element, context);
                return (font ? font.LineHeight(FontSizeOf(element, context)) : 16.0f) + 16.0f;
            }

            // Where each page's tab is, left to right along the top of the frame.
            static std::vector<Rectangle> TabAreas(ElementState& element, const Context& context)
            {
                easyforge::Font font = FontOf(element, context);
                float size = FontSizeOf(element, context);
                float height = StripHeight(element, context);
                std::vector<Rectangle> areas;
                float x = element.Frame.X;
                for (ElementState* page : Pages(element))
                {
                    float width = (font ? font.Measure(page->Row.Name(), size).X : 0.0f) + TabPadding * 2.0f;
                    areas.push_back({ x, element.Frame.Y, width, height });
                    x += width;
                }
                return areas;
            }

            static std::optional<std::size_t> TabAt(ElementState& element, const Context& context, Vector2 point)
            {
                std::vector<Rectangle> areas = TabAreas(element, context);
                for (std::size_t index = 0; index < areas.size(); ++index)
                {
                    if (areas[index].Contains(point))
                    {
                        return index;
                    }
                }
                return std::nullopt;
            }

            Vector2 MeasureContent(ElementState& element, Context& context, const Constraint& constraint) override
            {
                float strip = StripHeight(element, context);
                Constraint below = constraint;
                below.Available.Y = Max(constraint.Available.Y - strip, 0.0f);
                Vector2 pages = MeasureLayers(element, context, below);
                float tabs = 0.0f;
                for (const Rectangle& area : TabAreas(element, context))
                {
                    tabs += area.Width;
                }
                return { Max(pages.X, tabs), pages.Y + strip };
            }

            void Arrange(ElementState& element, Context& context, Rectangle content) override
            {
                float strip = StripHeight(element, context);
                StripBottom = element.Frame.Y + strip;
                ArrangeLayers(element, context, { content.X, content.Y + strip, content.Width, Max(content.Height - strip, 0.0f) },
                    Alignment::Stretch);
            }

            void VisibleChildren(ElementState& element, std::vector<ElementState*>& into) override
            {
                if (ElementState* shown = Shown(element))
                {
                    into.push_back(shown);
                }
            }

            void Draw(ElementState& element, DrawContext& context) override
            {
                const Theme& theme = *context.Theme;
                const Canvas& canvas = context.Canvas;
                std::vector<ElementState*> pages = Pages(element);
                std::vector<Rectangle> areas = TabAreas(element, context);
                ElementState* shown = Shown(element);
                float strip = StripHeight(element, context);
                bool enabled = element.IsEnabled();
                easyforge::Font font = FontOf(element, context);
                float size = FontSizeOf(element, context);
                std::optional<Color> color = FromData<std::optional<Color>>(element.Get("Color"));

                canvas.Rectangle({ .Position = { element.Frame.X, element.Frame.Y + strip - 1.0f },
                    .Size = { element.Frame.Width, 1.0f }, .Color = theme.Border });
                canvas.PushClip({ element.Frame.X, element.Frame.Y, element.Frame.Width, strip });
                for (std::size_t index = 0; index < pages.size(); ++index)
                {
                    Rectangle area = areas[index];
                    bool current = pages[index] == shown;
                    if (enabled && !current && HoveredTab == index)
                    {
                        canvas.Rectangle({ .Position = { area.X + 2.0f, area.Y + 4.0f },
                            .Size = { area.Width - 4.0f, area.Height - 8.0f }, .Color = theme.ControlHovered,
                            .CornerRadius = 4.0f });
                    }
                    Color text = !enabled ? theme.DisabledText : current ? color.value_or(theme.Text) : theme.MutedText;
                    DrawTextIn(canvas, font, pages[index]->Row.Name(), size, text, area, TextAlignment::Center, true);
                    if (current)
                    {
                        canvas.Rectangle({ .Position = { area.X + 6.0f, area.Bottom() - 2.0f }, .Size = { area.Width - 12.0f, 2.0f },
                            .Color = enabled ? theme.Accent : theme.DisabledText, .CornerRadius = 1.0f });
                        if (element.Focused && context.FocusVisible)
                        {
                            DrawRing(canvas, { area.X + 3.0f, area.Y + 3.0f, area.Width - 6.0f, area.Height - 6.0f }, 4.0f, 0.0f,
                                2.0f, theme.Focus);
                        }
                    }
                }
                canvas.PopClip();
            }

            bool TakesPointer(ElementState&) const override { return true; }
            bool TakesPointerAt(ElementState&, Vector2 point) const override { return point.Y < StripBottom; }
            bool TakesKeyboard(ElementState& element) const override { return element.IsEnabled(); }
            bool ShowsFocusRing() const override { return false; }

            void PointerPressed(ElementState& element, Context& context, const Pointer& pointer) override
            {
                if (std::optional<std::size_t> tab = TabAt(element, context, pointer.Position))
                {
                    Show(element, Pages(element)[*tab]->Row.Name());
                }
            }

            void PointerMoved(ElementState& element, Context& context, const Pointer& pointer) override
            {
                HoveredTab = TabAt(element, context, pointer.Position);
            }

            void Update(ElementState& element, Context&) override
            {
                if (!element.Hovered)
                {
                    HoveredTab.reset();
                }
            }

            bool KeyPressed(ElementState& element, Context&, const Event& event) override
            {
                // Only while the tabs themselves have the keyboard, so arrows used
                // inside a page do not change the page.
                if (!element.Focused)
                {
                    return false;
                }
                std::vector<ElementState*> pages = Pages(element);
                if (pages.empty())
                {
                    return false;
                }
                std::size_t current = 0;
                for (std::size_t index = 0; index < pages.size(); ++index)
                {
                    if (pages[index] == Shown(element))
                    {
                        current = index;
                    }
                }
                std::size_t next = current;
                switch (event.Key)
                {
                case Key::Left: next = current > 0 ? current - 1 : 0; break;
                case Key::Right: next = Min(current + 1, pages.size() - 1); break;
                case Key::Home: next = 0; break;
                case Key::End: next = pages.size() - 1; break;
                default: return false;
                }
                Show(element, pages[next]->Row.Name());
                return true;
            }

            static bool Show(ElementState& element, std::string_view name)
            {
                std::shared_ptr<ElementState> next;
                for (const std::shared_ptr<ElementState>& child : element.Children)
                {
                    if (child->Row.Name() == name)
                    {
                        next = child;
                        break;
                    }
                }
                if (!next)
                {
                    Log(LogLevel::Warning, "Tabs: there is no page named {}", name);
                    return false;
                }
                ElementState* shown = Shown(element);
                if (shown == next.get())
                {
                    element.Set("Current", DataValue(name));
                    return true;
                }
                std::shared_ptr<ElementState> kept = element.shared_from_this();
                std::shared_ptr<ElementState> previous = shown ? shown->shared_from_this() : nullptr;

                // The keyboard leaves the page being hidden, for the tabs.
                if (RootState* root = element.Root())
                {
                    if (ElementState* focused = root->FocusedElement(); focused && previous && IsInside(focused, previous.get()))
                    {
                        root->SetFocus(element.IsEnabled() ? &element : nullptr, false);
                    }
                }
                element.Set("Current", DataValue(name));
                Call(next, "OnShown");
                Call(previous, "OnHidden");
                std::function<void(const std::string&)> changed =
                    element.Object<std::function<void(const std::string&)>>("OnChange");
                if (changed)
                {
                    changed(std::string(name));
                }
                return true;
            }

            std::optional<std::size_t> HoveredTab;
            float StripBottom = 0.0f;
        };
    }
}

namespace easyforge::ui
{
    using namespace internal;

    Tabs::Tabs(const TabsSettings& settings) : Tabs(MakeElement(std::make_unique<TabsBehavior>()))
    {
        const TabsSettings defaults {};
        ApplyCommonSettings(*State, settings);
        Choose(*State, "FontSize", settings.FontSize, defaults.FontSize);
        Choose(*State, "Color", settings.Color, defaults.Color);
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
        if (!settings.Start.empty() && !(TabsBehavior::Shown(*State) && TabsBehavior::Shown(*State)->Row.Name() == settings.Start))
        {
            Log(LogLevel::Warning, "ui::Tabs: there is no page named {} to start with", settings.Start);
        }
    }

    Tabs::Tabs(std::shared_ptr<internal::ElementState> state)
        : Element(std::move(state)),
          Current(
              &State,
              [](const void* owner) -> std::string {
                  ElementState* element = StateOf(owner);
                  if (!element)
                  {
                      return {};
                  }
                  ElementState* shown = TabsBehavior::Shown(*element);
                  return shown ? shown->Row.Name() : std::string();
              },
              [](void* owner, const std::string& name) {
                  if (ElementState* element = StateOf(owner))
                  {
                      TabsBehavior::Show(*element, name);
                  }
              }),
          FontSize(Stored<float, "FontSize">(State)), Color(Stored<std::optional<easyforge::Color>, "Color">(State)),
          OnChange(Kept<std::function<void(const std::string&)>, "OnChange">(State))
    {
    }

    Tabs::Tabs(const Tabs& other) : Tabs(other.State)
    {
    }

    Tabs& Tabs::operator=(const Tabs& other)
    {
        State = other.State;
        return *this;
    }
}
